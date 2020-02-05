#pragma once

#include "ast/fwd.hpp"
#include "ctree/fwd.hpp"
#include "ctree/ctree.hpp"
#include "common_internal.hpp"
#include "interpreter_internal.hpp"

struct function_cache
{
    struct frame
    {
    friend struct function_cache;
    public:
        ~frame() { m_parent.pop_frame(); }
    protected:
        frame(function_cache& parent) : m_parent(parent) { m_parent.push_frame(); }
        function_cache& m_parent;
    };

    function_cache() { m_cache.resize(1); }

    void add(const element_scope* scope, ctree_function_shared_ptr node) { m_cache.back()[scope] = node; }

    frame add_frame() { return frame(*this); }

    ctree_function_shared_ptr search(const element_scope* scope) const
    {
        for (auto it = m_cache.rbegin(); it != m_cache.rend(); ++it) {
            auto mit = it->find(scope);
            if (mit != it->end())
                return mit->second;
        }
        return nullptr;
    }

private:
    std::vector<std::unordered_map<const element_scope*, ctree_function_shared_ptr>> m_cache;

    void push_frame() { m_cache.emplace_back(); }
    void pop_frame() { m_cache.pop_back(); }
};


struct element_ctree_compiler_ctx
{
    element_interpreter_ctx& ictx;
    element_compiler_options options;
    function_cache fn_cache;
};


element_result element_ctree_compile(
    element_interpreter_ctx& ctx,
    function_const_shared_ptr fn,
    ctree_function_shared_ptr& expr,
    element_compiler_options opts);
