// Apex CUDA FWHT (Goose), from ggml-cuda/fwht.cu: fwht_cuda<N, T> for the warp-path
// sizes 64, 128, 256 and 512, which is what the Apex profile executes (the census
// shows fwht_cuda<(int)64, float> and fwht_cuda<(int)256, float>). The wide
// fwht_cuda_block path (N >= 1024) needs block-wide shared staging and stays native.
//
// Everything algorithmic is Goose: the row decode, the register load with the scale,
// the warp-level butterfly built from shfl_xor with the (lane & h) == 0 / else
// add-subtract rule, the in-register butterflies above the warp width, and the store.
// Extern symbols are only the raw element load/store for the two source widths and
// the shfl_xor primitive. No original kernel, GGML struct or C++ routine is called.
//
// Element width: T is float (4 bytes) or half (2 bytes). This module handles FLOAT
// sources; half-source rows stay native and the parent refuses them.
//
// Shape: el_w = N / 32, so N in {64,128,256,512} gives el_w in {2,4,8,16}. The
// register array is fixed at 16 slots and only the first el_w are used, which keeps
// one emitted body for all four sizes.
//
// Raw ABI: u64 values are global byte addresses; row offsets are in ELEMENTS.
extern "fg183f_ld_f32" fn ld_f32(src: u64, byte: u64) -> f32;
extern "fg183f_st_f32" fn st_f32(dst: u64, byte: u64, v: f32);
extern "fg183f_shfl_xor" fn shfl_xor(v: f32, mask: u32, off: i32, width: i32) -> f32;
extern "fg183f_fail" fn fail(code: i64);

fn ensure(v: bool, c: i64) {
    if !v {
        fail(c);
    }
}
// One row per warp. r = blockIdx.x*blockDim.y + threadIdx.y, lane = threadIdx.x.

export fn fg183f_row(src: u64, dst: u64, n: i32, n_rows: i64, scale: f32, bx: i32, by: i32, ty: i32, tx: i32, warp: i32) {
    ensure(n > 0 && (n % warp) == 0, 10);
    ensure(warp == 32, 11);
    let elw: i32 = n / warp;
    ensure(elw >= 1 && elw <= 16, 12);
    let r: i64 = (bx as i64) * (by as i64) + (ty as i64);

    if r < n_rows {
        let sbase: i64 = r * (n as i64);
        let dbase: i64 = r * (n as i64);
        let lane: i32 = tx;
        // register file, 16 slots, first el_w used
        var reg: f32[16] = [0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0];
        var i: i32 = 0;

        while i < elw {
            let o: i64 = (sbase + (i * warp + lane) as i64) * (4 as i64);
            reg[i] = ld_f32(src, (o as u64)) * scale;
            i += 1;
        }
        // stages within the warp: shfl_xor partner at distance h
        var h: i32 = 1;

        while h < warp {
            var j: i32 = 0;

            while j < elw {
                let val: f32 = reg[j];
                let val2: f32 = shfl_xor(val, 4294967295 as u32, h, warp);

                if (lane & h) == 0 {
                    reg[j] = val + val2;
                } else {
                    reg[j] = val2 - val;
                }
                j += 1;
            }
            h = h * 2;
        }
        // stages above the warp width: partner is another register of this thread
        var h2: i32 = warp;

        while h2 < n {
            let step: i32 = h2 / warp;
            var j2: i32 = 0;

            while j2 < elw {
                var k: i32 = 0;

                while k < step {
                    let x: f32 = reg[j2 + k];
                    let y: f32 = reg[j2 + k + step];
                    reg[j2 + k] = x + y;
                    reg[j2 + k + step] = x - y;
                    k += 1;
                }
                j2 = j2 + 2 * step;
            }
            h2 = h2 * 2;
        }
        var m: i32 = 0;

        while m < elw {
            let o: i64 = (dbase + (m * warp + lane) as i64) * (4 as i64);
            st_f32(dst, (o as u64), reg[m]);
            m += 1;
        }
    }
}
// Retained entry; the parent never executes main. n=0 fails the first guard.

