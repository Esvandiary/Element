#include "ctree/ctree.hpp"

DEFINE_TYPE_ID(element_ctree_function,        1U << 0);
DEFINE_TYPE_ID(element_ctree_intrinsic,       1U << 1);
DEFINE_TYPE_ID(element_ctree_custom_function, 1U << 2);
DEFINE_TYPE_ID(element_ctree_struct,          1U << 3);
DEFINE_TYPE_ID(element_ctree_struct_instance, 1U << 4);
DEFINE_TYPE_ID(element_ctree_constant,        1U << 5);
DEFINE_TYPE_ID(element_ctree_call,            1U << 6);
DEFINE_TYPE_ID(element_ctree_input,           1U << 7);
