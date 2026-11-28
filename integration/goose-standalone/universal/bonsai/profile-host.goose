fn bonsai_profile_env(name: const u8[:]) -> u64 {
    return fg_cuda_model_getenv(vocab_text(name).address);
}

fn bonsai_profile_is(value: u64, expected: const u8[:]) -> bool {
    if value == 0 { return false; }
    for index in expected.len {
        let byte = vocab_byte(value + (index as! u64));
        if byte != expected[index] { return false; }
    }
    return vocab_byte(value + (expected.len as! u64)) == 0;
}

export fn fg_bonsai_profile(model: u64, mtp_image: u64) -> i32 {
    if mtp_image == 0 || vocab_byte(mtp_image) != 0 {
        batch_set(model + 16, 1);
        batch_set(model + 56, 1);
        batch_set(model + 60, 1);
        batch_set(model + 76, 1);
        batch_set(model + 64, 1);
        batch_set(model + 68, 1);
        batch_set(model + 72, 1);
        batch_set(model + 84, 1);
        batch_set(model + 52, 4);
    }
    let flag = bonsai_profile_env("BONSAI_FLASH_F32\0");
    batch_set(model + 14664, if flag != 0 && bonsai_profile_is(flag, "1") { 1 } else { 0 });
    let stage = bonsai_profile_env("BONSAI_FLASH_REUSE\0");
    batch_set(model + 14668, if batch_u32(model + 14664) != 0 && stage != 0 && bonsai_profile_is(stage, "1") { 1 } else { 0 });
    let merge = bonsai_profile_env("BONSAI_FLASH_ORDERED_MERGE\0");
    batch_set(model + 14672, if batch_u32(model + 14668) != 0 && merge != 0 && bonsai_profile_is(merge, "1") { 1 } else { 0 });
    let compact = bonsai_profile_env("BONSAI_FLASH_COMPACT\0");
    batch_set(model + 14676, if batch_u32(model + 14668) != 0 && compact != 0 && bonsai_profile_is(compact, "1") { 1 } else { 0 });
    let frags = bonsai_profile_env("BONSAI_FLASH_ASYNC\0");
    batch_set(model + 14680, if batch_u32(model + 14676) != 0 && frags != 0 && bonsai_profile_is(frags, "1") { 1 } else { 0 });
    if batch_u32(model + 14680) != 0 { vocab_log("Flash fragments: async staging and ldmatrix\n"); }
    let panel = bonsai_profile_env("BONSAI_FLASH_KV_PANEL\0");
    batch_set(model + 4784, if panel == 0 || bonsai_profile_is(panel, "1") { 1 } else { 0 });
    let reuse = bonsai_profile_env("BONSAI_FLASH_TILE_REUSE\0");
    batch_set(model + 14760, if reuse == 0 || bonsai_profile_is(reuse, "1") { 1 } else { 0 });
    if batch_u32(model + 14680) != 0 && batch_u32(model + 14760) != 0 { vocab_log("Flash: reuse Q across KV tiles\n"); }
    let mmq = bonsai_profile_env("BONSAI_PQ2_MMQ_ASYNC\0");
    batch_set(model + 14684, if mmq == 0 || bonsai_profile_is(mmq, "1") { 1 } else { 0 });
    if batch_u32(model + 14684) != 0 { vocab_log("PQ2 MMQ: async activation and warp fragment loads\n"); }
    let gdn_mode = bonsai_profile_env("BONSAI_GDN_V32\0");
    batch_set(model + 14688, if gdn_mode == 0 || bonsai_profile_is(gdn_mode, "1") { 1 } else { 0 });
    if batch_u32(model + 14688) != 0 { vocab_log("Prefill GDN: frozen Goose MMA V32\n"); }
    let wide = bonsai_profile_env("BONSAI_PQ2_MMQ_WIDE\0");
    batch_set(model + 14692, if wide == 0 || bonsai_profile_is(wide, "1") { 1 } else { 0 });
    if batch_u32(model + 14692) != 0 { vocab_log("PQ2 MMQ: wide tiles for 128 or more tokens\n"); }
    let kchunk = bonsai_profile_env("BONSAI_PQ2_MMQ_K64\0");
    batch_set(model + 14704, if kchunk == 0 || bonsai_profile_is(kchunk, "1") { 1 } else { 0 });
    if batch_u32(model + 14704) != 0 { vocab_log("PQ2 MMQ: K64 exact wide tiles\n"); }
    let half2_down = bonsai_profile_env("BONSAI_MMQ_HALF2_DOWN\0");
    batch_set(model + 4812, if half2_down == 0 || !bonsai_profile_is(half2_down, "0") { 1 } else { 0 });
    let full6144 = bonsai_profile_env("BONSAI_MMQ_HALF_FULL6144\0");
    batch_set(model + 4804, if full6144 == 0 || !bonsai_profile_is(full6144, "0") { 1 } else { 0 });
    let full = bonsai_profile_env("BONSAI_MMQ_HALF_FULL\0");
    batch_set(model + 4800, if full == 0 || !bonsai_profile_is(full, "0") { 1 } else { 0 });
    let half_acc = bonsai_profile_env("BONSAI_MMQ_HALF_ACC\0");
    batch_set(model + 4796, if half_acc != 0 && bonsai_profile_is(half_acc, "1") { 1 } else { 0 });
    let cached = bonsai_profile_env("BONSAI_MMQ_CACHED_PQ2\0");
    batch_set(model + 4788, if cached == 0 || bonsai_profile_is(cached, "1") { 1 } else { 0 });
    if batch_u32(model + 14704) != 0 && batch_u32(model + 4788) != 0 { vocab_log("PQ2 MMQ: reuse packed weight scales\n"); }
    let small = bonsai_profile_env("BONSAI_DECAY_SMALL\0");
    batch_set(model + 4780, if small == 0 || bonsai_profile_is(small, "1") { 1 } else { 0 });
    if batch_u32(model + 4780) != 0 { vocab_log("Bonsai decay: staged single-token products\n"); }
    let decay = bonsai_profile_env("BONSAI_DECAY_REUSE\0");
    batch_set(model + 14696, if decay == 0 || bonsai_profile_is(decay, "1") { 1 } else { 0 });
    if batch_u32(model + 14696) != 0 { vocab_log("Batch decay: reuse alpha/beta weights\n"); }
    let tail = bonsai_profile_env("BONSAI_FFN_TAIL\0");
    batch_set(model + 14700, if tail == 0 || bonsai_profile_is(tail, "1") { 1 } else { 0 });
    if batch_u32(model + 14700) != 0 { vocab_log("FFN tail: fused SwiGLU, FWHT and FP16 packing\n"); }
    if batch_u32(model + 14676) != 0 { vocab_log("Flash query layout: compact 6:1\n"); }
    if batch_u32(model + 14672) != 0 { vocab_log("Flash merge: ordered parallel coefficients\n"); }
    vocab_log(str("Prefill attention: ", if batch_u32(model + 14664) != 0 { if batch_u32(model + 14668) != 0 { "staged Bonsai FP32 Flash" } else { "direct Bonsai FP32 Flash" } } else { "tiled Q4" }, "\n"));
    let decode_merge = bonsai_profile_env("BONSAI_DECODE_ORDERED_MERGE\0");
    batch_set(model + 4748, if decode_merge != 0 && bonsai_profile_is(decode_merge, "1") { 1 } else { 0 });
    let decode_half2 = bonsai_profile_env("BONSAI_DECODE_DIRECT_HALF2\0");
    batch_set(model + 4752, if decode_half2 != 0 && bonsai_profile_is(decode_half2, "1") { 1 } else { 0 });
    let mma = bonsai_profile_env("BONSAI_DECODE_DIRECT_MMA\0");
    batch_set(model + 4744, if bonsai_profile_is(mma, "1") { 1 } else { if bonsai_profile_is(mma, "diagnostic") { 2 } else { 0 } });
    if batch_u32(model + 4744) != 0 { vocab_log("Bonsai single-query attention: direct Q4 MMA\n"); }
    let grouped = bonsai_profile_env("BONSAI_DECODE_GQA\0");
    batch_set(model + 14752, if grouped == 0 || bonsai_profile_is(grouped, "1") { 1 } else { 0 });
    if batch_u32(model + 14752) != 0 { vocab_log("Decode attention: grouped Q4 K/V\n"); }
    let q8 = bonsai_profile_env("BONSAI_DECODE_Q8\0");
    var q8_mode: u32 = 1;
    if q8 != 0 {
        if bonsai_profile_is(q8, "off") { q8_mode = 0; }
        else if bonsai_profile_is(q8, "full") { q8_mode = 1; }
        else if bonsai_profile_is(q8, "ffn") { q8_mode = 2; }
        else if bonsai_profile_is(q8, "vocab") { q8_mode = 3; }
        else { return -1; }
    }
    batch_set(model + 4712, q8_mode);
    if q8_mode != 0 {
        let mode = if q8_mode == 1 { "full" } else { if q8_mode == 2 { "ffn" } else { "vocab" } };
        vocab_log(str("Bonsai Q8 decode: ", mode, "\n"));
    }
    let norm = bonsai_profile_env("BONSAI_WIDE_NORM\0");
    batch_set(model + 4720, if norm == 0 || bonsai_profile_is(norm, "1") { 1 } else { 0 });
    if batch_u32(model + 4720) != 0 { vocab_log("Bonsai RMSNorm: parallel width 5120\n"); }
    let full_q8 = bonsai_profile_env("BONSAI_FFN_Q8_FULL128\0");
    batch_set(model + 4728, if full_q8 == 0 || !bonsai_profile_is(full_q8, "0") { 1 } else { 0 });
    let spread = bonsai_profile_env("BONSAI_FFN_Q8_SPREAD\0");
    batch_set(model + 4740, if spread != 0 && bonsai_profile_is(spread, "1") { 1 } else { 0 });
    let down_q8 = bonsai_profile_env("BONSAI_FFN_DOWN_Q8_FUSED\0");
    batch_set(model + 4756, if down_q8 != 0 && bonsai_profile_is(down_q8, "1") { 1 } else { 0 });
    if batch_u32(model + 4756) != 0 { vocab_log("Bonsai prefill: fused Q8 FFN down\n"); }
    let q8_panel = bonsai_profile_env("BONSAI_FFN_Q8_PANEL\0");
    batch_set(model + 4736, if q8_panel != 0 && bonsai_profile_is(q8_panel, "1") { 1 } else { 0 });
    if batch_u32(model + 4736) != 0 { vocab_log("Bonsai prefill: panel-major Q8 activations\n"); }
    let pairload = bonsai_profile_env("BONSAI_FFN_Q8_PAIRLOAD\0");
    batch_set(model + 4732, if pairload != 0 && bonsai_profile_is(pairload, "1") { 1 } else { 0 });
    if batch_u32(model + 4732) != 0 { vocab_log("Bonsai prefill: paired Q8 weight loads\n"); }
    let ffn = bonsai_profile_env("BONSAI_FFN_Q8_PREFILL\0");
    var ffn_mode: u32 = 16;
    if ffn != 0 {
        if bonsai_profile_is(ffn, "0") { ffn_mode = 0; }
        else if bonsai_profile_is(ffn, "1") { ffn_mode = 1; }
        else if bonsai_profile_is(ffn, "last16") { ffn_mode = 16; }
        else if bonsai_profile_is(ffn, "last32") { ffn_mode = 32; }
        else { return -1; }
    }
    batch_set(model + 4724, ffn_mode);
    if ffn_mode != 0 { vocab_log("Bonsai prefill: Q8 FFN gate/up\n"); }
    let mode = bonsai_profile_env("BONSAI_MATVEC\0");
    let rows2 = mode == 0 || bonsai_profile_is(mode, "rows2");
    batch_set(model + 14756, if rows2 { 1 } else { 0 });
    var kernel: const u8[:] = "bonsai_pq2_matvec_ordered\0";
    if !rows2 && !bonsai_profile_is(mode, "ordered") {
        if bonsai_profile_is(mode, "exchange") { kernel = "bonsai_pq2_matvec_exchange\0"; }
        else if bonsai_profile_is(mode, "scalar") { kernel = "bonsai_pq2_matvec\0"; }
        else {
            vocab_log("Invalid BONSAI_MATVEC mode\n");
            return -1;
        }
    }
    let name = vocab_text(kernel);
    vocab_set(model + 4696, name.address);
    vocab_log(str("Decode matrix: ", if rows2 { "bonsai_pq2_matvec_rows2" } else { kernel[0 .. kernel.len - 1] }, "\n"));
    return 0;
}
