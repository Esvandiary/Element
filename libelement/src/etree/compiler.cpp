#include "etree/compiler.hpp"

#include <cassert>
#include <utility>

#include <fmt/format.h>

#include "ast/ast_indexes.hpp"

//When compiling a function that needs direct input from the boundary, generate placeholder expressions to represent that input when it's evaluated
static std::vector<expression_shared_ptr> generate_placeholder_inputs(const element_type* t)
{
    std::vector<expression_shared_ptr> results;
    const size_t insize = t->inputs().size();
    results.reserve(insize);
    for (size_t i = 0; i < insize; ++i) {
        results.push_back(std::make_shared<element_expression_input>(i, t->inputs()[i].type->get_size()));
    }
    return results;
}

static expression_shared_ptr generate_intrinsic_expression(const element_intrinsic* fn, const std::vector<expression_shared_ptr>& args)
{
    //todo: logging rather than asserting?

    if (auto ui = fn->as<element_intrinsic_unary>()) {
        assert(args.size() >= 1);
        return std::make_shared<element_expression_unary>(ui->operation(), args[0]);
    }

    if (auto bi = fn->as<element_intrinsic_binary>()) {
        assert(args.size() >= 2);
        return std::make_shared<element_expression_binary>(bi->operation(), args[0], args[1]);
    }

    assert(false);
    return nullptr;
}

static element_result compile_intrinsic(
    element_compiler_ctx& ctx,
    const element_function* fn,
    std::vector<expression_shared_ptr> inputs,
    expression_shared_ptr& expr)
{
    if (const auto ui = fn->as<element_intrinsic_unary>()) {
        assert(inputs.size() >= 1);
        // TODO: better error codes
        //todo: logging
        if (inputs[0]->get_size() != 1) return ELEMENT_ERROR_ARGS_MISMATCH;
        expr = std::make_shared<element_expression_unary>(ui->operation(), inputs[0]);
        return ELEMENT_OK;
    }

    if (const auto bi = fn->as<element_intrinsic_binary>()) {
        assert(inputs.size() >= 2);
        // TODO: better error codes
        //todo: logging
        if (inputs[0]->get_size() != 1) return ELEMENT_ERROR_ARGS_MISMATCH;
        if (inputs[1]->get_size() != 1) return ELEMENT_ERROR_ARGS_MISMATCH;
        expr = std::make_shared<element_expression_binary>(bi->operation(), inputs[0], inputs[1]);
        return ELEMENT_OK;
    }

    // not implemented yet
    ctx.ictx.logger->log(ctx, ELEMENT_ERROR_NO_IMPL, fmt::format("Tried to compile intrinsic {} with no implementation.", fn->name()));
    assert(false);
    return ELEMENT_ERROR_NO_IMPL;
}

static element_result compile_type_ctor(
    element_compiler_ctx& ctx,
    const element_function* fn,
    std::vector<expression_shared_ptr> inputs,
    expression_shared_ptr& expr)
{
    assert(fn->inputs().size() >= inputs.size());

    // TODO: is flat list here OK?
    std::vector<std::pair<std::string, expression_shared_ptr>> deps;
    deps.reserve(inputs.size());
    for (size_t i = 0; i < inputs.size(); ++i)
        deps.emplace_back(fn->inputs()[i].name, inputs[i]);
    expr = std::make_shared<element_expression_structure>(std::move(deps));
    return ELEMENT_OK;
}

static element_result compile_expression(
    element_compiler_ctx& ctx,
    const element_scope* scope,
    const element_ast* bodynode,
    expression_shared_ptr& expr);

