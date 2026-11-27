extern fn fg_cuda_unary_begin() -> i32;
extern fn fg_cuda_unary_prepare(kind: i32, input: const u8[:], output: u8[:], plan: u64[:]) -> i32;
extern fn fg_cuda_unary_launch(kind: i32, plan: const u64[:], elements: u32) -> i32;
extern fn fg_cuda_unary_publish(output: u8[:], device: u64) -> i32;
extern fn fg_cuda_unary_end() -> i32;
extern fn fg_cuda_unary_tanh_error(status: i32, popped: i32);

fn cuda_unary(kind: i32, input: const u8[:], output: u8[:]) -> i64 {
    let active = fg_cuda_unary_begin();
    if active != 1 { return if kind == 0 { active as i64 } else { -1 }; }
    var plan: u64[3] = [0, 0, 0];
    var status = fg_cuda_unary_prepare(kind, input, output, plan);
    if status == 0 { status = fg_cuda_unary_launch(kind, plan, (output.len / 4) as u32); }
    if status == 0 { status = fg_cuda_unary_publish(output, plan[2]); }
    let popped = fg_cuda_unary_end();
    if status == 0 && popped == 0 { return 1; }
    if kind == 0 { fg_cuda_unary_tanh_error(status, popped); }
    return -1;
}

export fn fg_cuda_tanh_f32(input: const u8[:], output: u8[:]) -> i64 {
    if input.len != output.len || input.len <= 0 || input.len % 4 != 0 ||
        input.len / 4 > 2147483647 { return -1; }
    return cuda_unary(0, input, output);
}

export fn fg_cuda_inkling_moe_situ(input: const u8[:], output: u8[:]) -> i64 {
    if output.len <= 0 || output.len % (1152 * 8 * 4) != 0 || output.len / 4 > 2147483647 ||
        input.len != 2 * output.len { return -1; }
    return cuda_unary(1, input, output);
}
