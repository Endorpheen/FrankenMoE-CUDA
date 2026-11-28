const BONSAI_KERNELS: u64 = 4824;
const BONSAI_CAPTURE_STREAM: u64 = 15288;
const BONSAI_CAPTURE_NEXT: u64 = 16328;
const BONSAI_LAUNCH_KERNEL: u64 = 144;

type BonsaiKernelLaunch = c_fnptr(u64, u32, u32, u32, u32, u32, u32, u32, u64, u64, u64) -> i32;
extern fn bonsai_bonsai_gdn_packet(input: VocabSpan, tokens: u32, packet: VocabSpan) -> bool;
extern fn bonsai_bonsai_flash_schedule(model: u64, position: u64, count: u64, partial_bytes: u64,
    stage_capacity: u64, flags: u64, pointers: VocabSpan) -> i32;

fn launch_cuda(model: u64, kernel: i32, x: u32, y: u32, z: u32, threads: u32, shared: u32,
    stream: u64, args: u64, label: const u8[:]) -> i32 {
    let function = vocab_word(model + BONSAI_KERNELS + (kernel as! u64) * 8);
    let address = vocab_word(fg_bonsai_cuda_dispatch() + BONSAI_LAUNCH_KERNEL);
    return bonsai_cuda_status(unsafe_c_fnptr<BonsaiKernelLaunch>(address)(
        function, x, y, z, threads, 1, 1, shared, stream, args, 0), label);
}

export fn fg_bonsai_verify_launch(model: u64, kernel: i32, x: u32, y: u32, z: u32, threads: u32, args: u64) -> i32 {
    let phase = batch_u32(model + BATCH_CAPTURE_PHASE);
    var stream: u64 = 0;
    if phase != 0 {
        let nodes: i32[10] = [47, 1, 44, 46, 46, 13, 1, 44, 46, 3];
        let next = batch_u32(model + BONSAI_CAPTURE_NEXT);
        if next >= 10 { return -1; }
        batch_set(model + BONSAI_CAPTURE_NEXT, next + 1);
        if nodes[next as! i64] != kernel { return -1; }
        if phase == 2 { return 0; }
        stream = vocab_word(model + BONSAI_CAPTURE_STREAM);
    }
    return launch_cuda(model, kernel, x, y, z, threads, if kernel == 53 { 50176 } else { 0 }, stream, args, "kernel launch");
}

export fn fg_bonsai_launch_gdn(model: u64, input: u64, tokens: u32) -> i32 {
    var packet: u64[57] = [0; 57];
    let base = vocab_address(unsafe_c_address<c_ptr<const void>>(packet[0]));
    if !bonsai_bonsai_gdn_packet(vocab_span(input, 8), tokens, vocab_span(base, 57)) { return -1; }
    let args = vocab_address(unsafe_c_address<c_ptr<const void>>(base));
    if batch_u32(model + 14688) != 0 && tokens >= 64 {
        return launch_cuda(model, 32, 192, 1, 1, 256, 38016, 0, args, "Bonsai GDN V32");
    }
    return fg_bonsai_verify_launch(model, if tokens > 1 { 15 } else { 6 }, 48, 1, 8, 128, args);
}

export fn bonsai_bonsai_verify_emit_gdn(model: u64, args: VocabSpan) -> i32 {
    return fg_bonsai_launch_gdn(model, args.address, 1);
}

export fn bonsai_bonsai_flash_launch_raw(model: u64, role: u32, x: u32, y: u32,
    threads: u32, shared: u32, args: u64) -> i32 {
    let roles: i32[16] = [61, 62, 63, 25, 56, 22, 53, 43, 42, 30, 27, 24, 29, 28, 26, 23];
    if role >= 16 { return -1; }
    var kernel = roles[role as! i64];
    if batch_u32(model + 4816) != 0 {
        if role == 1 { kernel = 66; }
        if role == 2 { kernel = 67; }
    }
    if role == 2 || role == 4 {
        return launch_cuda(model, kernel, x, y, 1, threads, shared, 0, args,
            if role == 2 { "Bonsai FP8 QK" } else { "Bonsai ordered fused Flash" });
    }
    return fg_bonsai_verify_launch(model, kernel, x, y, 1, threads, args);
}

fn flash_flag(model: u64, offset: u64, flag: u64) -> u64 {
    return if batch_u32(model + offset) != 0 { flag } else { 0 };
}

export fn fg_bonsai_flash_f32(model: u64, q: u64, k: u64, v: u64, dst: u64) -> i32 {
    var pointers: u64[7] = [q, k, v, dst, vocab_word(model + 14712), vocab_word(model + 14720), vocab_word(model + 5696)];
    let flags = flash_flag(model, 14668, 1) | flash_flag(model, 14672, 2) | flash_flag(model, 14676, 4) |
        flash_flag(model, 14680, 8) | flash_flag(model, 14760, 16) | flash_flag(model, 4784, 32) |
        flash_flag(model, 4792, 64) | flash_flag(model, 4808, 128);
    return bonsai_bonsai_flash_schedule(model, batch_u32(model + BATCH_POSITION) as! u64,
        batch_u32(model + BATCH_COUNT) as! u64, vocab_word(model + 14656), batch_u32(model + 14728) as! u64,
        flags, vocab_span(vocab_address(unsafe_c_address<c_ptr<const void>>(pointers[0])), 7));
}

export fn bonsai_ffn_tail_launch(model: u64, gate: i64, up: i64, signs: i64, owner: i64) -> i64 {
    let count = batch_u32(model + BATCH_COUNT);
    if batch_u32(model + 14700) == 0 || count < 32 { return 0; }
    let width: u32 = 17408;
    let width_address = vocab_address(unsafe_c_address<c_ptr<const void>>(width));
    let packed = model + 14640;
    var args: u64[6] = [vocab_address(unsafe_c_address<c_ptr<const void>>(gate)),
        vocab_address(unsafe_c_address<c_ptr<const void>>(up)),
        vocab_address(unsafe_c_address<c_ptr<const void>>(signs)), packed, width_address, 0];
    let address = vocab_address(unsafe_c_address<c_ptr<const void>>(args[0]));
    vocab_set(model + BATCH_DOWN_Q8_OWNER, 0);
    if batch_u32(model + 4756) != 0 && owner != 0 && count % 128 == 0 &&
        (count as! u64) * 136 * 4 <= vocab_word(model + 14656) {
        args[4] = model + 5696;
        args[5] = width_address;
        if fg_bonsai_verify_launch(model, 75, 17, count, 1, 256, address) != 0 { return -1; }
        vocab_set(model + BATCH_DOWN_Q8_OWNER, owner as! u64);
        vocab_set(model + BATCH_PACKED_SOURCE, 0);
        vocab_set(model + BATCH_FFN_Q8_INPUT, 0);
        return 1;
    }
    if fg_bonsai_verify_launch(model, 36, 17, count, 1, 256, address) != 0 { return -1; }
    // The next projection is the only consumer of the rotation output.
    vocab_set(model + BATCH_PACKED_SOURCE, owner as! u64);
    return 1;
}