static element_result compile_custom_fn_scope(
    element_compiler_ctx& ctx,
    const element_scope* scope,
    std::vector<expression_shared_ptr> inputs,
    expression_shared_ptr& expr)
{
    const element_ast* node = scope->node;

    if (node->type != ELEMENT_AST_NODE_FUNCTION) {
        ctx.ictx.logger->log(ctx, ELEMENT_ERROR_INVALID_OPERATION, 
            fmt::format("Tried to compile custom function scope {} but it's not a function.", scope->name),
            node);
        return ELEMENT_ERROR_INVALID_OPERATION; // TODO: better error code
    }

    if (node->children.size() <= ast_idx::fn::body)
    {
        ctx.ictx.logger->log(ctx, ELEMENT_ERROR_INVALID_OPERATION, 
            fmt::format("Tried to compile custom function scope {} but it has no body.", scope->name),
            node);
        return ELEMENT_ERROR_INVALID_OPERATION; // TODO: better error code
    }

    //todo: understand what this chunk of code does, what it's caching, and when that cache will be used again
    assert(scope->function() && scope->function()->inputs().size() >= inputs.size());
    auto frame = ctx.expr_cache.add_frame();
    for (size_t i = 0; i < inputs.size(); ++i) {
        const element_scope* input_scope = scope->lookup(scope->function()->inputs()[i].name, false);
        ctx.expr_cache.add(input_scope, inputs[i]);
    }

    // find output
    // output is a function that's always present in the body of a function/lambda, representing what it returns
    const element_scope* output = scope->lookup("return", false);
    if (output)
        return compile_expression(ctx, output, output->node, expr);

    ctx.ictx.logger->log(ctx, ELEMENT_ERROR_INVALID_OPERATION,
        fmt::format("Tried to find return scope in function scope {} and failed.", scope->name),
        node);
    return ELEMENT_ERROR_INVALID_OPERATION;
}

//todo: understand what this does and document it
static element_result place_args(expression_shared_ptr& expr, const std::vector<expression_shared_ptr>& args)
{
    if (const auto ua = expr->as<element_expression_unbound_arg>()) {
        if (ua->index() < args.size()) {
            expr = args[ua->index()];
            return ELEMENT_OK;
        } else {
            return ELEMENT_ERROR_ARGS_MISMATCH; //logging is done by the caller
        }
    } else {
        for (auto& dep : expr->dependents()) {
            const auto result = place_args(dep, args);
            if (result != ELEMENT_OK)
                return result; //logging is done by the caller
        }
        return ELEMENT_OK;
    }
}

static element_result compile_call_experimental_literal(
    element_compiler_ctx& ctx,
    const element_scope* callsite_root,
    const element_ast* callsite_node,
    const element_scope*& callsite_current,
    expression_shared_ptr& expr);

static element_result compile_call_experimental(
    element_compiler_ctx& ctx,
    const element_scope* callsite_root,
    const element_ast* callsite_node,
    const element_scope*& callsite_current,
    expression_shared_ptr& expr); //note: might contain an expression

static element_result compile_call_experimental_function(
    element_compiler_ctx& ctx,
    const element_scope* callsite_root,
    const element_ast* callsite_node,
    const element_scope* parent_scope,
    const element_scope*& callsite_current,
    expression_shared_ptr& expr); //note: might contain an expression

static element_result compile_call_experimental_namespace(
    const element_ast* callsite_node,
    const element_scope* parent_scope,
    const expression_shared_ptr& expr);

static element_result compile_call_experimental_literal(
    element_compiler_ctx& ctx,
    const element_scope* callsite_root,
    const element_ast* callsite_node,
    const element_scope*& callsite_current,
    expression_shared_ptr& expr)
{
    if (callsite_node->type != ELEMENT_AST_NODE_LITERAL) {
        assert(false); //todo
        return ELEMENT_ERROR_UNKNOWN;
    }

    expr = std::make_shared<element_expression_constant>(callsite_node->literal);
    callsite_current = callsite_root->root()->lookup("Num", false); // HACK?
    return ELEMENT_OK;
}