extern "fg_fwht_quantize_shared" fn quantize_shared(src: u64, sb: u64, dst: u64, db: u64);
extern "fg_fwht_warp_sync" fn warp_sync();

export fn fwht_q4_write(src: u64, dst: u64, scratch: u64, width: i32, rows: i64,
    scale: f32, row: i64, lane: i32) {
    if row >= rows { return; }
    fg183f_row(src + ((4 * row * width) as u64), scratch, width, 1, scale, 0, 1, 0, lane, 32);
    warp_sync();
    let blocks = width / 32;
    if lane < blocks {
        quantize_shared(scratch, (128 * lane) as u64, dst, (18 * (row * blocks + (lane as i64))) as u64);
    }
}

export fn fwht_row64(src: u64, dst: u64, rows: i64, scale: f32, row: i64, lane: i32) {
    if row >= rows { return; }
    let base = row * 64 + (lane as i64);
    var r0 = ld_f32(src, ((base + 0) * 4) as u64) * scale;
    var r1 = ld_f32(src, ((base + 32) * 4) as u64) * scale;
    let lower_1 = (lane & 1) == 0;
    let peer_1_0 = shfl_xor(r0, 4294967295 as u32, 1, 32);
    r0 = if lower_1 { r0 + peer_1_0 } else { peer_1_0 - r0 };
    let peer_1_1 = shfl_xor(r1, 4294967295 as u32, 1, 32);
    r1 = if lower_1 { r1 + peer_1_1 } else { peer_1_1 - r1 };
    let lower_2 = (lane & 2) == 0;
    let peer_2_0 = shfl_xor(r0, 4294967295 as u32, 2, 32);
    r0 = if lower_2 { r0 + peer_2_0 } else { peer_2_0 - r0 };
    let peer_2_1 = shfl_xor(r1, 4294967295 as u32, 2, 32);
    r1 = if lower_2 { r1 + peer_2_1 } else { peer_2_1 - r1 };
    let lower_4 = (lane & 4) == 0;
    let peer_4_0 = shfl_xor(r0, 4294967295 as u32, 4, 32);
    r0 = if lower_4 { r0 + peer_4_0 } else { peer_4_0 - r0 };
    let peer_4_1 = shfl_xor(r1, 4294967295 as u32, 4, 32);
    r1 = if lower_4 { r1 + peer_4_1 } else { peer_4_1 - r1 };
    let lower_8 = (lane & 8) == 0;
    let peer_8_0 = shfl_xor(r0, 4294967295 as u32, 8, 32);
    r0 = if lower_8 { r0 + peer_8_0 } else { peer_8_0 - r0 };
    let peer_8_1 = shfl_xor(r1, 4294967295 as u32, 8, 32);
    r1 = if lower_8 { r1 + peer_8_1 } else { peer_8_1 - r1 };
    let lower_16 = (lane & 16) == 0;
    let peer_16_0 = shfl_xor(r0, 4294967295 as u32, 16, 32);
    r0 = if lower_16 { r0 + peer_16_0 } else { peer_16_0 - r0 };
    let peer_16_1 = shfl_xor(r1, 4294967295 as u32, 16, 32);
    r1 = if lower_16 { r1 + peer_16_1 } else { peer_16_1 - r1 };
    let left_0_0 = r0;
    r0 = left_0_0 + r1;
    r1 = left_0_0 - r1;
    st_f32(dst, ((base + 0) * 4) as u64, r0);
    st_f32(dst, ((base + 32) * 4) as u64, r1);
}

