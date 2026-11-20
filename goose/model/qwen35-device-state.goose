extern "fg_cuda_load_model_state" fn state_load(ranges: const i64[:], bytes: i64) -> i64;
extern "fg_cuda_reset_model_state" fn state_reset_cuda() -> i64;

// State manifest rows: host address, byte span, device offset, kind.
// Kind 0 is a Q4 KV tensor, and kind 1 is the recurrent matrix state.
fn qr_state_span(tensor: i64, kind: i64) -> i64 {
    if tensor == 0 || pred(0, tensor, 0) == 0 || ne(tensor, 3) != 1 { return 0; }
    if kind == 0 {
        if prop(tensor, 0) != 2 || ne(tensor, 2) != 1 { return 0; }
        return qk_row_bytes(ne(tensor, 0)) * ne(tensor, 1);
    }
    if prop(tensor, 0) != 0 { return 0; }
    return 4 * ne(tensor, 0) * ne(tensor, 1) * ne(tensor, 2);
}

export fn qr_cuda_state(owner: const i64[:], layers: const i64[:], head: i64) -> i64 {
    if owner.len < 13 || owner[0] != 9191 || owner[5] <= 0 ||
       layers.len / 32 < owner[5] || head == 0 { return -1; }
    let available = mr_cuda_free();
    if available <= 0 { return available; }
    var ranges: i64[>..] = [];
    ranges.resize(owner[5] * 8, 0);
    var bytes: i64 = 0;
    var count: i64 = 0;
    for i in owner[5] {
        let row = i * 32;
        let recurrent = layers[row] == 1;
        let span = if recurrent { 1 } else { 2 };
        for j in span {
            let at = if recurrent { row + 21 } else { row + 22 + j };
            let t = layers[at];
            let kind = if recurrent { 1 } else { 0 };
            let length = qr_state_span(t, kind);
            if length <= 0 || length > owner[12] - bytes { return -1; }
            let addr = qa_get(t, 4, 0);
            if addr == 0 { return -1; }
            ranges[count] = addr as! i64;
            ranges[count + 1] = length;
            ranges[count + 2] = bytes;
            ranges[count + 3] = kind;
            bytes += length;
            count += 4;
        }
    }
    if bytes > available || mr_cuda_head_bytes(head) > available - bytes { return 0; }
    return state_load(ranges[0 .. count], bytes);
}

export fn qr_state_offset(ranges: const i64[:], address: u64, bytes: i64, kind: i64) -> i64 {
    if bytes <= 0 || ranges.len % 4 != 0 { return -1; }
    for i in ranges.len / 4 {
        let at = 4 * i;
        let base = ranges[at] as! u64;
        let size = ranges[at + 1];
        if address < base || size < bytes || (kind >= 0 && ranges[at + 3] != kind) { continue; }
        let offset = address - base;
        if offset > ((size - bytes) as u64) { continue; }
        // The GDN operation publishes exactly one complete model tensor.
        if kind == 1 && (offset != 0 || bytes != size) { continue; }
        return ranges[at + 2] + (offset as i64);
    }
    return -1;
}

export fn qr_reset_state(owner: const i64[:], layers: const i64[:], session: i64[:]) -> i64 {
    if owner.len < 12 || owner[0] != 9191 || owner[5] <= 0 || layers.len / 32 < owner[5] { return -1; }
    for i in owner[5] {
        let row = i * 32;
        let slot = if layers[row] == 1 { 20 } else { 22 };
        if qo_tensor_zero(layers[row + slot]) == 0 || qo_tensor_zero(layers[row + slot + 1]) == 0 { return -1; }
    }
    if state_reset_cuda() != 0 { return -1; }
    return q35_kv_init(owner[10], 32, session);
}