static element_result compile_call_experimental_namespace(
    const element_ast* callsite_node,
    const element_scope* parent_scope,
    const expression_shared_ptr& expr)
{
    const bool has_parent = parent_scope;

    //todo; re-enable one james has fixed the lack of parentage on these reversed call nodes
    /*const bool has_child = callsite_node->parent
                        && callsite_node->parent->type == ELEMENT_AST_NODE_CALL;

    //Having a namespace that isn't being indexed is an error
    if (!has_child)
        return ELEMENT_ERROR_UNKNOWN; //todo*/

    //A namespace can index in to another namespace, but nothing else
    if (has_parent && parent_scope->node->type != ELEMENT_AST_NODE_NAMESPACE)
        return ELEMENT_ERROR_UNKNOWN; //todo

    //An expression implies something was compiled before getting to us, but that shouldn't be possible
    if (expr)
        return ELEMENT_ERROR_UNKNOWN; //todo

    //Our scope has already been updated, so our child will index in to us correctly
    return ELEMENT_OK;
}

static element_result fill_args_from_callsite(
    std::vector<expression_shared_ptr>& args,
    element_compiler_ctx& ctx,
    const element_scope* callsite_root,
    const element_ast* callsite_node)
{
    const bool calling_with_arguments = callsite_node->children.size() > ast_idx::call::args
        && callsite_node->children[ast_idx::call::args]->type == ELEMENT_AST_NODE_EXPRLIST;

    if (calling_with_arguments) {
        const auto callargs_node = callsite_node->children[ast_idx::call::args].get();
        args.resize(callargs_node->children.size());

        //Compile all of the exprlist AST nodes and assign them to the arguments we're calling with
        for (size_t i = 0; i < callargs_node->children.size(); ++i)
            ELEMENT_OK_OR_RETURN(compile_expression(ctx, callsite_root, callargs_node->children[i].get(), args[i]));
    }

    return ELEMENT_OK;
};

