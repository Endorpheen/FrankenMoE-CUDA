// Stateless descriptors for the pinned Edge0-120B Q3_K pack.
// Output: file id, byte offset, byte count, ggml storage type, ne0, ne1,
// row stride, outer-record stride. This module neither maps nor retains files.
// File ids: 0 experts, 1 Q4 D/U, 2 Q8 D/U, 3 FP8 PLE, 4 labels,
// 100..147 per-layer F32 column scales. PLE storage I8 means raw E4M3 bytes.

export fn e0_expert_desc(layer: i64, expert: i64, matrix: i64, out: i64[:]) -> i64 {
    if out.len < 8 || layer < 0 || layer >= 48 || expert < 0 || expert >= 512 || matrix < 0 || matrix > 2 {
        return -1;
    }
    let base: i64 = (layer * 512 + expert) * 1164800;
    out[0] = 0;
    out[7] = 1164800;
    if matrix == 2 {
        out[1] = base + 704000;
        out[2] = 460800;
        out[3] = 20;
        out[4] = 640;
        out[5] = 1280;
        out[6] = 360;
    } else {
        out[1] = base + matrix * 352000;
        out[2] = 352000;
        out[3] = 11;
        out[4] = 1280;
        out[5] = 640;
        out[6] = 550;
    }
    return 0;
}

export fn e0_du_desc(layer: i64, group: i64, bits: i64, matrix: i64, out: i64[:]) -> i64 {
    if out.len < 8 || layer < 0 || layer >= 48 || group < 0 || group >= 8 || (bits != 4 && bits != 8) || matrix < 0 || matrix > 1 {
        return -1;
    }
    let block_bytes: i64 = if bits == 4 { 18 } else { 34 };
    let group_bytes: i64 = 2560 * 1280 / 32 * block_bytes;
    let layer_bytes: i64 = group_bytes * 16;
    out[0] = if bits == 4 { 1 } else { 2 };
    out[1] = layer * layer_bytes + matrix * group_bytes * 8 + group * group_bytes;
    out[2] = group_bytes;
    out[3] = if bits == 4 { 2 } else { 8 };
    out[4] = if matrix == 0 { 2560 } else { 1280 };
    out[5] = if matrix == 0 { 1280 } else { 2560 };
    out[6] = out[4] / 32 * block_bytes;
    out[7] = layer_bytes;
    return 0;
}

export fn e0_ple_desc(row: i64, out: i64[:]) -> i64 {
    if out.len < 8 || row < 0 || row >= 320001536 {
        return -1;
    }
    out[0] = 3;
    out[1] = 352 + row * 160;
    out[2] = 160;
    out[3] = 24;
    out[4] = 160;
    out[5] = 1;
    out[6] = 160;
    out[7] = 160;
    return 0;
}

export fn e0_label_desc(layer: i64, expert: i64, out: i64[:]) -> i64 {
    if out.len < 8 || layer < 0 || layer >= 48 || expert < 0 || expert >= 512 {
        return -1;
    }
    out[0] = 4;
    out[1] = (layer * 512 + expert) * 4;
    out[2] = 4;
    out[3] = 26;
    out[4] = 1;
    out[5] = 1;
    out[6] = 4;
    out[7] = 2048;
    return 0;
}

export fn e0_route_group(labels: u8[:], layer: i64, expert: i64) -> i64 {
    if labels.len != 98304 || layer < 0 || layer >= 48 || expert < 0 || expert >= 512 {
        return -1;
    }
    let offset: i64 = (layer * 512 + expert) * 4;
    let group: i64 = labels[offset] as i64 + (labels[offset + 1] as i64) * 256 +
                     (labels[offset + 2] as i64) * 65536 + (labels[offset + 3] as i64) * 16777216;
    return if group < 8 { group } else { -1 };
}

export fn e0_scale_desc(layer: i64, out: i64[:]) -> i64 {
    if out.len < 8 || layer < 0 || layer >= 48 {
        return -1;
    }
    out[0] = 100 + layer;
    out[1] = 0;
    out[2] = 5120;
    out[3] = 0;
    out[4] = 1280;
    out[5] = 1;
    out[6] = 5120;
    out[7] = 5120;
    return 0;
}
