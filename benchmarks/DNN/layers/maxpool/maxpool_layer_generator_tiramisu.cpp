#include <tiramisu/tiramisu.h>
#include "configure.h"
#define padValue -2147483647
using namespace tiramisu;

int main(int argc, char **argv)
{
    init("maxpool_tiramisu");

    // -------------------------------------------------------
    // Layer I
    // -------------------------------------------------------

    function maxpool_tiramisu("maxpool_tiramisu");

    // parameters
    // N: parameters[0]
    // FIn: parameters[1]
    // BATCH_SIZE: parameters[2]
    var i("i", 0, 3);
    input parameters("parameters", {i}, p_int32);

    constant C_N("C_N", parameters(0)); //input size
    constant C_FIn("C_FIn", parameters(1));
    constant C_BATCH_SIZE("C_BATCH_SIZE", parameters(2));

    var j("j", 0, 2);
    input strides("strides", {j}, p_int32);
    input padding("padding", {j}, p_int32);
    input kernel("kernel", {j}, p_int32);

    constant C_SX("C_SX", strides(0));
    constant C_SY("C_SY", strides(1));
    constant C_PX("C_PX", padding(0));
    constant C_PY("C_PY", padding(1));
    constant C_KX("C_KX", kernel(0));
    constant C_KY("C_KY", kernel(1));

    constant C_N_PADX("C_N_PADX", parameters(0) + 2 * padding(0)); //buffer padded size
    constant C_N_PADY("C_N_PADY", parameters(0) + 2 * padding(1));

    constant C_N_PADD2("C_N_PADD2", parameters(0) + padding(0));
    constant C_N_PADD3("C_N_PADD3", parameters(0) + padding(1));

    constant C_N_OUTPUTX("C_N_OUTPUTX", (parameters(0) - kernel(0) + 2 * padding(0)) / strides(0) + expr(1)); //output size
    constant C_N_OUTPUTY("C_N_OUTPUTY", (parameters(0) - kernel(1) + 2 * padding(1)) / strides(1) + expr(1));

    var x("x", 0, C_N), y("y", 0, C_N), z("z", 0, C_FIn), n("n", 0, C_BATCH_SIZE); // input
    var x1("x1", 0, C_N_PADX), y1("y1", 0, C_N_PADY);                              // inputpadd
    var x2("x2", C_PX, C_N_PADD2), y2("y2", C_PY, C_N_PADD3);                      //init_input
    var x3("x3", 0, C_N_OUTPUTX), y3("y3", 0, C_N_OUTPUTY);                        // output
    var k_y("k_y", 0, C_KY), k_x("k_x", 0, C_KX);                                  // kernel

    // Input computations
    input c_input("c_input", {n, z, y, x}, p_float32);

    computation inputPadd("inputPadd", {n, z, y1, x1}, padValue);
    computation init_input("init_input", {n, z, y2, x2}, c_input(n, z, cast(p_int32, y2) - cast(p_int32, C_PY), cast(p_int32, x2) - cast(p_int32, C_PX)));

    computation init_output("init_output", {n, z, y3, x3}, (float)-2147483647);
    computation output("output", {n, z, y3, x3, k_y, k_x}, expr(o_max, cast(p_float32, init_output(n, z, y3, x3)), cast(p_float32, inputPadd(n, z, y3 * C_SY + k_y, x3 * C_SX + k_x))));

    // Layer II
    if (LARGE_DATA_SET)
    {
        if (0)
        {
            int vec_len = 32;
            int y_block = 32;
            int o_block = 4;

            init_input.after(inputPadd, 2);
            init_input.tag_parallel_level(0);

            init_output.after(init_input, 2);

            init_output.tag_parallel_level(0);
            init_output.split(3, vec_len);
            init_output.tag_vector_level(4, vec_len);

            output.after(init_output, 1);

            output.interchange(3, 4);
            // init_output.tile(1, 2, 32, 32);
            // output.tile(1, 2, 32, 32);

            // output.split(1, o_block);
            output.split(2, y_block);
            output.split(5, vec_len);
            output.tag_vector_level(6, vec_len);
            output.tag_unroll_level(7);
            output.tag_unroll_level(8);
        }
        if (0)
        {
            int vec_len = 32;
            int y_block = 32;
            int o_block = 4;
            init_input.after(inputPadd, 2);
            init_input.tag_parallel_level(0);

            init_output.after(init_input, 2);

            init_output.tag_parallel_level(0);
            output.after(init_output, 2);

            // 0, 1,   2,   3,   4,   5,     6,
            // n, z,   y,   x, r_z, r_y,   r_x,
            output.interchange(3, 4);
            // n, z,   y  r_z,   x, r_y,   r_x,

            output.split(1, o_block);
            init_output.split(1, o_block);

            output.split(3, y_block);
            output.split(6, vec_len);
            output.tag_vector_level(7, vec_len);
            output.tag_unroll_level(9);

            init_output.split(4, vec_len);
            init_output.tag_vector_level(5, vec_len);
        }
        if (1)
        {
            int vec_len = 32;
            int y_block = 32;
            int o_block = 4;
            init_input.after(inputPadd, 2);
            init_input.tag_parallel_level(0);

            init_output.after(init_input, 2);

            init_output.tag_parallel_level(0);
            output.after(init_output, 2);

            // 0, 1,   2,   3,   4,   5,     6,
            // n, z,   y,   x, r_z, r_y,   r_x,
            output.interchange(3, 4);
            // n, z,   y, (r_z,   x), r_y,   r_x,
            output.interchange(3, 2);
            // n, z, (r_z,   y),   x, r_y,   r_x,

            output.split(1, o_block);
            init_output.split(1, o_block);
            // n, (z, z_t), r_z,   y,       x, r_y,   r_x,

            output.split(3, y_block);
            output.split(6, vec_len);
            output.tag_vector_level(7, vec_len);
            output.tag_unroll_level(8);
            output.tag_unroll_level(9);

            // n,  z, z_t,  r_z,  (y, y_t), x, r_y,   r_x,
            init_output.split(4, vec_len);
            init_output.tag_vector_level(5, vec_len);
        }
    }
    else if (MEDIUM_DATA_SET)
    {
        int vec_len = 32;
        inputPadd.tag_parallel_level(0);
        init_input.after(inputPadd, 2);
        init_input.tag_parallel_level(0);
        init_output.after(init_input, 1);
        init_output.tag_parallel_level(0);
        output.after(init_output, 0);
        output.interchange(3, 4);
        init_output.tag_vector_level(6, vec_len);
    }
    else if (SMALL_DATA_SET)
    {
        int vec_len = 16;
        inputPadd.tag_parallel_level(0);
        init_input.after(inputPadd, 2);
        init_input.tag_parallel_level(0);
        init_output.after(init_input, 1);
        init_output.tag_parallel_level(0);
        output.after(init_output, 0);
        output.interchange(3, 4);
        init_output.tag_vector_level(6, vec_len);
    }

    // Layer III

    buffer parameters_buf("parameters_buf", {expr(3)}, p_int32, a_input);
    buffer input_buf("input_buf", {C_BATCH_SIZE, C_FIn, C_N, C_N}, p_float32, a_input);

    buffer strides_buf("strides_buf", {expr(2)}, p_int32, a_input);
    buffer padding_buf("padding_buf", {expr(2)}, p_int32, a_input);
    buffer kernel_buf("kernel_buf", {expr(2)}, p_int32, a_input);

    buffer inputPadd_buf("inputPadd_buf", {C_BATCH_SIZE, C_FIn, C_N_PADY, C_N_PADX}, p_float32, a_output);
    buffer output_buf("output_buf", {C_BATCH_SIZE, C_FIn, C_N_OUTPUTY, C_N_OUTPUTX}, p_float32, a_output);

    parameters.store_in(&parameters_buf);
    c_input.store_in(&input_buf);

    strides.store_in(&strides_buf);
    padding.store_in(&padding_buf);
    kernel.store_in(&kernel_buf);

    inputPadd.store_in(&inputPadd_buf);
    init_input.store_in(&inputPadd_buf);
    init_output.store_in(&output_buf);
    output.store_in(&output_buf, {n, z, y3, x3});

    tiramisu::codegen({&parameters_buf, &input_buf, &output_buf, &strides_buf, &padding_buf, &kernel_buf, &inputPadd_buf}, "generated_maxpool_layer.o");

    return 0;
}