static element_result compile_call_experimental_function(
    element_compiler_ctx& ctx,
    const element_scope* callsite_root,
    const element_ast* callsite_node,
    const element_scope* parent_scope,
    const element_scope*& callsite_current, //todo: rename, it's the indexing scope (of our parent), or our scope, or the scope we're being called from
    expression_shared_ptr& expr)
{
    if (callsite_node->type != ELEMENT_AST_NODE_CALL) {
        assert(false); //todo
        return ELEMENT_ERROR_UNKNOWN;
    }

    const auto our_scope = callsite_current;

    //Handle any arguments to this function call
    std::vector<expression_shared_ptr> args;
    const auto result = fill_args_from_callsite(args, ctx, callsite_root, callsite_node);
    assert(result == ELEMENT_OK); //todo

    //todo: understand what this chunk of code does, what it's caching, and when that cache will be used again
    assert(args.empty() || (our_scope->function() && our_scope->function()->inputs().size() >= args.size()));
    auto frame = ctx.expr_cache.add_frame(); //frame is popped when it goes out of scope
    for (size_t i = 0; i < args.size(); ++i) {
        const element_scope* input_scope = our_scope->lookup(our_scope->function()->inputs()[i].name, false);
        ctx.expr_cache.add(input_scope, args[i]);
    }

    //todo: I believe this is seeing if this function was compiled previously when resolving the inputs to another function
    //todo: This doesn't update the fnscope if it's found, which seems to be part of the reason why indexing has issues

    const auto found_expr = ctx.expr_cache.search(our_scope);
    if (found_expr) {
        expr = found_expr;
        return ELEMENT_OK;
    }

    //Now we've compiled any and all of our parents, and we've compiled any and all of our arguments

    //If we had a parent, their compiled expression will be what's passed to us. The exception is namespace parents, but we want to ignore those here.
    const bool has_compiled_parent = expr.get();

    if (has_compiled_parent) {
        const auto compiled_parent = expr;

        /* Compiling our parent resulted in a struct instance, so we index that instance with our name.
          This is struct instance indexing.
          The callsite_scope will be invalid, as we did a lookup of ourselves based on the scope our parent set.
          Struct instances don't have a scope in libelement, as a scope is somewhere in the source code.
          Literals in source code are not struct instances. */
        if (compiled_parent->is<element_expression_structure>()) {
            expr = compiled_parent->as<element_expression_structure>()->output(callsite_node->identifier);

            if (expr) {
                //todo: understand what this does and document it
                const auto result = place_args(expr, args);
                //todo: We need to update the current callsite for the thing indexing us, otherwise once we index a struct instance we're stuck unable to index unless we ourselves compile to a struct instance
                return result;
            }

            //We failed to find ourselves in the struct instance.
            //todo: add fallback here if we can't find it in the struct instance, then we have to grab the parent scope as that is the struct declaration/body
            //We fall back out. Does this cause the fallback for finding ourselves in the struct body?
        }

        //Our parent didn't compile to a struct instance, or it did and we couldn't find ourselves as a member of it.
        //It was either a more complicated expression composed of intrinsics, a namespace, or a struct (e.g. Num.) ?

        const auto parent_as_function = parent_scope->function();

        //this does partial application of parent to arguments for this call. This only works when the parent is a constructor (including literals)
        //This does a bunch of stuff to get the type that the constructor returns, but that seems unecessary, as callsite_current that our parent modified should already point to that structs scope
        if (parent_as_function) {
            const auto parent_fn_type = parent_as_function->type(); //this doesn't work when it is a custom_function, which is good, yay
            const auto parent_fn_type_named = parent_fn_type ? parent_fn_type->as<element_type_named>() : nullptr;

            assert(parent_fn_type); //todo

            const element_scope* parent_fn_type_scope = parent_fn_type_named->scope();
            assert(parent_scope == parent_fn_type_scope); //debug. This is at least true when dealing with literals/Num
            assert(parent_fn_type_scope); //todo

            assert(callsite_current == parent_fn_type_scope->lookup(callsite_node->identifier, false)); //debug. //This is at least true when dealing with literals/Num, so this is all pointless for that case
            callsite_current = parent_fn_type_scope->lookup(callsite_node->identifier, false);
            assert(callsite_current); //todo

            // found a function in type's scope. 
            const auto fn = callsite_current->function();
            assert(fn); //todo, should be if, because if it's not a function then there's no partial application to do
            
            //if we're missing an argument to a method call while indexing, then pass the parent as the first argument
            const bool mising_one_argument = fn->inputs().size() == args.size() + 1;
            const bool argument_one_matches_parent_type = !fn->inputs().empty() && fn->inputs()[0].type->is_satisfied_by(parent_fn_type);
            if (mising_one_argument && argument_one_matches_parent_type) {
                args.insert(args.begin(), compiled_parent);
            }

            assert(fn->inputs().size() == args.size()); //todo
        }
    }

    // TODO: temporary check if intrinsic
    //todo: why is this temporary?
    const auto fn = callsite_current->function();
    if (fn && fn->is<element_intrinsic>()) {
        expr = generate_intrinsic_expression(fn->as<element_intrinsic>(), args);
        assert(expr); //todo

        //todo: we don't update the scope, so if our parent is an intrinsic when indexing, it fails
        callsite_current = nullptr; //we don't seem to be relying on this being valid right now, because no tests change as a result, but that might change in the future as other bugs are fixed
    }
    else if (fn && fn->is<element_type_ctor>()) {
        //todo: are the dependents always meant to be empty? should we not be calling compile_type_ctor?
        expr = std::shared_ptr<element_expression_structure>(new element_expression_structure({}));
        //todo: we don't update the scope, so the thing indexing in to us doesn't know what type this structure is
        //This seems to be valid in some cases with numbers(or literals only?). i.e if we set it to nullptr, all tests fail, so we're relying on something here
    }
    else if (fn && fn->is<element_custom_function>()) {
        //todo: understand this better
        ELEMENT_OK_OR_RETURN(compile_custom_fn_scope(ctx, callsite_current, args, expr));
        auto btype = fn->type();
        const auto type = btype ? btype->output("return")->type : nullptr;
        const auto ctype = type ? type->as<element_type_named>() : nullptr;
        if (ctype) {
            callsite_current = ctype->scope();
        }
        //assert(ctype); it seems like the lack of ctype here is okay, our scope is something we can rely on even though we're not updating it?
    }
    else {
        assert(false); //todo
    }

    return ELEMENT_OK;
}

