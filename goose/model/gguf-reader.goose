// Engine GGUF v2/v3 reader. No native reads, strings, containers or allocations.
// Borrowed immutable bytes; caller-owned disjoint records and hash slots.
// KV stride8: name_off,len,wire_type,element_type,payload_off,count,bytes,hash.
// Tensor stride12: name_off,len,type,declared_dims,ne[4],rel,bytes,abs,hash.
// State8: magic919,version,n_kv,n_tensors,alignment,data_off,padded_data,meta_end.
// A failed parse clears magic; scratch outputs may be dirty and are unpublished.

fn gr_fits(data: const u8[:], at: i64, n: i64) -> bool {
    return at >= 0 && n >= 0 && at <= data.len && n <= data.len - at;
}

fn gr_bits(data: const u8[:], at: i64, n: i64) -> u64 {
    var result: u64 = 0; var i: i64 = 0;
    while i < n { result = result | ((data[at + i] as u64) << (i * 8)); i += 1; }
    return result;
}

fn gr_length(data: const u8[:], at: i64) -> i64 {
    if !gr_fits(data, at, 8) { return -1; }
    let length: u64 = gr_bits(data, at, 8);
    if length > 9223372036854775807 { return -1; }
    let n: i64 = length as i64;
    return if gr_fits(data, at + 8, n) { n } else { -1 };
}

fn gr_hash(data: const u8[:], at: i64, n: i64) -> u64 {
    var h: u64 = 14695981039346656037; var i: i64 = 0;
    while i < n { h = (h ^ (data[at + i] as u64)) * 1099511628211; i += 1; }
    return h;
}

fn gr_same(data: const u8[:], a: i64, na: i64, b: i64, nb: i64) -> bool {
    if na != nb || !gr_fits(data, a, na) || !gr_fits(data, b, nb) { return false; }
    var i: i64 = 0; while i < na { if data[a + i] != data[b + i] { return false; } i += 1; }
    return true;
}

fn gr_name_ok(data: const u8[:], at: i64, n: i64) -> bool {
    var i: i64 = 0; while i < n { if data[at + i] == 0 { return false; } i += 1; }
    return true;
}

fn gr_slots_ok(slots: i64[:], n: i64) -> bool {
    return slots.len > 0 && (slots.len & (slots.len - 1)) == 0 && n >= 0 && n <= slots.len / 2;
}

fn gr_insert(data: const u8[:], records: i64[:], stride: i64, slots: i64[:], index: i64) -> bool {
    let base: i64 = stride * index; let h: u64 = records[base + stride - 1] as! u64;
    var slot: i64 = (h & ((slots.len - 1) as u64)) as i64; var visited: i64 = 0;
    while visited < slots.len {
        let entry: i64 = slots[slot];
        if entry == 0 { slots[slot] = index + 1; return true; }
        if entry < 1 || entry > index { return false; }
        let prior: i64 = (entry - 1) * stride;
        if records[prior + stride - 1] == records[base + stride - 1] &&
           gr_same(data, records[prior], records[prior + 1], records[base], records[base + 1]) { return false; }
        slot = (slot + 1) & (slots.len - 1); visited += 1;
    }
    return false;
}

