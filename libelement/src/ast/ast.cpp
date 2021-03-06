#include "element/ast.h"
#include "element/token.h"

#include <vector>
#include <string>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <cassert>
#include <memory>
#include <unordered_set>

#include "ast/ast_internal.hpp"
#include "ast/ast_indexes.hpp"
#include "token_internal.hpp"
#include "MemoryPool.h"

static std::unordered_set<std::string> qualifiers {"intrinsic", "extern", "static"};
static std::unordered_set<std::string> constructs {"struct", "namespace"};
static std::unordered_set<std::string> reserved_args {};

static element_result check_reserved_words(const std::string& text, bool allow_reserved_arg)
{
    return (qualifiers.count(text) == 0 && constructs.count(text) == 0 && (allow_reserved_arg || reserved_args.count(text) == 0))
        ? ELEMENT_OK
        : ELEMENT_ERROR_INVALID_ARCHIVE; // TODO: errcode
}

//
// Token helpers
//
#define GET_TOKEN(tctx, tindex, tok) ELEMENT_OK_OR_RETURN(element_tokeniser_get_token((tctx), (tindex), &(tok)))
#define GET_TOKEN_COUNT(tctx, tcount) ELEMENT_OK_OR_RETURN(element_tokeniser_get_token_count((tctx), &(tcount)))

static int tokenlist_advance(element_tokeniser_ctx* tctx, size_t* tindex)
{
    size_t tcount;
    ELEMENT_OK_OR_RETURN(element_tokeniser_get_token_count(tctx, &tcount));

    ++(*tindex);
    // TODO: do something with these, we might need them later to preserve formatting...
    const element_token* tok;
    GET_TOKEN(tctx, *tindex, tok);
    while (*tindex < tcount && tok->type == ELEMENT_TOK_NONE) {
        ++(*tindex);
        GET_TOKEN(tctx, *tindex, tok);
    }
    return (*tindex < tcount);
}

#define TOKENLIST_ADVANCE_AND_UPDATE(tctx, tindex, tok) \
{ tokenlist_advance((tctx), (tindex)); ELEMENT_OK_OR_RETURN(element_tokeniser_get_token((tctx), *(tindex), &(tok))); }

//
// AST memory pool
//
static MemoryPool<element_ast> ast_pool;

static void delete_ast_unique_ptr(element_ast* p)
{
    ast_pool.deleteElement(p);
}

static ast_unique_ptr ast_new(element_ast* parent, element_ast_node_type type = ELEMENT_AST_NODE_NONE)
{
    // return std::make_unique<element_ast>(parent);
    element_ast* node = ast_pool.newElement(parent);
    node->type = type;
    return ast_unique_ptr(node, delete_ast_unique_ptr);
}


element_result element_ast_get_type(const element_ast* node, element_ast_node_type* type)
{
    assert(node);
    assert(type);
    *type = node->type;
    return ELEMENT_OK;
}

element_result element_ast_get_value_as_identifier(const element_ast* node, const char** value)
{
    assert(node);
    assert(value);
    if (!ast_node_has_identifier(node))
        return ELEMENT_ERROR_INVALID_OPERATION;
    *value = node->identifier.c_str();
    return ELEMENT_OK;
}

element_result element_ast_get_value_as_literal(const element_ast* node, element_value* value)
{
    assert(node);
    assert(value);
    if (!ast_node_has_literal(node))
        return ELEMENT_ERROR_INVALID_OPERATION;
    *value = node->literal;
    return ELEMENT_OK;
}

element_result element_ast_get_parent(const element_ast* ast, element_ast** parent)
{
    assert(ast);
    assert(parent);
    *parent = ast->parent;
    return ELEMENT_OK;
}

element_result element_ast_get_child_count(const element_ast* ast, size_t* count)
{
    assert(ast);
    assert(count);
    *count = ast->children.size();
    return ELEMENT_OK;
}