static element_result compile_call_experimental_compile_parent(
    element_compiler_ctx& ctx,
    const element_scope* callsite_root,
    const element_ast* callsite_node,
    const element_scope*& callsite_current,
    expression_shared_ptr& expr)
{
    const auto our_scope = callsite_current;
    //NOTE {2}: This looks like it can be simplified (see NOTE {1})
    const bool has_parent = callsite_node->children.size() > ast_idx::call::parent && callsite_node->children[ast_idx::call::parent]->type != ELEMENT_AST_NODE_NONE;
    if (!has_parent)
        return ELEMENT_OK;

    //We're starting from the right-most call in the source and found out that we have a parent
    //Before we can start compiling this call, we need to find and compile our parent

    //Our parent could be anything (namespace, struct instance, number, number literal)
    //This will continue recursing until we're at the left-most call in the source (bottom of the AST)
    const auto callsite_node_parent = callsite_node->children[ast_idx::call::parent].get();
    const auto result = compile_call_experimental(ctx, callsite_root, callsite_node_parent, callsite_current, expr);
    assert(result == ELEMENT_OK); //todo
    assert(callsite_current != our_scope); //Our parent is done compiling, so it must update the scope we're indexing in to. This should happen for all situations.
    return result;
}

static element_result compile_call_experimental(
    element_compiler_ctx& ctx,
    const element_scope* callsite_root,
    const element_ast* callsite_node,
    const element_scope*& callsite_current,
    expression_shared_ptr& expr)
{
    if (callsite_node->type == ELEMENT_AST_NODE_LITERAL)
        return compile_call_experimental_literal(ctx, callsite_root, callsite_node, callsite_current, expr);

    if (callsite_node->type != ELEMENT_AST_NODE_CALL)
        return ELEMENT_ERROR_UNKNOWN;

    const auto original_scope = callsite_current;
    const bool has_parent = callsite_node->children.size() > ast_idx::call::parent
                         && callsite_node->children[ast_idx::call::parent]->type != ELEMENT_AST_NODE_NONE;
    if (has_parent) {
        const auto result = compile_call_experimental_compile_parent(ctx, callsite_root, callsite_node, callsite_current, expr);
        assert(result == ELEMENT_OK);
    }
    const auto parent_scope = has_parent ? callsite_current : nullptr;

    //We've now compiled any parents (if we had any parents), so let's find what this call was meant to be
    //If we did have a parent then we don't want to recurse when doing the lookup, what we're looking for should be directly in that scope
    //todo: For parents, this only works when the parent has updated its scope to be a valid index target, as we don't handle the return of a function call atm
    assert(callsite_current); //todo
    callsite_current = callsite_current->lookup(callsite_node->identifier, !has_parent);
    const auto our_scope = callsite_current;
    assert(callsite_current); //todo

    if (our_scope->function())
        return compile_call_experimental_function(ctx, callsite_root, callsite_node, parent_scope, callsite_current, expr);

    if (our_scope->node->type == ELEMENT_AST_NODE_NAMESPACE)
        return compile_call_experimental_namespace(callsite_node, parent_scope, expr);

    //This node we're compiling came from a port, so its expression should be cached from the function that called the function this port is for
    if (our_scope->node->type == ELEMENT_AST_NODE_PORT) {
        expr = ctx.expr_cache.search(our_scope);
        //todo: we should update the current scope so we can do indexing with this expression
        callsite_current = nullptr; //we're not relying on this right now because no tests fail as a result, but may not be the case when we fix other bugs
        if (expr)
            return ELEMENT_OK;
    }

    //Wasn't something we know about
    expr = nullptr; //probably unecessary
    callsite_current = nullptr;
    assert(false);
    return ELEMENT_ERROR_UNKNOWN; //todo
}