export fn gr_parse(data: const u8[:], file_bytes: i64, state: i64[:], kv: i64[:], tensors: i64[:],
                    kv_slots: i64[:], tensor_slots: i64[:]) -> i64 {
    if state.len < 8 { return -1; }
    state[0] = 0;
    if data.len > 67108864 || !gr_fits(data, 0, 24) || file_bytes < data.len ||
       gr_bits(data, 0, 4) != (ggh_const_value(4) as u64) { return -1; }
    let version: i64 = gr_bits(data, 4, 4) as i64;
    if version != 2 && version != 3 { return -1; }
    let nt_raw: u64 = gr_bits(data, 8, 8); let nk_raw: u64 = gr_bits(data, 16, 8);
    if nt_raw > 9223372036854775807 || nk_raw > 9223372036854775807 { return -2; }
    let nt: i64 = nt_raw as i64; let nk: i64 = nk_raw as i64;
    if nk > kv.len / 8 || nt > tensors.len / 12 || !gr_slots_ok(kv_slots, nk) || !gr_slots_ok(tensor_slots, nt) { return -2; }
    var i: i64 = 0; while i < kv_slots.len { kv_slots[i] = 0; i += 1; }
    i = 0; while i < tensor_slots.len { tensor_slots[i] = 0; i += 1; }
    var at: i64 = 24; var alignment: i64 = 32; i = 0;
    while i < nk {
        let n: i64 = gr_length(data, at);
        if n <= 0 || !gr_name_ok(data, at + 8, n) { return -3; }
        let base: i64 = i * 8; kv[base] = at + 8; kv[base + 1] = n;
        kv[base + 7] = gr_hash(data, at + 8, n) as! i64;
        if !gr_insert(data, kv, 8, kv_slots, i) { return -4; }
        at += 8 + n;
        if !gr_fits(data, at, 4) { return -3; }
        let wire: i64 = gr_bits(data, at, 4) as i64; at += 4;
        var element: i64 = -1; var count: i64 = 1; var payload: i64 = at; var bytes: i64 = 0;
        if wire == 8 {
            let length: i64 = gr_length(data, at); if length < 0 { return -3; }
            payload = at + 8; count = length; bytes = length; at += 8 + length;
        } else {
            if wire == 9 {
                if !gr_fits(data, at, 12) { return -3; }
                element = gr_bits(data, at, 4) as i64;
                let raw: u64 = gr_bits(data, at + 4, 8);
                if raw > 9223372036854775807 { return -3; }
                count = raw as i64; at += 12; payload = at;
                if element == 8 {
                    if count > (data.len - at) / 8 { return -3; }
                    var j: i64 = 0;
                    while j < count { let length: i64 = gr_length(data, at); if length < 0 { return -3; } at += 8 + length; j += 1; }
                    bytes = at - payload;
                } else {
                    let size: i64 = ggh_type_size(element);
                    if size <= 0 || count > (data.len - at) / size { return -5; }
                    bytes = size * count; at += bytes;
                }
            } else {
                bytes = ggh_type_size(wire);
                if bytes <= 0 || !gr_fits(data, at, bytes) { return -5; }
                at += bytes;
            }
        }
        kv[base + 2] = wire; kv[base + 3] = element; kv[base + 4] = payload;
        kv[base + 5] = count; kv[base + 6] = bytes;
        if wire == 7 || (wire == 9 && element == 7) {
            var b: i64 = 0; while b < bytes { if data[payload + b] > 1 { return -5; } b += 1; }
        }
        if data[kv[base] .. kv[base] + n] == "general.alignment" {
            if wire != 4 { return -5; }
            alignment = gr_bits(data, payload, 4) as i64;
            if ggh_alignment_valid(alignment) == 0 { return -6; }
        }
        i += 1;
    }
    var total: i64 = 0; i = 0;
    while i < nt {
        let n: i64 = gr_length(data, at);
        if n < 0 || n >= 64 || !gr_name_ok(data, at + 8, n) { return -3; }
        let base: i64 = i * 12; tensors[base] = at + 8; tensors[base + 1] = n;
        tensors[base + 11] = gr_hash(data, at + 8, n) as! i64;
        if !gr_insert(data, tensors, 12, tensor_slots, i) { return -4; }
        at += 8 + n;
        if !gr_fits(data, at, 4) { return -3; }
        let dims: i64 = gr_bits(data, at, 4) as i64; at += 4;
        if dims > 4 || !gr_fits(data, at, 8 * dims + 12) { return -3; }
        tensors[base + 3] = dims; var d: i64 = 0; var elements: i64 = 1;
        while d < 4 {
            var extent: i64 = 1;
            if d < dims {
                let raw: u64 = gr_bits(data, at, 8); if raw > 9223372036854775807 { return -6; }
                extent = raw as i64; at += 8;
            }
            if extent > 0 && elements > 9223372036854775807 / extent { return -6; }
            elements *= extent; tensors[base + 4 + d] = extent; d += 1;
        }
        let dtype: i64 = gr_bits(data, at, 4) as i64; at += 4;
        let block_size: i64 = t_blck_size(dtype); let size: i64 = t_type_size(dtype);
        if block_size <= 0 || size <= 0 || tensors[base + 4] % block_size != 0 { return -5; }
        var bytes: i64 = 0;
        if elements > 0 {
            if elements / block_size > 9223372036854775807 / size { return -6; }
            bytes = (elements / block_size) * size;
        }
        let relative: u64 = gr_bits(data, at, 8); at += 8;
        if relative > 9223372036854775807 || (relative as i64) != total || bytes > 9223372036854775807 - (alignment - 1) { return -6; }
        let padded: i64 = (bytes + alignment - 1) & (0 - alignment);
        if total > 9223372036854775807 - padded { return -6; }
        tensors[base + 2] = dtype; tensors[base + 8] = total; tensors[base + 9] = bytes;
        total += padded; i += 1;
    }
    let data_offset: i64 = ggh_data_offset(at, alignment, nt);
    if data_offset < 0 || data_offset > file_bytes || total > file_bytes - data_offset { return -7; }
    i = 0; while i < nt { tensors[i * 12 + 10] = data_offset + tensors[i * 12 + 8]; i += 1; }
    state[1] = version; state[2] = nk; state[3] = nt; state[4] = alignment;
    state[5] = data_offset; state[6] = total; state[7] = at; state[0] = 919;
    return 0;
}