element_result element_ast_get_child(const element_ast* ast, const size_t index, element_ast** child)
{
    assert(ast);
    assert(child);
    if (index >= ast->children.size())
        return ELEMENT_ERROR_INVALID_OPERATION;
    *child = ast->children[index].get();
    return ELEMENT_OK;
}


static void ast_clear(element_ast* n)
{
    assert(n);
    n->children.clear();
}

static void ast_move(element_ast* from, element_ast* to, bool reparent)
{
    assert(from != to);
    element_ast* new_parent = reparent ? from->parent : to->parent;
    ast_clear(to);

    *to = std::move(*from);
    to->parent = new_parent;
}

static element_ast* ast_add_child(element_ast* parent, ast_unique_ptr child)
{
    assert(parent);
    assert(child);
    assert(child->parent == parent || child->parent == NULL);
    child->parent = parent;
    element_ast* childr = child.get();
    parent->children.push_back(std::move(child));
    return childr;
}

static element_ast* ast_new_child(element_ast* parent, element_ast_node_type type = ELEMENT_AST_NODE_NONE)
{
    auto c = ast_new(parent, type);
    element_ast* cr = c.get();
    ast_add_child(parent, std::move(c));
    return cr;
}

// literal ::= [-+]? [0-9]+ ('.' [0-9]*)? ([eE] [-+]? [0-9]+)?
static element_result parse_literal(element_tokeniser_ctx* tctx, size_t* tindex, element_ast* ast)
{
    const element_token* token;
    GET_TOKEN(tctx, *tindex, token);
    assert(token->type == ELEMENT_TOK_NUMBER);
    ast->type = ELEMENT_AST_NODE_LITERAL;
    ast->literal = std::stof(tctx->text(token));
    tokenlist_advance(tctx, tindex);
    return ELEMENT_OK;
}

// identifier ::= '_'? [a-zA-Z#x00F0-#xFFFF] [_a-zA-Z0-9#x00F0-#xFFFF]*
static element_result parse_identifier(element_tokeniser_ctx* tctx, size_t* tindex, element_ast* ast, bool allow_reserved_args = false)
{
    const element_token* token;
    GET_TOKEN(tctx, *tindex, token);
    assert(token->type == ELEMENT_TOK_IDENTIFIER);
    ast->identifier.assign(tctx->text(token));
    tokenlist_advance(tctx, tindex);
    return check_reserved_words(ast->identifier, allow_reserved_args);
}

// type ::= ':' identifier ('.' identifier)*
static element_result parse_typename(element_tokeniser_ctx* tctx, size_t* tindex, element_ast* ast)
{
    const element_token* tok;
    GET_TOKEN(tctx, *tindex, tok);
    assert(tok->type == ELEMENT_TOK_IDENTIFIER);
    ast->type = ELEMENT_AST_NODE_TYPENAME;
    while (tok->type == ELEMENT_TOK_IDENTIFIER) {
        element_ast* child = ast_new_child(ast);
        child->type = ELEMENT_AST_NODE_IDENTIFIER;
        ELEMENT_OK_OR_RETURN(parse_identifier(tctx, tindex, child));
        GET_TOKEN(tctx, *tindex, tok);
        if (tok->type == ELEMENT_TOK_DOT)
            TOKENLIST_ADVANCE_AND_UPDATE(tctx, tindex, tok);
    }
    return ELEMENT_OK;
}

// port ::= (identifier | unidentifier) type?
static element_result parse_port(element_tokeniser_ctx* tctx, size_t* tindex, element_ast* ast)
{
    const element_token* tok;
    GET_TOKEN(tctx, *tindex, tok);
    assert(tok->type == ELEMENT_TOK_IDENTIFIER || tok->type == ELEMENT_TOK_UNDERSCORE);
    ast->type = ELEMENT_AST_NODE_PORT;
    if (tok->type == ELEMENT_TOK_IDENTIFIER) {
        ELEMENT_OK_OR_RETURN(parse_identifier(tctx, tindex, ast, true));
    }

    GET_TOKEN(tctx, *tindex, tok);
    if (tok->type == ELEMENT_TOK_COLON) {
        tokenlist_advance(tctx, tindex);
        element_ast* type = ast_new_child(ast);
        ELEMENT_OK_OR_RETURN(parse_typename(tctx, tindex, type));
    }
    return ELEMENT_OK;
}

