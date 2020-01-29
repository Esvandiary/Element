#pragma once

#include <memory>
#include "common_internal.hpp"

struct element_ctree_node;
using ctree_node_shared_ptr = std::shared_ptr<element_ctree_node>;
using ctree_node_const_shared_ptr = std::shared_ptr<const element_ctree_node>;

struct element_ctree_struct;
using ctree_struct_shared_ptr = std::shared_ptr<element_ctree_struct>;
using ctree_struct_const_shared_ptr = std::shared_ptr<const element_ctree_struct>;

struct element_ctree_function;
using ctree_function_shared_ptr = std::shared_ptr<element_ctree_function>;
using ctree_function_const_shared_ptr = std::shared_ptr<const element_ctree_function>;