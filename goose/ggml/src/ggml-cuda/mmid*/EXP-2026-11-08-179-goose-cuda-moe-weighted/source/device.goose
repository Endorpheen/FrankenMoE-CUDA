// Apex CUDA MoE weighted reduction (Goose), from
// ggml-cuda/moe-weighted-reduction.cu.
//
// Scope: the whole operator. One GPU thread produces one output element by
// accumulating n_expert_used scaled expert rows:
//   first_row = token * n_expert_used
//   first_scale = expert_scale ? expert_scale[first_row] : 1.0
//   sum = (experts[first_row*n_embd + col] * first_scale) * weights[first_row]
//   for expert in 1..n_expert_used-1:
//       row = first_row + expert
//       scale = expert_scale ? expert_scale[row] : 1.0
//       sum += (experts[row*n_embd + col] * scale) * weights[row]
//   dst[token*n_embd + col] = sum
//
// Everything algorithmic is Goose: the token/column decode, the optional scale
// selection, the row arithmetic, the multiply-add order and the store. Extern
// symbols are only the raw float load and store. No original kernel, GGML struct
// or C++ routine is called.
//
// The multiply order matters for bit-exactness: native computes
// (experts * scale) * weights, not experts * (scale * weights), and the
// accumulation into `sum` is a plain left-to-right fadd chain. Both are preserved.
//
// Raw ABI: u64 values are global byte addresses; offsets below are byte offsets.
// n_embd is the row length in FLOATS.
extern "fg179w_ld_f32" fn ld_f32(src: u64, byte: u64) -> f32;
extern "fg179w_st_f32" fn st_f32(dst: u64, byte: u64, v: f32);
extern "fg179w_fail" fn fail(code: i64);

fn ensure(v: bool, c: i64) {
    if !v {
        fail(c);
    }
}
// One output element per thread.
//   token = blockIdx.x, col = blockIdx.y*blockDim.x + threadIdx.x

fn fg179w_reduce(exp: u64, scale: u64, w: u64, dst: u64, n_embd: i64, n_used: i64, bx: i32, by: i32, tx: i32, nt: i32) {
    ensure(n_embd > 0 && n_used > 0 && nt > 0 && bx >= 0 && by >= 0 && tx >= 0, 10);
    let col: i64 = (by as i64) * (nt as i64) + (tx as i64);

    if col < n_embd {
        let token: i64 = bx as i64;
        let first: i64 = token * n_used;
        var sc0: f32 = 1.0;

        if scale != 0 {
            sc0 = ld_f32(scale, (first * (4 as i64)) as u64);
        }
        let base: i64 = first * n_embd + col;
        var sum: f32 = (ld_f32(exp, (base * (4 as i64)) as u64) * sc0) * ld_f32(w, (first * (4 as i64)) as u64);
        var e: i64 = 1;

        while e < n_used {
            let row: i64 = first + e;
            var sc: f32 = 1.0;

            if scale != 0 {
                sc = ld_f32(scale, (row * (4 as i64)) as u64);
            }
            let o: i64 = row * n_embd + col;
            sum = sum + (ld_f32(exp, (o * (4 as i64)) as u64) * sc) * ld_f32(w, (row * (4 as i64)) as u64);
            e += 1;
        }
        st_f32(dst, ( (token * n_embd + col) * (4 as i64)) as u64, sum);
    }
}
// Retained entry; the parent never executes main. n_embd=0 makes the body return
// before any memory effect.

fn main() {
    let z: u64 = 0;
    fg179w_reduce(z, z, z, z, 0 as i64, 1 as i64, 0, 0, 0, 32 as i32);
}