// portlist ::= port (',' port)*
static element_result parse_portlist(element_tokeniser_ctx* tctx, size_t* tindex, element_ast* ast)
{
    const element_token* tok;
    GET_TOKEN(tctx, *tindex, tok);
    assert(tok->type == ELEMENT_TOK_IDENTIFIER);
    ast->type = ELEMENT_AST_NODE_PORTLIST;
    do
    {
        element_ast* port = ast_new_child(ast);
        ELEMENT_OK_OR_RETURN(parse_port(tctx, tindex, port));
        GET_TOKEN(tctx, *tindex, tok);
    } while (tok->type == ELEMENT_TOK_COMMA && tokenlist_advance(tctx, tindex));
    return ELEMENT_OK;
}

static element_result parse_expression(element_tokeniser_ctx* tctx, size_t* tindex, element_ast* ast);
static element_result parse_body(element_tokeniser_ctx* tctx, size_t* tindex, element_ast* ast);

// exprlist ::= expression (',' expression)*
static element_result parse_exprlist(element_tokeniser_ctx* tctx, size_t* tindex, element_ast* ast)
{
    const element_token* token;
    GET_TOKEN(tctx, *tindex, token);
    assert(token->type == ELEMENT_TOK_BRACKETL);
    TOKENLIST_ADVANCE_AND_UPDATE(tctx, tindex, token);
    ast->type = ELEMENT_AST_NODE_EXPRLIST;
    if (token->type != ELEMENT_TOK_BRACKETR) {
        do {
            element_ast* eid = ast_new_child(ast);
            ELEMENT_OK_OR_RETURN(parse_expression(tctx, tindex, eid));
            GET_TOKEN(tctx, *tindex, token);
        } while (token->type == ELEMENT_TOK_COMMA && tokenlist_advance(tctx, tindex));
    }
    assert(token->type == ELEMENT_TOK_BRACKETR);
    tokenlist_advance(tctx, tindex);
    return ELEMENT_OK;
}

// call ::= (identifier | literal) ('(' exprlist ')' | '.' identifier)*
static element_result parse_call(element_tokeniser_ctx* tctx, size_t* tindex, element_ast* ast)
{
    const element_token* token;
    GET_TOKEN(tctx, *tindex, token);
    assert(token->type == ELEMENT_TOK_IDENTIFIER || token->type == ELEMENT_TOK_NUMBER);
    // get identifier
    ast_unique_ptr root = ast_new(nullptr, ELEMENT_AST_NODE_CALL);
    if (token->type == ELEMENT_TOK_IDENTIFIER) {
        ELEMENT_OK_OR_RETURN(parse_identifier(tctx, tindex, root.get()));
    } else if (token->type == ELEMENT_TOK_NUMBER) {
        // this will change root's type to LITERAL
        ELEMENT_OK_OR_RETURN(parse_literal(tctx, tindex, root.get()));
    } else {
        assert(false);
        return ELEMENT_ERROR_INVALID_ARCHIVE; // TODO: error code
    }
    GET_TOKEN(tctx, *tindex, token);

    while (token->type == ELEMENT_TOK_DOT || token->type == ELEMENT_TOK_BRACKETL) {
        if (token->type == ELEMENT_TOK_BRACKETL) {
            // call with args
            // TODO: bomb out if we're trying to call a literal
            // add blank "none" if this is a simple call
            if (root->children.empty())
                ast_add_child(root.get(), ast_new(root.get(), ELEMENT_AST_NODE_NONE));
            // parse args
            element_ast* cid = ast_new_child(root.get());
            ELEMENT_OK_OR_RETURN(parse_exprlist(tctx, tindex, cid));
        } else if (token->type == ELEMENT_TOK_DOT) {
            // member field access
            ast_unique_ptr callroot = ast_new(nullptr, ELEMENT_AST_NODE_CALL);
            // move existing root into left side of new one
            ast_add_child(callroot.get(), std::move(root));
            tokenlist_advance(tctx, tindex);
            ELEMENT_OK_OR_RETURN(parse_identifier(tctx, tindex, callroot.get()));
            root = std::move(callroot);
        }
        GET_TOKEN(tctx, *tindex, token);
    }
    ast_move(root.get(), ast, 0);
    return ELEMENT_OK;
}