export fn fwht_row256(src: u64, dst: u64, rows: i64, scale: f32, row: i64, lane: i32) {
    if row >= rows { return; }
    let base = row * 256 + (lane as i64);
    var r0 = ld_f32(src, ((base + 0) * 4) as u64) * scale;
    var r1 = ld_f32(src, ((base + 32) * 4) as u64) * scale;
    var r2 = ld_f32(src, ((base + 64) * 4) as u64) * scale;
    var r3 = ld_f32(src, ((base + 96) * 4) as u64) * scale;
    var r4 = ld_f32(src, ((base + 128) * 4) as u64) * scale;
    var r5 = ld_f32(src, ((base + 160) * 4) as u64) * scale;
    var r6 = ld_f32(src, ((base + 192) * 4) as u64) * scale;
    var r7 = ld_f32(src, ((base + 224) * 4) as u64) * scale;
    let lower_1 = (lane & 1) == 0;
    let peer_1_0 = shfl_xor(r0, 4294967295 as u32, 1, 32);
    r0 = if lower_1 { r0 + peer_1_0 } else { peer_1_0 - r0 };
    let peer_1_1 = shfl_xor(r1, 4294967295 as u32, 1, 32);
    r1 = if lower_1 { r1 + peer_1_1 } else { peer_1_1 - r1 };
    let peer_1_2 = shfl_xor(r2, 4294967295 as u32, 1, 32);
    r2 = if lower_1 { r2 + peer_1_2 } else { peer_1_2 - r2 };
    let peer_1_3 = shfl_xor(r3, 4294967295 as u32, 1, 32);
    r3 = if lower_1 { r3 + peer_1_3 } else { peer_1_3 - r3 };
    let peer_1_4 = shfl_xor(r4, 4294967295 as u32, 1, 32);
    r4 = if lower_1 { r4 + peer_1_4 } else { peer_1_4 - r4 };
    let peer_1_5 = shfl_xor(r5, 4294967295 as u32, 1, 32);
    r5 = if lower_1 { r5 + peer_1_5 } else { peer_1_5 - r5 };
    let peer_1_6 = shfl_xor(r6, 4294967295 as u32, 1, 32);
    r6 = if lower_1 { r6 + peer_1_6 } else { peer_1_6 - r6 };
    let peer_1_7 = shfl_xor(r7, 4294967295 as u32, 1, 32);
    r7 = if lower_1 { r7 + peer_1_7 } else { peer_1_7 - r7 };
    let lower_2 = (lane & 2) == 0;
    let peer_2_0 = shfl_xor(r0, 4294967295 as u32, 2, 32);
    r0 = if lower_2 { r0 + peer_2_0 } else { peer_2_0 - r0 };
    let peer_2_1 = shfl_xor(r1, 4294967295 as u32, 2, 32);
    r1 = if lower_2 { r1 + peer_2_1 } else { peer_2_1 - r1 };
    let peer_2_2 = shfl_xor(r2, 4294967295 as u32, 2, 32);
    r2 = if lower_2 { r2 + peer_2_2 } else { peer_2_2 - r2 };
    let peer_2_3 = shfl_xor(r3, 4294967295 as u32, 2, 32);
    r3 = if lower_2 { r3 + peer_2_3 } else { peer_2_3 - r3 };
    let peer_2_4 = shfl_xor(r4, 4294967295 as u32, 2, 32);
    r4 = if lower_2 { r4 + peer_2_4 } else { peer_2_4 - r4 };
    let peer_2_5 = shfl_xor(r5, 4294967295 as u32, 2, 32);
    r5 = if lower_2 { r5 + peer_2_5 } else { peer_2_5 - r5 };
    let peer_2_6 = shfl_xor(r6, 4294967295 as u32, 2, 32);
    r6 = if lower_2 { r6 + peer_2_6 } else { peer_2_6 - r6 };
    let peer_2_7 = shfl_xor(r7, 4294967295 as u32, 2, 32);
    r7 = if lower_2 { r7 + peer_2_7 } else { peer_2_7 - r7 };
    let lower_4 = (lane & 4) == 0;
    let peer_4_0 = shfl_xor(r0, 4294967295 as u32, 4, 32);
    r0 = if lower_4 { r0 + peer_4_0 } else { peer_4_0 - r0 };
    let peer_4_1 = shfl_xor(r1, 4294967295 as u32, 4, 32);
    r1 = if lower_4 { r1 + peer_4_1 } else { peer_4_1 - r1 };
    let peer_4_2 = shfl_xor(r2, 4294967295 as u32, 4, 32);
    r2 = if lower_4 { r2 + peer_4_2 } else { peer_4_2 - r2 };
    let peer_4_3 = shfl_xor(r3, 4294967295 as u32, 4, 32);
    r3 = if lower_4 { r3 + peer_4_3 } else { peer_4_3 - r3 };
    let peer_4_4 = shfl_xor(r4, 4294967295 as u32, 4, 32);
    r4 = if lower_4 { r4 + peer_4_4 } else { peer_4_4 - r4 };
    let peer_4_5 = shfl_xor(r5, 4294967295 as u32, 4, 32);
    r5 = if lower_4 { r5 + peer_4_5 } else { peer_4_5 - r5 };
    let peer_4_6 = shfl_xor(r6, 4294967295 as u32, 4, 32);
    r6 = if lower_4 { r6 + peer_4_6 } else { peer_4_6 - r6 };
    let peer_4_7 = shfl_xor(r7, 4294967295 as u32, 4, 32);
    r7 = if lower_4 { r7 + peer_4_7 } else { peer_4_7 - r7 };
    let lower_8 = (lane & 8) == 0;
    let peer_8_0 = shfl_xor(r0, 4294967295 as u32, 8, 32);
    r0 = if lower_8 { r0 + peer_8_0 } else { peer_8_0 - r0 };
    let peer_8_1 = shfl_xor(r1, 4294967295 as u32, 8, 32);
    r1 = if lower_8 { r1 + peer_8_1 } else { peer_8_1 - r1 };
    let peer_8_2 = shfl_xor(r2, 4294967295 as u32, 8, 32);
    r2 = if lower_8 { r2 + peer_8_2 } else { peer_8_2 - r2 };
    let peer_8_3 = shfl_xor(r3, 4294967295 as u32, 8, 32);
    r3 = if lower_8 { r3 + peer_8_3 } else { peer_8_3 - r3 };
    let peer_8_4 = shfl_xor(r4, 4294967295 as u32, 8, 32);
    r4 = if lower_8 { r4 + peer_8_4 } else { peer_8_4 - r4 };
    let peer_8_5 = shfl_xor(r5, 4294967295 as u32, 8, 32);
    r5 = if lower_8 { r5 + peer_8_5 } else { peer_8_5 - r5 };
    let peer_8_6 = shfl_xor(r6, 4294967295 as u32, 8, 32);
    r6 = if lower_8 { r6 + peer_8_6 } else { peer_8_6 - r6 };
    let peer_8_7 = shfl_xor(r7, 4294967295 as u32, 8, 32);
    r7 = if lower_8 { r7 + peer_8_7 } else { peer_8_7 - r7 };
    let lower_16 = (lane & 16) == 0;
    let peer_16_0 = shfl_xor(r0, 4294967295 as u32, 16, 32);
    r0 = if lower_16 { r0 + peer_16_0 } else { peer_16_0 - r0 };
    let peer_16_1 = shfl_xor(r1, 4294967295 as u32, 16, 32);
    r1 = if lower_16 { r1 + peer_16_1 } else { peer_16_1 - r1 };
    let peer_16_2 = shfl_xor(r2, 4294967295 as u32, 16, 32);
    r2 = if lower_16 { r2 + peer_16_2 } else { peer_16_2 - r2 };
    let peer_16_3 = shfl_xor(r3, 4294967295 as u32, 16, 32);
    r3 = if lower_16 { r3 + peer_16_3 } else { peer_16_3 - r3 };
    let peer_16_4 = shfl_xor(r4, 4294967295 as u32, 16, 32);
    r4 = if lower_16 { r4 + peer_16_4 } else { peer_16_4 - r4 };
    let peer_16_5 = shfl_xor(r5, 4294967295 as u32, 16, 32);
    r5 = if lower_16 { r5 + peer_16_5 } else { peer_16_5 - r5 };
    let peer_16_6 = shfl_xor(r6, 4294967295 as u32, 16, 32);
    r6 = if lower_16 { r6 + peer_16_6 } else { peer_16_6 - r6 };
    let peer_16_7 = shfl_xor(r7, 4294967295 as u32, 16, 32);
    r7 = if lower_16 { r7 + peer_16_7 } else { peer_16_7 - r7 };
    let left_0_0 = r0;
    r0 = left_0_0 + r1;
    r1 = left_0_0 - r1;
    let left_0_2 = r2;
    r2 = left_0_2 + r3;
    r3 = left_0_2 - r3;
    let left_0_4 = r4;
    r4 = left_0_4 + r5;
    r5 = left_0_4 - r5;
    let left_0_6 = r6;
    r6 = left_0_6 + r7;
    r7 = left_0_6 - r7;
    let left_1_0 = r0;
    r0 = left_1_0 + r2;
    r2 = left_1_0 - r2;
    let left_1_1 = r1;
    r1 = left_1_1 + r3;
    r3 = left_1_1 - r3;
    let left_1_4 = r4;
    r4 = left_1_4 + r6;
    r6 = left_1_4 - r6;
    let left_1_5 = r5;
    r5 = left_1_5 + r7;
    r7 = left_1_5 - r7;
    let left_2_0 = r0;
    r0 = left_2_0 + r4;
    r4 = left_2_0 - r4;
    let left_2_1 = r1;
    r1 = left_2_1 + r5;
    r5 = left_2_1 - r5;
    let left_2_2 = r2;
    r2 = left_2_2 + r6;
    r6 = left_2_2 - r6;
    let left_2_3 = r3;
    r3 = left_2_3 + r7;
    r7 = left_2_3 - r7;
    st_f32(dst, ((base + 0) * 4) as u64, r0);
    st_f32(dst, ((base + 32) * 4) as u64, r1);
    st_f32(dst, ((base + 64) * 4) as u64, r2);
    st_f32(dst, ((base + 96) * 4) as u64, r3);
    st_f32(dst, ((base + 128) * 4) as u64, r4);
    st_f32(dst, ((base + 160) * 4) as u64, r5);
    st_f32(dst, ((base + 192) * 4) as u64, r6);
    st_f32(dst, ((base + 224) * 4) as u64, r7);
}