fn gr_lookup(data: const u8[:], state: i64[:], records: i64[:], slots: i64[:],
              key: const u8[:], stride: i64, which: i64) -> i64 {
    if state.len < 8 || state[0] != 919 || !gr_slots_ok(slots, state[which]) || state[which] > records.len / stride { return -1; }
    let h: u64 = gr_hash(key, 0, key.len);
    var slot: i64 = (h & ((slots.len - 1) as u64)) as i64; var visited: i64 = 0;
    while visited < slots.len {
        let entry: i64 = slots[slot]; if entry == 0 { return -1; }
        if entry < 0 || entry > state[which] { return -1; }
        let base: i64 = (entry - 1) * stride;
        let at: i64 = records[base]; let n: i64 = records[base + 1];
        if (records[base + stride - 1] as! u64) == h && n == key.len && gr_fits(data, at, n) && data[at .. at + n] == key { return entry - 1; }
        slot = (slot + 1) & (slots.len - 1); visited += 1;
    }
    return -1;
}

export fn gr_find_kv(data: const u8[:], state: i64[:], kv: i64[:], slots: i64[:], key: const u8[:]) -> i64 {
    return gr_lookup(data, state, kv, slots, key, 8, 2);
}

// Same decision as gr_find_kv for callers inside the unit: the export must keep
// one concrete signature, so internal callers use this body directly.
fn gr_find_kv_impl(data: const u8[:], state: i64[:], kv: i64[:], slots: i64[:], key: const u8[:]) -> i64 {
    return gr_lookup(data, state, kv, slots, key, 8, 2);
}

export fn gr_find_tensor(data: const u8[:], state: i64[:], tensors: i64[:], slots: i64[:], key: const u8[:]) -> i64 {
    return gr_lookup(data, state, tensors, slots, key, 12, 3);
}

fn gr_int(data: const u8[:], at: i64, dtype: i64, out: i64[:]) -> i64 {
    let n: i64 = ggh_type_size(dtype);
    if out.len < 1 || n <= 0 || !gr_fits(data, at, n) || dtype == 6 || dtype == 12 { return -1; }
    let raw: u64 = gr_bits(data, at, n); var value: i64 = raw as! i64;
    if dtype == 10 && raw > 9223372036854775807 { return -1; }
    if dtype == 1 && value >= 128 { value -= 256; }
    if dtype == 3 && value >= 32768 { value -= 65536; }
    if dtype == 5 && value >= 2147483648 { value -= 4294967296; }
    if dtype == 7 { value = if raw == 0 { 0 } else { 1 }; }
    out[0] = value; return 0;
}

fn gr_meta_int_impl(data: const u8[:], state: i64[:], kv: i64[:], index: i64, out: i64[:]) -> i64 {
    if state.len < 8 || state[0] != 919 || index < 0 || index >= state[2] || index >= kv.len / 8 || kv[index * 8 + 2] == 9 { return -1; }
    return gr_int(data, kv[index * 8 + 4], kv[index * 8 + 2], out);
}

