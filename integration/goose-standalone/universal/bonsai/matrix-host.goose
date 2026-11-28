fn matrix_pack_q8(model: u64, input: u64, groups: u32) -> i32 {
    let source = vocab_word(input + 8);
    if vocab_word(model + BATCH_Q8_INPUT) == source { return 0; }
    let args: u64[3] = [input + 8, model + 14640, vocab_address(unsafe_c_address<c_ptr<const void>>(groups))];
    if elementary_launch(model, 44, (groups + 3) / 4, 1, 128, args) != 0 { return -1; }
    vocab_set(model + BATCH_Q8_INPUT, source);
    return 0;
}

fn bonsai_matrix(model: u64, input: u64, in_width: u32, out_width: u32, count: u32) -> i32 {
    let packed = model + 14640;
    let output_arg = input + 16;
    let output = vocab_word(output_arg);
    let source = vocab_word(input + 8);
    let in_arg = vocab_address(unsafe_c_address<c_ptr<const void>>(in_width));
    let out_arg = vocab_address(unsafe_c_address<c_ptr<const void>>(out_width));
    let tokens_arg = vocab_address(unsafe_c_address<c_ptr<const void>>(count));
    let vocabulary = out_width == 248320;
    let gate = vocab_word(model + BATCH_B + 112);
    let up = vocab_word(model + BATCH_B + 120);
    let down = vocab_word(model + BATCH_B + 144);
    let pq2: u64[5] = [input, packed, output_arg, in_arg, out_arg];
    if count == 1 {
        let ffn = output == gate || output == up || output == down;
        let mode = batch_u32(model + 4712);
        let use_q8 = batch_u32(model + BATCH_IN_DECODE) != 0 && out_width > 1024 &&
            (mode == 1 || (mode == 2 && ffn) || (mode == 3 && vocabulary));
        if use_q8 {
            if matrix_pack_q8(model, input, in_width / 32) != 0 { return -1; }
            let rows: u32 = if vocabulary { 4 } else { 8 };
            return elementary_launch(model, if vocabulary { 45 } else { 46 }, (out_width + rows - 1) / rows, 1, 128, pq2);
        }
        let grouped = batch_u32(model + 14756) != 0 && out_width > 1024;
        let rows: u32 = if grouped { 8 } else { 4 };
        let args: u64[5] = [input, input + 8, output_arg, in_arg, out_arg];
        return elementary_launch(model, if grouped { 41 } else { 2 }, (out_width + rows - 1) / rows, 1, 128, args);
    }
    if fg_bonsai_batch_verify(model, count) != 0 && out_width > 1024 {
        if matrix_pack_q8(model, input, count * (in_width / 32)) != 0 { return -1; }
        var rows: u32 = if vocabulary { 4 } else { 8 };
        var kernel: i32 = if vocabulary { 81 } else { 82 };
        let mtp = batch_u32(model + SESSION_MTP_ENABLED) != 0;
        if count == 4 {
            kernel = if vocabulary { 89 } else { 90 };
            if mtp {
                kernel = if vocabulary { 97 } else { 98 };
                rows = if vocabulary { 8 } else { 16 };
            }
        }
        if count == 5 {
            kernel = if vocabulary { 93 } else { 94 };
            if mtp { kernel = 99; rows = 16; }
        }
        return elementary_launch(model, kernel, (out_width + rows - 1) / rows, 1, 128, pq2);
    }
    let partial = model + 5696;
    let capacity = vocab_word(model + 14656);
    let q8_args: u64[7] = [input, packed, partial, output_arg, in_arg, out_arg, tokens_arg];
    let owner = vocab_word(model + BATCH_DOWN_Q8_OWNER);
    if owner != 0 && source == owner {
        vocab_set(model + BATCH_DOWN_Q8_OWNER, 0);
        if in_width != 17408 || out_width != 5120 || count % 128 != 0 || output != down { return -1; }
        return elementary_launch(model, 76, out_width / 64, count / 128, 256, q8_args);
    }
    let gate_or_up = count >= 128 && in_width == 5120 && out_width == 17408;
    let ffn_gate = gate_or_up && output == gate;
    let ffn_up = gate_or_up && output == up;
    if ffn_gate { batch_set(model + BATCH_FFN_LAYER, batch_u32(model + BATCH_FFN_LAYER) + 1); }
    let ffn_q8 = batch_u32(model + 4724);
    let scales = (count as! u64) * ((in_width / 128) as! u64) * 4;
    if ffn_q8 != 0 && (ffn_gate || ffn_up) && scales <= capacity &&
        (ffn_q8 == 1 || batch_u32(model + BATCH_FFN_LAYER) > 64 - ffn_q8) {
        let full = batch_u32(model + 4728) != 0 && count % 128 == 0;
        let pair = batch_u32(model + 4732) != 0;
        let panel = full && pair && batch_u32(model + 4736) != 0;
        if vocab_word(model + BATCH_FFN_Q8_INPUT) != source {
            let groups = count * (in_width / 128);
            let args: u64[4] = [input + 8, packed, partial, vocab_address(unsafe_c_address<c_ptr<const void>>(groups))];
            if elementary_launch(model, if panel { 69 } else { 49 },
                (groups + 3) / 4, 1, 128, args) != 0 { return -1; }
            vocab_set(model + BATCH_FFN_Q8_INPUT, source);
            vocab_set(model + BATCH_PACKED_SOURCE, 0);
        }
        let tile: u32 = if full { 128 } else { 64 };
        var kernel: i32 = 50;
        if panel { kernel = if batch_u32(model + 4740) != 0 { 71 } else { 70 }; }
        else if full { kernel = if pair { 68 } else { 65 }; }
        return elementary_launch(model, kernel, (out_width + 63) / 64, (count + tile - 1) / tile, 256, q8_args);
    }
    if vocab_word(model + BATCH_PACKED_SOURCE) != source {
        let pairs = count * in_width / 2;
        let args: u64[3] = [input + 8, packed, vocab_address(unsafe_c_address<c_ptr<const void>>(pairs))];
        if elementary_launch(model, 20, (pairs + 255) / 256, 1, 256, args) != 0 { return -1; }
        vocab_set(model + BATCH_PACKED_SOURCE, source);
    }
    let async_mmq = batch_u32(model + 14684) != 0;
    let wide = async_mmq && batch_u32(model + 14692) != 0 && count >= 128;
    let tile: u32 = if wide { 64 } else { 32 };
    let k64 = wide && batch_u32(model + 14704) != 0;
    let cached = batch_u32(model + 4788) != 0;
    let partial_half = k64 && cached && batch_u32(model + 4796) != 0;
    let token_tile: u32 = if k64 && (count >= 512 || partial_half) { 128 } else { tile };
    var kernel: i32 = if wide { 33 } else { if async_mmq { 31 } else { 21 } };
    if partial_half { kernel = 57; }
    else if k64 {
        if cached { kernel = if token_tile == 128 { 55 } else { 54 }; }
        else { kernel = if token_tile == 128 { 38 } else { 37 }; }
    }
    if partial_half && batch_u32(model + 4800) != 0 && out_width % 64 == 0 && count % 128 == 0 {
        if in_width == 5120 { kernel = 58; }
        if in_width == 17408 {
            let half2 = batch_u32(model + 4812) != 0 && out_width == 5120 && output == down;
            kernel = if half2 { 64 } else { 59 };
        }
        if in_width == 6144 && batch_u32(model + 4804) != 0 { kernel = 60; }
    }
    let args: u64[6] = [input, packed, output_arg, in_arg, out_arg, tokens_arg];
    return elementary_launch(model, kernel, (out_width + tile - 1) / tile, (count + token_tile - 1) / token_tile,
        if wide { 256 } else { 128 }, args);
}

export fn bonsai_launch(model: u64, op: i64, a: i64, b: i64, c: i64, d: i64, e: i64, f: i64, g: i64, h: i64) -> i64 {
    let args: i64[8] = [a, b, c, d, e, f, g, h];
    let input = vocab_address(unsafe_c_address<c_ptr<const void>>(args[0]));
    let count = batch_u32(model + BATCH_COUNT);
    if batch_u32(model + BATCH_TXN_PHASE) == 1 {
        if op == 5 { return fg_bonsai_transaction_conv(model, input) as! i64; }
        if op == 6 { return fg_bonsai_transaction_gdn(model, input) as! i64; }
    }
    if fg_bonsai_batch_verify(model, count) != 0 &&
        ((op == 2 && e == 1024) || op == 4 || op == 8 || op == 10) {
        return fg_bonsai_launch_verify_rows(model, op as! i32, input) as! i64;
    }
    if op == 0 || op == 1 { return fg_bonsai_norm_rotate(model, op, input) as! i64; }
    if op == 2 { return bonsai_matrix(model, input, d as! u32, e as! u32, count) as! i64; }
    return fg_bonsai_elementary(model, op, input, c, e) as! i64;
}