// lambda ::= unidentifier '(' portlist ')' body
static element_result parse_lambda(element_tokeniser_ctx* tctx, size_t* tindex, element_ast* ast)
{
    const element_token* token;
    GET_TOKEN(tctx, *tindex, token);
    assert(token->type == ELEMENT_TOK_UNDERSCORE);
    ast->type = ELEMENT_AST_NODE_LAMBDA;

    TOKENLIST_ADVANCE_AND_UPDATE(tctx, tindex, token);
    if (token->type != ELEMENT_TOK_BRACKETL)
        return ELEMENT_ERROR_INVALID_OPERATION;
    tokenlist_advance(tctx, tindex);
    element_ast* ports = ast_new_child(ast);
    ELEMENT_OK_OR_RETURN(parse_portlist(tctx, tindex, ports));
    GET_TOKEN(tctx, *tindex, token);
    if (token->type != ELEMENT_TOK_BRACKETR)
        return ELEMENT_ERROR_INVALID_OPERATION;
    tokenlist_advance(tctx, tindex);
    element_ast* body = ast_new_child(ast);
    ELEMENT_OK_OR_RETURN(parse_body(tctx, tindex, body));
    return ELEMENT_OK;
}

// expression ::= call | lambda
static element_result parse_expression(element_tokeniser_ctx* tctx, size_t* tindex, element_ast* ast)
{
    const element_token* token;
    GET_TOKEN(tctx, *tindex, token);
    assert(token->type == ELEMENT_TOK_IDENTIFIER || token->type == ELEMENT_TOK_UNDERSCORE || token->type == ELEMENT_TOK_NUMBER);
    if (token->type == ELEMENT_TOK_IDENTIFIER || token->type == ELEMENT_TOK_NUMBER)
        return parse_call(tctx, tindex, ast);
    if (token->type == ELEMENT_TOK_UNDERSCORE)
        return parse_lambda(tctx, tindex, ast);
    return ELEMENT_ERROR_INVALID_OPERATION; // TODO: better error code...
}

// qualifier ::= 'intrinsic' | 'extern'
static element_result parse_qualifiers(element_tokeniser_ctx* tctx, size_t* tindex, element_ast_flags* flags)
{
    const element_token* tok;
    GET_TOKEN(tctx, *tindex, tok);

    while (tok->type == ELEMENT_TOK_IDENTIFIER) {
        std::string id = tctx->text(tok);
        if (id == "intrinsic")
            *flags |= ELEMENT_AST_FLAG_DECL_INTRINSIC;
        else if (id == "extern")
            *flags |= ELEMENT_AST_FLAG_DECL_EXTERN;
        else
            break;
        
        TOKENLIST_ADVANCE_AND_UPDATE(tctx, tindex, tok);
    }

    return ELEMENT_OK;
}

