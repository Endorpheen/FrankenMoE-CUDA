export fn giga_fill_step_inputs(base: i64, count: i64, cells: i64,
        positions: i32[:], rows: i64[:], mask: u16[:], output_ids: i32[:]) {
    for row in count {
        let position = base + row;
        positions[row] = position as i32;
        rows[row] = position;
        let start = row * cells;
        let visible = position + 1;
        for cell in visible { mask[start + cell] = 0; }
        for cell in cells - visible { mask[start + visible + cell] = 0xfc00; }
    }
    for i in output_ids.len {
        output_ids[i] = (count - output_ids.len + i) as i32;
    }
}

extern "ggml_new_tensor_1d" fn giga_step_tensor1(ctx: u64, kind: i32, count: i64) -> u64;
extern "ggml_set_input" fn giga_step_mark_input(tensor: u64);

export fn giga_prepare_step_inputs(ctx: u64, capacity: i64, base: i64, count: i64,
        output_count: i64, tensors: u64[:]) -> i64 {
    if ctx == 0 || tensors.len != 5 || count < 1 || count > 4096 ||
       output_count < 0 || output_count > count || base < 0 || base > capacity ||
       count > capacity - base { return -1; }

    let live_cells = base + count;
    let padded_cells = (live_cells + 31) & ~31;
    let cells = if padded_cells <= capacity { padded_cells } else { live_cells };
    tensors[0] = giga_step_tensor1(ctx, 26, count);
    tensors[1] = giga_step_tensor1(ctx, 26, count);
    tensors[2] = giga_step_tensor1(ctx, 27, count);
    tensors[3] = giga_tensor4(ctx, 1, cells, count, 1, 1);
    tensors[4] = if output_count > 0 && output_count < count {
        giga_step_tensor1(ctx, 26, output_count)
    } else { 0 };
    for i in 4 { giga_step_mark_input(tensors[i]); }
    if tensors[4] != 0 { giga_step_mark_input(tensors[4]); }
    return cells;
}
