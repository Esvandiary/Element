#pragma once

#include "ctree/fwd.hpp"
#include "ast/ast_internal.hpp"
#include "ast/functions.hpp"
#include "ast/types.hpp"
#include "typeutil.hpp"

#include <vector>


struct field_info
{
    std::string name() const { return m_name; }
    type_const_shared_ptr type() const { return m_type; }
    type_shared_ptr type() { return m_type; }

    bool operator==(const field_info& other) const
    {
        return m_name == other.m_name && m_type == other.m_type;
    }

private:
    std::string m_name;
    type_shared_ptr m_type;
};


struct element_ctree_node : public rtti_type<element_ctree_node>, public std::enable_shared_from_this<element_ctree_node>
{
    const element_scope* scope() const { return m_scope; }

    virtual ~element_ctree_node() = default;

protected:
    element_ctree_node(element_type_id id, const element_scope* scope) : rtti_type(id), m_scope(scope) {}

    template <typename Derived>
    std::shared_ptr<Derived> shared_from_base()
    {
        return std::static_pointer_cast<Derived>(shared_from_this());
    }

private:
    const element_type_id m_type_id;
    const element_scope* m_scope;
};


struct element_ctree_function : public element_ctree_node
{
    DECLARE_TYPE_ID();

    element_ctree_function(
        const element_type_id id,
        const element_scope* scope,
        std::vector<field_info> inputs,
        std::vector<field_info> outputs
    )
        : element_ctree_node(type_id | id, scope)
        , m_inputs(std::move(inputs))
        , m_outputs(std::move(outputs))
    {
    }

    function_const_shared_ptr function() const { return m_scope->function(); }

protected:
    std::vector<field_info> m_inputs;
    std::vector<field_info> m_outputs;
};

struct element_ctree_intrinsic : public element_ctree_function
{
    DECLARE_TYPE_ID();

    element_ctree_intrinsic(const element_scope* scope, std::vector<field_info> inputs, std::vector<field_info> outputs)
        : element_ctree_function(type_id, scope, inputs, outputs)
    {
    }

    function_const_shared_ptr function() const { return m_scope->function(); }
};

struct element_ctree_custom_function : public element_ctree_function
{
    DECLARE_TYPE_ID();

    element_ctree_custom_function(
        const element_scope* scope,
        std::vector<field_info> inputs,
        std::vector<field_info> outputs,
        std::vector<ctree_node_shared_ptr> trees
    )
        : element_ctree_function(type_id, scope, inputs, outputs)
        , m_output_trees(std::move(tree))
    {
    }

    function_const_shared_ptr function() const { return m_scope->function(); }

    ctree_node_shared_ptr tree(std::string name);
    ctree_node_const_shared_ptr tree(std::string name) const;

private:
    mutable std::vector<ctree_node_shared_ptr> m_output_trees;
};


struct element_ctree_struct : public element_ctree_node
{
    DECLARE_TYPE_ID();

    element_ctree_struct(const element_scope* scope, std::vector<field_info> members)
        : element_ctree_node(type_id, scope)
        , m_members(std::move(members))
    {
    }

private:
    std::vector<field_info> m_members;
};


struct element_ctree_struct_instance : public element_ctree_node
{
    DECLARE_TYPE_ID();

    element_ctree_struct_instance(
        const element_scope* scope,
        ctree_struct_shared_ptr struc,
        std::vector<ctree_node_shared_ptr> mem
    )
        : element_ctree_struct(type_id, scope)
        , m_struct(std::move(struc))
        , m_member_trees(std::move(mem))
    {
    }

private:
    ctree_struct_shared_ptr m_struct;
    std::vector<ctree_node_shared_ptr> m_member_trees;
};


struct element_ctree_constant : public element_ctree_node
{
    DECLARE_TYPE_ID();

    element_value value() const { return m_value; }

    element_ctree_constant(element_value value)
        : element_ctree_node(nullptr) // TODO: is this OK?
        , m_value(value)
    {
    }

private:
    element_value m_value;
};


struct element_ctree_call : public element_ctree_node
{
    DECLARE_TYPE_ID();

    enum class call_type
    {
        function_call, // either globally-defined function or local
        member_access,
        method_call
    };

private:
    call_type m_type;
    ctree_node_shared_ptr m_parent;
    ctree_function_const_shared_ptr m_function;
    std::vector<ctree_node_shared_ptr> m_args;
};


struct element_ctree_input : public element_ctree_node
{
    DECLARE_TYPE_ID();

    element_ctree_input(size_t input_index, size_t input_size)
        : element_ctree_node(nullptr) // TODO: is this OK?
        , m_index(input_index)
        , m_size(input_size)
    {
    }

    size_t index() const { return m_index; }
    size_t size() const { return m_size; }

private:
    size_t m_index;
    size_t m_size;
};