export fn fwht_q4_write64(src: u64, dst: u64, scratch: u64, rows: i64, scale: f32, row: i64, lane: i32) {
    if row >= rows { return; }
    fwht_row64(src + ((4 * row * 64) as u64), scratch, 1, scale, 0, lane);
    warp_sync();
    if lane < 2 {
        quantize_shared(scratch, (128 * lane) as u64, dst, (18 * (row * 2 + (lane as i64))) as u64);
    }
}

export fn fwht_q4_write256(src: u64, dst: u64, scratch: u64, rows: i64, scale: f32, row: i64, lane: i32) {
    if row >= rows { return; }
    fwht_row256(src + ((4 * row * 256) as u64), scratch, 1, scale, 0, lane);
    warp_sync();
    if lane < 8 {
        quantize_shared(scratch, (128 * lane) as u64, dst, (18 * (row * 8 + (lane as i64))) as u64);
    }
}

export fn apex_fwht_row64(src: u64, dst: u64, rows: i64, scale: f32, block_index: i64, thread: i64) {
    fwht_row64(src, dst, rows, scale, 4 * block_index + thread / 32, (thread % 32) as i32);
}

export fn apex_fwht_row256(src: u64, dst: u64, rows: i64, scale: f32, block_index: i64, thread: i64) {
    fwht_row256(src, dst, rows, scale, 4 * block_index + thread / 32, (thread % 32) as i32);
}

export fn apex_fwht_q4_64(src: u64, dst: u64, rows: i64, scale: f32, block_index: i64, thread: i64, scratch: u64) {
    let warp = thread / 32;
    fwht_q4_write64(src, dst, scratch + 256 * (warp as u64), rows, scale, 4 * block_index + warp, (thread % 32) as i32);
}

export fn apex_fwht_q4_256(src: u64, dst: u64, rows: i64, scale: f32, block_index: i64, thread: i64, scratch: u64) {
    let warp = thread / 32;
    fwht_q4_write256(src, dst, scratch + 1024 * (warp as u64), rows, scale, 4 * block_index + warp, (thread % 32) as i32);
}
