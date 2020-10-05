#include "testsetup_jit_native.h"

#include "test_maths_scalar.h"
#include "test_maths_vector.h"
#include "test_bounds.h"
#include "test_trig.h"
#include "test_misc.h"
#include "test_branch.h"
#include "test_fncall.h"
#include "test_manifest.h"


int main(int argc, char** argv)
{
    register_suite_maths_scalar();
    register_suite_maths_vector();
    register_suite_bounds();
    register_suite_trig();
    register_suite_misc();
    register_suite_branch();
    register_suite_fncall();
    register_suite_manifest();

    return CU_CI_main(argc, argv);
}