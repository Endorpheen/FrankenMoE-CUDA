export fn goose_tensor_span(kind: i64, layout: const i64[:]) -> i64 {
    if layout.len != 8 { return ABI_NONE; }
    let d0 = layout[0];
    let d1 = layout[1];
    let d2 = layout[2];
    let d3 = layout[3];
    if d0 <= 0 || d1 <= 0 || d2 <= 0 || d3 <= 0 { return 0; }
    if !ab_range(kind) { return ABI_NONE; }

    let quant_block = AB_BLCK[kind];
    if quant_block <= 0 { return ABI_NONE; }
    let stride0 = layout[4];
    let tail = (d1 - 1) * layout[5] + (d2 - 1) * layout[6] + (d3 - 1) * layout[7];
    if quant_block == 1 { return AB_SIZE[kind] + (d0 - 1) * stride0 + tail; }
    return d0 * stride0 / quant_block + tail;
}

export fn goose_tensor_spans(records: const i64[:], spans: i64[:]) -> bool {
    if records.len % 9 != 0 || records.len / 9 != spans.len { return false; }
    for i in spans.len {
        let first = 9 * i;
        let bytes = goose_tensor_span(records[first], records[first + 1 .. first + 9]);
        if bytes < 0 { return false; }
        spans[i] = bytes;
    }
    return true;
}