static element_result compile_call(
    element_compiler_ctx& ctx,
    const element_scope* scope,
    const element_ast* bodynode,
    const element_scope*& fnscope,
    expression_shared_ptr& expr)
{
    if (bodynode->type == ELEMENT_AST_NODE_LITERAL) {
        expr = std::make_shared<element_expression_constant>(bodynode->literal);
        fnscope = scope->root()->lookup("Num", false); // HACK?
        return ELEMENT_OK;
    }

    std::vector<expression_shared_ptr> args;

    // scope is the current scope the outer call is happening in
    // fnscope tracks the current available scope of the nested call

    const element_scope* orig_fnscope = fnscope;
	//NOTE {2}: This looks like it can be simplified (see NOTE {1})
    const bool has_parent = bodynode->children.size() > ast_idx::call::parent && bodynode->children[ast_idx::call::parent]->type != ELEMENT_AST_NODE_NONE;
    // compound identifier with "parent" - could either be member access or method call
    expression_shared_ptr parent;
    if (has_parent) {
        const auto parent_node = bodynode->children[ast_idx::call::parent].get();
        assert(parent_node->type == ELEMENT_AST_NODE_CALL || parent_node->type == ELEMENT_AST_NODE_LITERAL);

        ELEMENT_OK_OR_RETURN(compile_call(ctx, scope, parent_node, fnscope, parent));

        if (!parent) {
            //todo: not sure message is correct for any of these, but better than nothing right now. Fix as issues are found
            ctx.ictx.logger->log(ctx, ELEMENT_ERROR_INVALID_OPERATION,
                fmt::format("Failed to get expression for parent {} while indexing with {}",
                    bodynode->children[ast_idx::call::parent]->identifier, bodynode->identifier),
                bodynode);
            return ELEMENT_ERROR_INVALID_OPERATION; // TODO: better error code
        }
    }

    //The function scope was (probably) modified while compiling the call to the parent. Keep track of it
    const element_scope* parent_fnscope = fnscope;

    if (!fnscope) {
        ctx.ictx.logger->log(ctx, ELEMENT_ERROR_INVALID_OPERATION,
            fmt::format("Was going to find {}{} while indexing {} but failed. Started at {}.",
                bodynode->identifier, !has_parent ? " recursively" : "", orig_fnscope->name, scope->name),
            bodynode);
        return ELEMENT_ERROR_INVALID_OPERATION; // TODO: better error code
    }

    fnscope = fnscope->lookup(bodynode->identifier, !has_parent);

    if (!fnscope) {
        ctx.ictx.logger->log(ctx, ELEMENT_ERROR_INVALID_OPERATION,
            fmt::format("Tried to find {} when indexing {}{} within {} but failed.",
                 bodynode->identifier, parent_fnscope->name, !has_parent ? " recursively" : "", scope->parent ? scope->parent->name : scope->name),
            bodynode);

        return ELEMENT_ERROR_INVALID_OPERATION; // TODO: better error code
    }

    // TODO: check if we're doing partial application

    if (bodynode->children.size() > ast_idx::call::args &&
        bodynode->children[ast_idx::call::args]->type == ELEMENT_AST_NODE_EXPRLIST) {
        // call with args
        const element_ast* callargs = bodynode->children[ast_idx::call::args].get();
        args.resize(callargs->children.size());
        for (size_t i = 0; i < callargs->children.size(); ++i)
            ELEMENT_OK_OR_RETURN(compile_expression(ctx, scope, callargs->children[i].get(), args[i]));
    }

    //todo: understand what this chunk of code does, what it's caching, and when that cache will be used again
    auto argument_count_match = args.empty() || (fnscope->function() && fnscope->function()->inputs().size() >= args.size());
    if(!argument_count_match)
        return ELEMENT_ERROR_ARGUMENT_COUNT_MISMATCH;
    //assert(args.empty() || (fnscope->function() && fnscope->function()->inputs().size() >= args.size()));
    auto frame = ctx.expr_cache.add_frame();
    for (size_t i = 0; i < args.size(); ++i) {
        const element_scope* input_scope = fnscope->lookup(fnscope->function()->inputs()[i].name, false);
        ctx.expr_cache.add(input_scope, args[i]);
    }

    //todo: I believe this is seeing if this function was compiled previously when resolving the inputs to another function
    //todo: This doesn't update the fnscope if it's found, which seems to be part of the reason why indexing has issues
    expr = ctx.expr_cache.search(fnscope);
    if (!expr) {
        // see if we need to redirect (e.g. method call)
        if (has_parent) {
            if (parent->is<element_expression_structure>())
                expr = parent->as<element_expression_structure>()->output(bodynode->identifier);

            if (expr) {
                const auto result = place_args(expr, args);

                if (!result) {
                    ctx.ictx.logger->log(ctx, ELEMENT_ERROR_INVALID_OPERATION,
                        fmt::format("Failed to place arguments in output expression of an element_expression_structure. scope {}", scope->name),
                        bodynode);
                    return result;
                }

                // TODO: more here?
                return ELEMENT_OK;
            }
            // no member found - method access?
            //todo: for constructors this type is the type it's constructing, but for other functions it's the function constraint
            const auto type = parent_fnscope->function()->type();
            const auto ctype = type ? type->as<element_type_named>() : nullptr;

            if (!ctype) {
                ctx.ictx.logger->log(ctx, ELEMENT_ERROR_INVALID_OPERATION,
                    fmt::format("Return type of {} could not be found", parent_fnscope->name),
                    bodynode);
                return ELEMENT_ERROR_INVALID_OPERATION;
            }

            const element_scope* tscope = ctype->scope();
            fnscope = tscope->lookup(bodynode->identifier, false);

            if (!fnscope) {
                ctx.ictx.logger->log(ctx, ELEMENT_ERROR_INVALID_OPERATION,
                    fmt::format("Unable to find method {} in type {}", bodynode->identifier, ctype->name()),
                    bodynode);
                return ELEMENT_ERROR_INVALID_OPERATION;
            }

            // found a function in type's scope
            const auto fn = fnscope->function();

            //if we're missing an argument to a method call while indexing, then pass the parent as the first argument
            if (fn->inputs().size() == args.size() + 1 && fn->inputs()[0].type->is_satisfied_by(type)) {
                args.insert(args.begin(), parent);
            }

            if (fn->inputs().size() != args.size()) {
                ctx.ictx.logger->log(ctx, ELEMENT_ERROR_INVALID_OPERATION,
                    fmt::format("Function input count doesn't match argument count in function {} in type {} for scope {}", fn->name(), ctype->name(), scope->name),
                    bodynode);
                return ELEMENT_ERROR_INVALID_OPERATION;
            }
        }

        // TODO: temporary check if intrinsic
        //todo: why is this temporary?
        if (fnscope->function() && fnscope->function()->is<element_intrinsic>()) {
            expr = generate_intrinsic_expression(fnscope->function()->as<element_intrinsic>(), args);

            if (!expr) {
                ctx.ictx.logger->log(ctx, ELEMENT_ERROR_INVALID_OPERATION,
                    fmt::format("Function {} is intrinsic but failed to generate intrinsic expression.", fnscope->name),
                    bodynode);
                return ELEMENT_ERROR_INVALID_OPERATION;
            }

            //todo: we don't update the fnscope, so if our parent is an intrinsic when indexing, it fails
        }
        else if (fnscope->function() && fnscope->function()->is<element_type_ctor>()) {
            //todo: are the dependents always meant to be empty? should we not be calling compile_type_ctor?
            expr = std::shared_ptr<element_expression_structure>(new element_expression_structure({}));
        }
        else {
            ELEMENT_OK_OR_RETURN(compile_custom_fn_scope(ctx, fnscope, args, expr));
            auto btype = fnscope->function()->type();
            const auto type = btype ? btype->output("return")->type : nullptr;
            const auto ctype = type ? type->as<element_type_named>() : nullptr;
            if (ctype) {
                fnscope = ctype->scope();
            }
        }
    }
    return ELEMENT_OK;
}

