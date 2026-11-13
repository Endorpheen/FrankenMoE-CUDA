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

fn fg183f_row(src: u64, dst: u64, n: i32, n_rows: i64, scale: f32, bx: i32, by: i32, ty: i32, tx: i32, warp: i32) {
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

fn main() {
    let z: u64 = 0;
    fg183f_row(z, z, 0 as i32, 0 as i64, 0.0, 0, 0, 0, 0, 32 as i32);
}

