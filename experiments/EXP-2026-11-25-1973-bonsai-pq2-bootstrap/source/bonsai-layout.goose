// Bind the initial dense Bonsai profile to validated GGUF tensor indices.
// The base trunk and the MTP tail have different matrix encodings.
fn bonsai_tensor(data: const u8[:], state: i64[:], tensors: i64[:], slots: i64[:],
                 name: const u8[:], dtype: i64, columns: i64, rows: i64) -> i64 {
    let index = gr_find_tensor(data, state, tensors, slots, name);
    if index < 0 { return -1; }
    let at = index * 12;
    if tensors[at + 2] != dtype || tensors[at + 4] != columns ||
       tensors[at + 5] != rows || tensors[at + 6] != 1 || tensors[at + 7] != 1 { return -1; }
    return index;
}

fn bonsai_branch_dims(full: bool, i: i64, matrix: i64, dims: i64[:]) {
    dims[0] = 0; dims[1] = 5120; dims[2] = 1;
    if full {
        if i >= 1 && i <= 4 {
            dims[0] = matrix;
            dims[2] = if i == 1 { 12288 } else { 1024 };
            if i == 4 { dims[1] = 6144; dims[2] = 5120; }
        }
        if i == 5 || i == 6 { dims[1] = 256; }
    } else {
        if i == 1 || i == 2 {
            dims[0] = matrix;
            dims[2] = if i == 1 { 10240 } else { 6144 };
        }
        if i == 3 || i == 4 { dims[2] = 48; }
        if i == 5 || i == 6 { dims[1] = 48; }
        if i == 7 { dims[1] = 4; dims[2] = 10240; }
        if i == 8 { dims[1] = 128; }
        if i == 9 { dims[0] = matrix; dims[1] = 6144; dims[2] = 5120; }
    }
}

fn bonsai_integer(data: const u8[:], state: i64[:], kv: i64[:], slots: i64[:],
                  key: const u8[:], expected: i64) -> bool {
    let index = gr_find_kv(data, state, kv, slots, key);
    var value: i64[1] = [0];
    return index >= 0 && gr_meta_int(data, state, kv, index, value[..]) == 0 && value[0] == expected;
}

export fn bonsai_bind_layout(data: const u8[:], state: i64[:], kv: i64[:], tensors: i64[:],
    kv_slots: i64[:], tensor_slots: i64[:], globals: i64[:], layers: i64[:], draft: i64[:]) -> i64 {
    if globals.len < 3 || layers.len < 65 * 15 || draft.len < 4 { return -1; }
    let arch = gr_find_kv(data, state, kv, kv_slots, "general.architecture");
    if arch < 0 || kv[arch * 8 + 2] != 8 || kv[arch * 8 + 5] != 6 { return -1; }
    let arch_at = kv[arch * 8 + 4];
    if !gr_fits(data, arch_at, 6) || data[arch_at .. arch_at+6] != "qwen35" { return -1; }
    if !bonsai_integer(data, state, kv, kv_slots, "qwen35.block_count", 65) ||
       !bonsai_integer(data, state, kv, kv_slots, "qwen35.nextn_predict_layers", 1) ||
       !bonsai_integer(data, state, kv, kv_slots, "qwen35.full_attention_interval", 4) ||
       !bonsai_integer(data, state, kv, kv_slots, "prism.hadamard.block_size", 1024) ||
       !bonsai_integer(data, state, kv, kv_slots, "prism.hadamard.version", 1) { return -1; }

    globals[0] = bonsai_tensor(data, state, tensors, tensor_slots, "token_embd.weight", 142, 5120, 248320);
    globals[1] = bonsai_tensor(data, state, tensors, tensor_slots, "output_norm.weight", 0, 5120, 1);
    globals[2] = bonsai_tensor(data, state, tensors, tensor_slots, "output.weight", 142, 5120, 248320);
    for i in 3 { if globals[i] < 0 { return -2; } }

    var name: u8[64] = [0; 64];
    var dims: i64[3] = [0; 3];
    for layer in 65 {
        let full = layer % 4 == 3 || layer == 64;
        let matrix: i64 = if layer == 64 { 2 } else { 142 };
        let at = layer * 15;
        layers[at] = if full { 0 } else { 1 };
        for i in 11 { layers[at + 1 + i] = -1; }
        for i in (if full { 8 } else { 11 }) {
            let length = mr_name(layer, mr_branch_name(layers[at], i), name[..]);
            if length < 0 { return -2; }
            bonsai_branch_dims(full, i, matrix, dims[..]);
            let index = bonsai_tensor(data, state, tensors, tensor_slots, name[0..length], dims[0], dims[1], dims[2]);
            if index < 0 { return -2; }
            layers[at + 1 + i] = index;
        }
        for i in 3 {
            let suffix = if i == 0 { "ffn_gate.weight" } else { if i == 1 { "ffn_up.weight" } else { "ffn_down.weight" } };
            let length = mr_name(layer, suffix, name[..]);
            if length < 0 { return -2; }
            let index = bonsai_tensor(data, state, tensors, tensor_slots, name[0..length],
                matrix, if i == 2 { 17408 } else { 5120 }, if i == 2 { 5120 } else { 17408 });
            if index < 0 { return -2; }
            layers[at + 12 + i] = index;
        }
    }

    draft[0] = bonsai_tensor(data, state, tensors, tensor_slots, "blk.64.nextn.eh_proj.weight", 2, 10240, 5120);
    draft[1] = bonsai_tensor(data, state, tensors, tensor_slots, "blk.64.nextn.enorm.weight", 0, 5120, 1);
    draft[2] = bonsai_tensor(data, state, tensors, tensor_slots, "blk.64.nextn.hnorm.weight", 0, 5120, 1);
    draft[3] = bonsai_tensor(data, state, tensors, tensor_slots, "blk.64.nextn.shared_head_norm.weight", 0, 5120, 1);
    for i in 4 { if draft[i] < 0 { return -2; } }
    return 64;
}