static element_result compile_lambda(
    element_compiler_ctx& ctx,
    const element_scope* scope,
    const element_ast* bodynode,
    expression_shared_ptr& expr)
{
    // TODO: this
    ctx.ictx.logger->log(ctx, ELEMENT_ERROR_NO_IMPL,
        fmt::format("Tried to compile lambda {}. Lambdas are not implemented in the compiler.", scope->name),
        bodynode);
    return ELEMENT_ERROR_NO_IMPL;
}

static element_result compile_expression(
    element_compiler_ctx& ctx,
    const element_scope* scope,
    const element_ast* bodynode,
    expression_shared_ptr& expr)
{
    // do we have a body?
    if (bodynode->type == ELEMENT_AST_NODE_CALL || bodynode->type == ELEMENT_AST_NODE_LITERAL) {
        // literal or non-constant expression
        return compile_call_experimental(ctx, scope, bodynode, scope, expr);
    } else if (bodynode->type == ELEMENT_AST_NODE_LAMBDA) {
        // lambda
        return compile_lambda(ctx, scope, bodynode, expr);
    } else if (bodynode->type == ELEMENT_AST_NODE_SCOPE) {
        // function in current scope
        // generate inputs
        auto inputs = generate_placeholder_inputs(scope->function()->type().get());
        // now compile function using those inputs
        return compile_custom_fn_scope(ctx, scope, std::move(inputs), expr);
    } else {
        // interface
        ctx.ictx.logger->log(ctx, ELEMENT_ERROR_NO_IMPL, "Tried to compile an expression with no implementation.", bodynode);
        return ELEMENT_ERROR_NO_IMPL;  // TODO: better error code
    }
}