// declaration ::= identifier ('(' portlist ')')?
// note that we also grab an optional type on the end at AST level for simplicity
static element_result parse_declaration(element_tokeniser_ctx* tctx, size_t* tindex, element_ast* ast, bool find_return_type)
{
    const element_token* tok;
    GET_TOKEN(tctx, *tindex, tok);
    assert(tok->type == ELEMENT_TOK_IDENTIFIER);
    ast->type = ELEMENT_AST_NODE_DECLARATION;
    ELEMENT_OK_OR_RETURN(parse_identifier(tctx, tindex, ast));
    GET_TOKEN(tctx, *tindex, tok);
    // always create the args node, even if it ends up being none/empty
    element_ast* args = ast_new_child(ast, ELEMENT_AST_NODE_NONE);
    if (tok->type == ELEMENT_TOK_BRACKETL) {
        // argument list
        tokenlist_advance(tctx, tindex);
        ELEMENT_OK_OR_RETURN(parse_portlist(tctx, tindex, args));
        GET_TOKEN(tctx, *tindex, tok);
        assert(tok->type == ELEMENT_TOK_BRACKETR);
        TOKENLIST_ADVANCE_AND_UPDATE(tctx, tindex, tok);
    }
    if (find_return_type && tok->type == ELEMENT_TOK_COLON) {
        // output type
        tokenlist_advance(tctx, tindex);
        element_ast* type = ast_new_child(ast);
        ELEMENT_OK_OR_RETURN(parse_typename(tctx, tindex, type));
    } else {
        // implied any output
        ast_new_child(ast, ELEMENT_AST_NODE_NONE);
    }
    return ELEMENT_OK;
}

static element_result parse_item(element_tokeniser_ctx* tctx, size_t* tindex, element_ast* ast);

// scope ::= '{' item* '}'
static element_result parse_scope(element_tokeniser_ctx* tctx, size_t* tindex, element_ast* ast)
{
    const element_token* token;
    GET_TOKEN(tctx, *tindex, token);
    if (token->type == ELEMENT_TOK_BRACEL)
        TOKENLIST_ADVANCE_AND_UPDATE(tctx, tindex, token);

    ast->type = ELEMENT_AST_NODE_SCOPE;
    while (token->type != ELEMENT_TOK_BRACER) {
        element_ast* item = ast_new_child(ast);
        ELEMENT_OK_OR_RETURN(parse_item(tctx, tindex, item));
        GET_TOKEN(tctx, *tindex, token);
    }
    tokenlist_advance(tctx, tindex);
    return ELEMENT_OK;
}

static element_result parse_body(element_tokeniser_ctx* tctx, size_t* tindex, element_ast* ast)
{
    const element_token* token;
    GET_TOKEN(tctx, *tindex, token);
    if (token->type == ELEMENT_TOK_BRACEL) {
        // scope (function body)
        ELEMENT_OK_OR_RETURN(parse_scope(tctx, tindex, ast));
    } else if (token->type == ELEMENT_TOK_EQUALS) {
        tokenlist_advance(tctx, tindex);
        ELEMENT_OK_OR_RETURN(parse_expression(tctx, tindex, ast));
        GET_TOKEN(tctx, *tindex, token);
        assert(token->type == ELEMENT_TOK_SEMICOLON);
        tokenlist_advance(tctx, tindex);
    } else {
        assert(false);
        return ELEMENT_ERROR_INVALID_ARCHIVE;
    }
    return ELEMENT_OK;
}

// function ::= qualifier* declaration type? (scope | statement | interface)
// note qualifiers parsed further out and passed in
static element_result parse_function(element_tokeniser_ctx* tctx, size_t* tindex, element_ast* ast, element_ast_flags declflags)
{
    const element_token* token;
    GET_TOKEN(tctx, *tindex, token);

    ast->type = ELEMENT_AST_NODE_FUNCTION;
    element_ast* decl = ast_new_child(ast);
    ELEMENT_OK_OR_RETURN(parse_declaration(tctx, tindex, decl, true));
    decl->flags = declflags;

    element_ast* bodynode = ast_new_child(ast);
    const element_token* body;
    GET_TOKEN(tctx, *tindex, body);
    tokenlist_advance(tctx, tindex);
    if (body->type == ELEMENT_TOK_SEMICOLON) {
        // interface
        bodynode->type = ELEMENT_AST_NODE_INTERFACE;
    } else {
        // real body of some sort
        return parse_body(tctx, tindex, bodynode);
    }
    return ELEMENT_OK;
}

