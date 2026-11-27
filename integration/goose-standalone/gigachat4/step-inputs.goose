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