fn gr_array_int_impl(data: const u8[:], state: i64[:], kv: i64[:], index: i64, element: i64, out: i64[:]) -> i64 {
    if state.len < 8 || state[0] != 919 || index < 0 || index >= state[2] || index >= kv.len / 8 || element < 0 ||
       kv[index * 8 + 2] != 9 || element >= kv[index * 8 + 5] { return -1; }
    let dtype: i64 = kv[index * 8 + 3]; let size: i64 = ggh_type_size(dtype);
    if size <= 0 || kv[index * 8 + 4] < 0 || element > (9223372036854775807 - kv[index * 8 + 4]) / size { return -1; }
    return gr_int(data, kv[index * 8 + 4] + element * size, dtype, out);
}

fn gr_meta_f32_bits_impl(data: const u8[:], state: i64[:], kv: i64[:], index: i64, out: i64[:]) -> i64 {
    if state.len < 8 || state[0] != 919 || index < 0 || index >= state[2] || index >= kv.len / 8 || out.len < 1 ||
       kv[index * 8 + 2] != 6 || !gr_fits(data, kv[index * 8 + 4], 4) { return -1; }
    out[0] = gr_bits(data, kv[index * 8 + 4], 4) as i64; return 0;
}

// Stable public slice ABI. Internal model consumers call the private bodies,
// allowing literal/view specialization without changing the C entry signatures.
export fn gr_meta_int(data: const u8[:], state: i64[:], kv: i64[:], index: i64, out: i64[:]) -> i64 {
    return gr_meta_int_impl(data, state, kv, index, out);
}

export fn gr_array_int(data: const u8[:], state: i64[:], kv: i64[:], index: i64, element: i64, out: i64[:]) -> i64 {
    return gr_array_int_impl(data, state, kv, index, element, out);
}

export fn gr_meta_f32_bits(data: const u8[:], state: i64[:], kv: i64[:], index: i64, out: i64[:]) -> i64 {
    return gr_meta_f32_bits_impl(data, state, kv, index, out);
}

export fn gr_string_array_index(data: const u8[:], state: i64[:], kv: i64[:], index: i64, pairs: i64[:]) -> i64 {
    return gr_string_array_index_impl(data, state, kv, index, pairs);
}

fn gr_string_array_index_impl(data: const u8[:], state: i64[:], kv: i64[:], index: i64, pairs: i64[:]) -> i64 {
    if state.len < 8 || state[0] != 919 || index < 0 || index >= state[2] || index >= kv.len / 8 ||
       kv[index * 8 + 2] != 9 || kv[index * 8 + 3] != 8 { return -1; }
    let count: i64 = kv[index * 8 + 5]; if count < 0 || count > pairs.len / 2 { return -1; }
    var at: i64 = kv[index * 8 + 4]; var i: i64 = 0;
    while i < count { let n: i64 = gr_length(data, at); if n < 0 { return -1; } at += 8 + n; i += 1; }
    at = kv[index * 8 + 4]; i = 0;
    while i < count { let n: i64 = gr_length(data, at); pairs[i * 2] = at + 8; pairs[i * 2 + 1] = n; at += 8 + n; i += 1; }
    return count;
}

// Header-field readers for a host that keeps its own container but wants the
// Goose reader to answer the metadata questions. Index i is the key's position
// in the file, which is what both readers use.
export fn gr_kv_type(state: i64[:], kv: i64[:], index: i64) -> i64 {
    if state.len < 8 || state[0] != 919 || index < 0 || index >= state[2] || index >= kv.len / 8 {
        return -1;
    }
    return kv[index * 8 + 2];
}

export fn gr_arr_type(state: i64[:], kv: i64[:], index: i64) -> i64 {
    if state.len < 8 || state[0] != 919 || index < 0 || index >= state[2] || index >= kv.len / 8 ||
       kv[index * 8 + 2] != 9 {
        return -1;
    }
    return kv[index * 8 + 3];
}

export fn gr_arr_n(state: i64[:], kv: i64[:], index: i64) -> i64 {
    return gr_arr_n_impl(state, kv, index);
}

fn gr_arr_n_impl(state: i64[:], kv: i64[:], index: i64) -> i64 {
    if state.len < 8 || state[0] != 919 || index < 0 || index >= state[2] || index >= kv.len / 8 ||
       kv[index * 8 + 2] != 9 {
        return -1;
    }
    return kv[index * 8 + 5];
}