// struct ::= qualifier* 'struct' declaration (scope | interface)
// note qualifiers parsed further out and passed in
static element_result parse_struct(element_tokeniser_ctx* tctx, size_t* tindex, element_ast* ast, element_ast_flags declflags)
{
    const element_token* token;
    GET_TOKEN(tctx, *tindex, token);

    ast->type = ELEMENT_AST_NODE_STRUCT;
    element_ast* decl = ast_new_child(ast);
    ELEMENT_OK_OR_RETURN(parse_declaration(tctx, tindex, decl, false));
    decl->flags = declflags;

    element_ast* bodynode = ast_new_child(ast);
    const element_token* body;
    GET_TOKEN(tctx, *tindex, body);
    tokenlist_advance(tctx, tindex);
    if (body->type == ELEMENT_TOK_SEMICOLON) {
        // interface
        bodynode->type = ELEMENT_AST_NODE_INTERFACE;
    } else if (body->type == ELEMENT_TOK_BRACEL) {
        // scope (struct body)
        ELEMENT_OK_OR_RETURN(parse_scope(tctx, tindex, bodynode));
    } else {
        return ELEMENT_ERROR_INVALID_ARCHIVE;
    }
    return ELEMENT_OK;
}

// namespace ::= 'namespace' identifier scope
static element_result parse_namespace(element_tokeniser_ctx* tctx, size_t* tindex, element_ast* ast)
{
    const element_token* token;
    GET_TOKEN(tctx, *tindex, token);

    ast->type = ELEMENT_AST_NODE_NAMESPACE;
    ELEMENT_OK_OR_RETURN(parse_identifier(tctx, tindex, ast));

    element_ast* scope = ast_new_child(ast);
    ELEMENT_OK_OR_RETURN(parse_scope(tctx, tindex, scope));

    return ELEMENT_OK;
}

// item ::= namespace | struct | function
static element_result parse_item(element_tokeniser_ctx* tctx, size_t* tindex, element_ast* ast)
{
    const element_token* token;
    GET_TOKEN(tctx, *tindex, token);

    // either a qualifier, 'struct', 'namespace' or a name; either way...
    assert(token->type == ELEMENT_TOK_IDENTIFIER);

    if (tctx->text(token) == "namespace") {
        ELEMENT_OK_OR_RETURN(parse_namespace(tctx, tindex, ast));
    } else {
        element_ast_flags flags = 0;
        // parse qualifiers
        ELEMENT_OK_OR_RETURN(parse_qualifiers(tctx, tindex, &flags));
        GET_TOKEN(tctx, *tindex, token);
        if (tctx->text(token) == "struct") {
            ELEMENT_OK_OR_RETURN(parse_struct(tctx, tindex, ast, flags));
        } else {
            ELEMENT_OK_OR_RETURN(parse_function(tctx, tindex, ast, flags));
        }
    }
    return ELEMENT_OK;
}

// start : /^/ (<item>)* /$/;
static element_result parse(element_tokeniser_ctx* tctx, size_t* tindex, element_ast* ast)
{
    size_t tcount;
    const element_token* tok;
    GET_TOKEN_COUNT(tctx, tcount);
    GET_TOKEN(tctx, *tindex, tok);
    ast->type = ELEMENT_AST_NODE_ROOT;
    if (*tindex < tcount && tok->type == ELEMENT_TOK_NONE)
        TOKENLIST_ADVANCE_AND_UPDATE(tctx, tindex, tok);
    while (*tindex < tcount) {
        element_ast* item = ast_new_child(ast);
        ELEMENT_OK_OR_RETURN(parse_item(tctx, tindex, item));
        if (*tindex < tcount && tok->type == ELEMENT_TOK_NONE)
            TOKENLIST_ADVANCE_AND_UPDATE(tctx, tindex, tok);
    }
    return ELEMENT_OK;
}


