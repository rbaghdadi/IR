#include <isl/set.h>
#include <isl/union_map.h>
#include <isl/union_set.h>
#include <isl/ast_build.h>
#include <isl/schedule.h>
#include <isl/schedule_node.h>

#include <tiramisu/debug.h>
#include <tiramisu/core.h>

#include <string.h>
#include <Halide.h>

#include "wrapper_test_21.h"

using namespace tiramisu;

/**
 * Test calls to external functions.
 */

void generate_function(std::string name, int size, int val0)
{
    tiramisu::global::set_default_tiramisu_options();
    

    tiramisu::function function0(name);
    tiramisu::constant N("N", tiramisu::expr((int32_t) size), p_int32, true, NULL, 0, &function0);
    tiramisu::constant M("M", tiramisu::expr((int32_t) size), p_int32, true, NULL, 0, &function0);

    tiramisu::buffer buf0("buf0", {size, size}, tiramisu::p_uint8, a_output, &function0);
    tiramisu::buffer buf1("buf1", {size, size}, tiramisu::p_uint8, a_output, &function0);

    tiramisu::expr e0 = tiramisu::expr();
    tiramisu::computation S0("[N,M]->{S0[i,j]: 0<=i<N and 0<=j<N}", e0, false, p_uint8, &function0);

    tiramisu::expr e1 = tiramisu::expr(tiramisu::o_call, "my_external", {tiramisu::expr(o_address, tiramisu::var("S0"))},
                                       tiramisu::p_uint8);
    tiramisu::computation S1("[N,M]->{S1[i,j]: 0<=i<M and 0<=j<M}", e1, true, p_uint8, &function0);

    S0.set_access("[N,M]->{S0[i,j]->buf0[i,j]: 0<=i<N and 0<=j<N}");
    S1.set_access("[N,M]->{S1[i,j]->buf1[i,j]: 0<=i<N and 0<=j<N}");

    function0.set_arguments({&buf0, &buf1});
    function0.gen_time_space_domain();
    function0.gen_isl_ast();
    function0.gen_halide_stmt();
    function0.gen_c_code();
    function0.gen_halide_obj("generated_fct_test_" + std::string(TEST_NUMBER_STR) + ".o");
}

int main(int argc, char **argv)
{
    generate_function("tiramisu_generated_code", SIZE0, 1);

    return 0;
}