static element_result compile_custom_fn(
    element_compiler_ctx& ctx,
    const element_function* fn,
    std::vector<expression_shared_ptr> inputs,
    expression_shared_ptr& expr)
{
    const auto cfn = fn->as<element_custom_function>();
    const element_scope* scope = cfn->scope();
    return compile_custom_fn_scope(ctx, scope, std::move(inputs), expr);
}

static element_result element_compile(
    element_interpreter_ctx& ctx,
    const element_function* fn,
    std::vector<expression_shared_ptr> inputs,
    expression_shared_ptr& expr,
    element_compiler_options opts)
{
    element_compiler_ctx cctx = { ctx, std::move(opts) };
    if (fn->is<element_intrinsic>()) {
        return compile_intrinsic(cctx, fn, std::move(inputs), expr);
    } else if (fn->is<element_type_ctor>()) {
        return compile_type_ctor(cctx, fn, std::move(inputs), expr);
    } else if (fn->is<element_custom_function>()) {
        return compile_custom_fn(cctx, fn, std::move(inputs), expr);
    } else {
        assert(false);
        //todo: logging
        return ELEMENT_ERROR_INVALID_OPERATION; // TODO: better error code
    }
}

element_result element_compile(
    element_interpreter_ctx& ctx,
    const element_function* fn,
    expression_shared_ptr& expr,
    element_compiler_options opts)
{
    auto inputs = generate_placeholder_inputs(fn->type().get());
    return element_compile(ctx, fn, std::move(inputs), expr, std::move(opts));
}