element_result element_ast_build(element_tokeniser_ctx* tctx, element_ast** ast)
{
    // don't use ast_new here, as we need to return this pointer to the user
    *ast = new element_ast(nullptr);
    size_t tindex = 0;
    element_result r = parse(tctx, &tindex, *ast);
    if (r != ELEMENT_OK) {
        element_ast_delete(*ast);
        *ast = nullptr;
    }
    return r;
}

void element_ast_delete(element_ast* ast)
{
    if (ast)
        ast_clear(ast);
    delete ast;
}

#define PRINTCASE(a) case a: c = #a; break;
static element_result ast_print_depth(element_ast* ast, int depth)
{
    for (int i = 0; i < depth; ++i) printf("  ");
    if (ast->type == ELEMENT_AST_NODE_LITERAL) {
        printf("LITERAL: %f", ast->literal);
    } else if (ast->type == ELEMENT_AST_NODE_IDENTIFIER) {
        printf("IDENTIFIER: %s", ast->identifier.c_str());
    } else if (ast->type == ELEMENT_AST_NODE_DECLARATION) {
        printf("DECLARATION: %s", ast->identifier.c_str());
    } else if (ast->type == ELEMENT_AST_NODE_NAMESPACE) {
        printf("NAMESPACE: %s", ast->identifier.c_str());
    } else if (ast->type == ELEMENT_AST_NODE_CALL) {
        printf("CALL: %s", ast->identifier.c_str());
    } else if (ast->type == ELEMENT_AST_NODE_PORT) {
        printf("PORT: %s", ast->identifier.c_str());
    } else {
        char* c;
        switch (ast->type) {
            PRINTCASE(ELEMENT_AST_NODE_NONE);
            PRINTCASE(ELEMENT_AST_NODE_ROOT);
            PRINTCASE(ELEMENT_AST_NODE_FUNCTION);
            PRINTCASE(ELEMENT_AST_NODE_STRUCT);
            PRINTCASE(ELEMENT_AST_NODE_SCOPE);
            PRINTCASE(ELEMENT_AST_NODE_INTERFACE);
            PRINTCASE(ELEMENT_AST_NODE_EXPRESSION);
            PRINTCASE(ELEMENT_AST_NODE_EXPRLIST);
            PRINTCASE(ELEMENT_AST_NODE_PORTLIST);
            PRINTCASE(ELEMENT_AST_NODE_TYPENAME);
            PRINTCASE(ELEMENT_AST_NODE_LAMBDA);
            default: c = "ELEMENT_AST_NODE_<UNKNOWN>"; break;
        }
        printf("%s", c + strlen("ELEMENT_AST_NODE_"));
    }
    printf("\n");

    for (const auto& child : ast->children)
        ast_print_depth(child.get(), depth + 1);
    return ELEMENT_OK;
}

element_result element_ast_print(element_ast* ast)
{
    return ast_print_depth(ast, 0);
}


element_ast::walk_step element_ast::walk(const element_ast::walker& fn)
{
    walk_step s = fn(this);
    switch (s) {
    case walk_step::step_in:
    {
        auto it = children.begin();
        while (it != children.end() && fn(it->get()) == walk_step::next)
            ++it;
        return walk_step::next;
    }
    case walk_step::next:
    case walk_step::step_out:
    case walk_step::stop:
    default:
        return s;
    }
}

element_ast::walk_step element_ast::walk(const element_ast::const_walker& fn) const
{
    walk_step s = fn(this);
    switch (s) {
    case walk_step::step_in:
    {
        auto it = children.begin();
        while (it != children.end() && static_cast<const element_ast*>(it->get())->walk(fn) == walk_step::next)
            ++it;
        return walk_step::next;
    }
    case walk_step::next:
    case walk_step::step_out:
    case walk_step::stop:
    default:
        return s;
    }
}