export fn gr_n_kv(state: i64[:]) -> i64 {
    if state.len < 8 || state[0] != 919 {
        return -1;
    }
    return state[2];
}

// Payload spans for a host that keeps reading values from its own container but
// wants the Goose reader to say where they are. Offsets are relative to the
// header window the reader parsed; a value outside it is refused.
export fn gr_val_span(state: i64[:], kv: i64[:], index: i64, out: i64[:]) -> i64 {
    if state.len < 8 || state[0] != 919 || index < 0 || index >= state[2] || index >= kv.len / 8 || out.len < 4 {
        return -1;
    }
    out[0] = kv[index * 8 + 2]; // wire type
    out[1] = kv[index * 8 + 4]; // payload offset
    out[2] = kv[index * 8 + 6]; // payload bytes
    out[3] = kv[index * 8 + 3]; // element type, -1 for scalars
    return 0;
}

// Span of element `i` of a string array, as (offset, length) into the header.
export fn gr_arr_str_span(data: const u8[:], state: i64[:], kv: i64[:], index: i64, element: i64, out: i64[:]) -> i64 {
    if state.len < 8 || state[0] != 919 || index < 0 || index >= state[2] || index >= kv.len / 8 ||
       out.len < 2 || kv[index * 8 + 2] != 9 || kv[index * 8 + 3] != 8 || element < 0 ||
       element >= kv[index * 8 + 5] {
        return -1;
    }
    var at: i64 = kv[index * 8 + 4];
    var i: i64 = 0;
    while i < element {
        if !gr_fits(data, at, 8) {
            return -1;
        }
        let n: u64 = gr_bits(data, at, 8);
        if n > 9223372036854775807 {
            return -1;
        }
        at = at + 8 + (n as! i64);
        i = i + 1;
    }
    if !gr_fits(data, at, 8) {
        return -1;
    }
    let len: u64 = gr_bits(data, at, 8);
    if len > 9223372036854775807 || !gr_fits(data, at + 8, len as! i64) {
        return -1;
    }
    out[0] = at + 8;
    out[1] = len as! i64;
    return 0;
}

// Lay out a whole GGUF string array as NUL-terminated C strings in one pass.
// GGUF strings are length-prefixed and carry no terminator, so the terminator
// is written here, where the length is known; a consumer that treats the mapped
// bytes as a C string would otherwise run past the element into the next
// entry's length word. `offs[i]` receives the offset of element i inside
// `text`, which holds every entry back to back. Returns the element count, or
// -(needed + 1) when `text` is smaller than needed, or -1 on a bad span, or -2
// when `offs` is too small.
export fn gr_arr_str_table(data: const u8[:], state: i64[:], kv: i64[:], index: i64,
                           text: u8[:], offs: i64[:]) -> i64 {
    if state.len < 8 || state[0] != 919 || index < 0 || index >= state[2] || index >= kv.len / 8 ||
       kv[index * 8 + 2] != 9 || kv[index * 8 + 3] != 8 {
        return -1;
    }
    let n: i64 = kv[index * 8 + 5];
    if offs.len < n {
        return -2;
    }
    var at: i64 = kv[index * 8 + 4];
    var total: i64 = 0;
    var i: i64 = 0;
    while i < n {
        if !gr_fits(data, at, 8) {
            return -1;
        }
        let len: u64 = gr_bits(data, at, 8);
        if len > 9223372036854775807 || !gr_fits(data, at + 8, len as! i64) {
            return -1;
        }
        offs[i] = total;
        total = total + (len as! i64) + 1;
        at = at + 8 + (len as! i64);
        i = i + 1;
    }
    if text.len < total {
        return -(total + 1);
    }
    at = kv[index * 8 + 4];
    i = 0;
    while i < n {
        let len: u64 = gr_bits(data, at, 8);
        let ln: i64 = len as! i64;
        let base: i64 = offs[i];
        var k: i64 = 0;
        while k < ln {
            text[base + k] = data[at + 8 + k];
            k = k + 1;
        }
        text[base + ln] = 0;
        at = at + 8 + ln;
        i = i + 1;
    }
    return n;
}
