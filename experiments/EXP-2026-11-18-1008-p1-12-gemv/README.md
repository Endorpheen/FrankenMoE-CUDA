# EXP-2026-11-18-1008 — P1-12: exact Goose gemv, and why it cannot be accepted yet

Base: `cad70a30`. No production change, no model run, no GPU. Kernel work and measurement only.

## 1. What was delivered here

`src/moe_gemv.goose` is a Goose transcription of the AVX2 body of
`ggml_gemv_q4_K_8x8_q8_K` (`ggml/src/ggml-cpu/arch/x86/repack.cpp:1464`), the
kernel the live Apex Q4_K expert path calls through `forward_mul_mat_id`
(`repack.cpp:4783`, `:4908`).

It reproduces, lane for lane:

- the two-phase nibble extraction (`and` of the raw vector, `and` of the 16-bit
  shift) for both 64-element sub blocks;
- `utmp` unpacking of the 6-bit scales and mins with `0x3f/0x0f/0x03` masks;
- `shuffle_epi8` with the native `deltamask` and `scalemask`;
- `unpacklo_epi8` + `cvtepu8_epi16` for `mins_01`;
- all sixteen `blend_epi32`/`shuffle_epi32`/`maddubs` steps, with `add_epi16`
  wrapping and `madd_epi16` scaling into eight i32 lanes;
- `bsrli_epi128` of `bsums` and `madd_epi16` against `mins_01`;
- one FMA per super block for the value and one for the min term, with
  `col_scale * row_scale` rounded before the fused multiply-add;
- the final `permutevar8x32` and the subtraction of `acc_min`.

ABI, exactly as fixed in EXP1007:

```c
int32_t fg_cpu_simd_row_q4_k_q8_k_x8(fg_cpu_simd_bytes w,   // nc*nb*144, block_q4_Kx8
                                     fg_cpu_simd_bytes a,   // nb*292, block_q8_K
                                     int32_t nb, int32_t nc, // nc multiple of 8
                                     float * out);           // nc values
```

Built with the qualified compiler `8b7a56bb…` and `gcc-13 -O3 -mavx2 -mfma`.

## 2. Bit-exactness against the live kernel

`src/gate_x8.cpp` builds real `block_q4_Kx8` input through the library's own
`CPU_REPACK` buffer, quantizes activations with the library's `quantize_row_q8_K`,
and compares the Goose output with `ggml_gemv_q4_K_8x8_q8_K` bit for bit.

| n | nc | nb | cases | equal | differ |
|---:|---:|---:|---:|---:|---:|
| 512 | 8 / 16 / 64 / 512 | 2 | 128 / 256 / 1024 / 8192 | all | 0 |
| 2048 | 8 / 16 / 64 / 512 | 8 | 128 / 256 / 1024 / 8192 | all | 0 |
| 8192 | 8 / 16 / 64 / 512 | 32 | 128 / 256 / 1024 / 8192 | all | 0 |

Total: 28 800 comparisons, 28 800 bit-identical, max ULP 0, max relative 0.
`evidence-gate-x8.txt` is the raw output.

## 3. Why it cannot be integrated

The generated code is scalar for this kernel. `--packed` reports
`maddubs NONE` for both `packed_simd::maddubs` and `cpu_simd::maddubs`, and the
lane operations the kernel needs (`blend_epi32`, `shuffle_epi32`, `pshufb`,
`hadd_epi16`, `bsrli_epi128`, `permute2f128`) are not in the lowering table, so
they execute as 32-iteration byte loops. The emitted C contains 11 `_mm256`
operations for the whole kernel.

Measured on this host, same data, both arms bit-identical:

| kernel | native | Goose | ratio |
|---|---:|---:|---:|
| Q4_K gemv, n=2048, nc=64, nb=8 | 2.9 us | 791.4 us | 271x |
| Q4_K gemv, n=2048, nc=512, nb=8 | 23.9 us | 6531.2 us | 274x |
| Q4_K row (accepted EXP665 object), n=2048 | 0.05 us | 29.8 us | 576x |
| Q4_K row (accepted EXP665 object), n=512 | 0.02 us | 7.4 us | 383x |

The last two rows matter: the object that EXP665/EXP667 already accepted is
itself 383-576 times slower than the native `vec_dot`. EXP667 recorded
"No performance measurement exists, so no speedup or regression is claimed", so
this was never a hidden fact, only an unmeasured one.

Per token the Apex CPU MoE computes 41 layers x 8 experts x 3 matrices of about
1.05M multiply-accumulates, about 1.03 GMAC. At the measured native 44 GMAC/s per
thread this is a few milliseconds on eight threads and is consistent with the
accepted ~40 tok/s. The same work with the Goose kernel measured here is about
0.16 GMAC/s per thread, i.e. roughly 0.8 s per token before anything else runs.

Wiring this kernel into `forward_mul_mat_id` therefore cannot satisfy P1-12's
acceptance clause "Apex works without confirmed regressions", and the
qwen35moe expert matmul is not a cold path that could absorb it.

## 4. What is blocked and where

Blocker: the qualified Goose compiler `8b7a56bb…` does not lower this kernel's
lane operations to packed x86. Concretely, for `src/moe_gemv.goose` the compiler
emits scalar code for every `blend_epi32`, `shuffle_epi32`, `pshufb`,
`hadd_epi16`, `bsrli_epi128` and `permute2f128`, and it does not lower
`maddubs` at all.

Required to unblock: packed lowering, for this kernel shape, of `pmaddubsw`
(`maddubs`), `pshufb`, `pshufd`, `pblendw`/`vpblendd`, `phaddw`,
`vpsrldq`/lane shift and `vperm2i128`, or an equivalent whole-kernel lowering,
such that the Goose object lands within a small factor of the native kernel.

Reproduction:

```
cd experiments/EXP-2026-11-18-1008-p1-12-gemv
P=/home/end0/AI/Fried-Goose/build/qualified/cpu-simd-rows-8b637cdc7a8c
TMPDIR=$PWD/tmp GOOSE_HOME=$PWD/goose $P/compiler -o build/gemv_smoke.c src/gemv_smoke.goose
TMPDIR=$PWD/tmp gcc-13 -O3 -mavx2 -mfma -fPIC -shared -w -I src src/wrapper.c -o build/libmoe_gemv.so
./build/gate_x8 16      # equivalence gate, exit 0 when all bit-identical
./build/bench_x8 2048 512 2000
./build/bench_row 2048 20000
```

The gate binary is linked against the shipped `libggml-cpu.so.0.27.0`
(`sha256 63ad4984…`), which is the unmodified runtime reference.

## 4a. Ironbeak update (2026-11-18, verified from this side)

The compiler owner delivered `Fried Goose v0.1.3-preview «Ironbeak»`
(`releases/experimental/p112-ironbeak-preview`, binary sha256 `241e3fa4…`,
commit `64afce6a`). `sha256sum -c SHA256SUMS` is OK and its `stdlib/` is
byte-identical to the copy used here.

Rebuilt exactly per the handoff (`build/p112/`, log `build/p112/build.log`):

| check | result |
|---|---|
| emit + build with Ironbeak | rc 0 |
| AVX2 instructions in the object | 64 (`vpmaddubsw`, `vpblendd`, `vpshufb`, `vperm2i128`, …) |
| `gate_x8 16` | **28 800 / 28 800 equal, 0 differ, max ulp 0** |
| `bench_x8 2048 512 2000` | native 24.1 us, Goose 44.5 us, **1.84x**, diffs 0 |

This supersedes the 271x measurement in section 3. The scalar baseline object is
preserved as `build/libmoe_gemv.baseline.so` (sha256 `68ceb389…`), the Ironbeak
object as `build/libmoe_gemv.so` (sha256 `a66cb225…`), and its generated C is
`build/p112/gemv_smoke.c` while `src/gemv_smoke.c` remains the old scalar C.

## 4b. A/B hook, validated without a model

`src/gemv_preload.c` builds `build/libironbeak_hook.so`: a constructor finds
`ggml_gemv_q4_K_8x8_q8_K` in the loaded `libggml-cpu.so.0`, saves 12 bytes and
writes `movabs rax, imm64; jmp rax` to `fg_cpu_simd_row_q4_k_q8_k_x8`; the
destructor restores the bytes. `FG_IRONBEAK_HOOK=0` leaves the library untouched.

Both call sites of the Q4_K gemv pass `nr = 1` (`repack.cpp:4908` and `:4644`),
so the hook handles exactly that case and exits loudly on anything else.

Measured with `bench_x8` under the hook, so the mechanism is proven before any
model run:

| arm | "native" arm | Goose arm | diffs |
|---|---:|---:|---:|
| `FG_IRONBEAK_HOOK=0` (control) | 22.9 us | 46.8 us | 0 |
| `LD_PRELOAD=hook` | 47.8 us (redirected) | 44.5 us | 0 |

plus `IRONBEAK_HOOK first_call n=2048 nc=512 nr=1` and
`IRONBEAK_HOOK restored calls=200`.

## 5. Not done, and why

- Q6_K against the live tiled path: not reached. The delivered Q6_K object is a
  `vec_dot`-order kernel and measures about 383x slower than the native `vec_dot` on
  the same host; the live Q6_K route is the EXP141 tiled kernel, a different
  artifact. Comparing against it does not change the performance blocker above.
- Full Apex A/B: prepared, not started. It needs a model run and therefore
  explicit approval (goal, duration, number of runs) before launch. The hook is
  validated and the command is one line.
- C++ removal: not before the A/B accepts the runtime effect.

No production file, no reference binary and no model were touched.

## 6. Minimal CPU MoE A/B (2026-11-18)

### Profile

The accepted reference EXP906 and its files were not touched. The run used an
isolated copy of the accepted build (`build/accepted-run/`, sha256
`3e4437d9…` for `llama-server`, `33baf1f3…` for `libggml-cpu.so.0`) with two
changes to the *run profile only*:

- `--moe-cache-mib 0` — the GPU expert cache is off, so the experts kept in host
  memory are multiplied on the CPU (`llama-moe-cache.h`: the cache lives in a
  device buffer and requires a GPU backend);
- `GGML_CPU_TILED_MM=0` — the tiled MoE dispatcher returns false
  (`tiled.cpp:572`, "master switch, on by default. If off, we fast return false
  and normal vec_dot mul_mat resumes"), so the stock `vec_dot` path runs.

Request: `requests/req32x8.json`, 32 prompt tokens and `n_predict = 8`
(sha256 `45ddd274…`). Context allocation stays the fixed profile value.

### Counters (both arms really ran CPU MoE)

| arm | proof |
|---|---|
| native | `VECDOT_COUNT_TOTAL 15219483` |
| Goose | `VECDOT_TOTAL 27545753`, first call `n=2048 nrc=1 bx=0 by=0` |

The counters are non-atomic increments shared by eight threads, so their values
are approximate; the point is that both are large and non-zero. Earlier, with
the expert cache on, this same counter was exactly 0 and canary symbols
(`ggml_barrier`, `ggml_row_size`) proved the interception works.

### Correctness

| item | native | Goose |
|---|---|---|
| tokens | `[321, 3019, 364, 279, 16015, 13, 271, 2]` | identical |
| content | `' and plan for the unexpected.\n\n#'` | identical |
| logprobs (8) | `-0.069939815, -2.91748786, …` | bit-identical |
| top-5 per step (40 entries) | — | bit-identical |
| stop | stop | stop |

### Speed and memory (same profile, one request each)

| metric | native | Goose (EXP665 row object) | ratio |
|---|---:|---:|---:|
| prompt tok/s | 44.75 | 1.22 | 36.7x slower |
| predict tok/s | 27.09 | 0.66 | 41.0x slower |
| MemAvailable ready → after | 48,915,360 → 48,645,352 kB | 48,781,624 → 48,789,184 kB | — |
| VRAM ready → after | 4872 → 4894 MiB | 4993 → 4966 MiB | — |

### Which Goose artifact fits

The kernel the stock CPU path executes is `ggml_vec_dot_q4_K_q8_K`, and the Goose
artifact that matches it bit for bit is the accepted EXP665 row object
(`fg_cpu_simd_row_q4_k_q8_k`, `--row-order avx2`), which is what the Goose arm
above delegates to. Ironbeak's `fg_cpu_simd_row_q4_k_q8_k_x8` targets the
8-column `block_q4_Kx8` gemv ABI and does not apply to either the stock
`vec_dot` path or the tiled kernel.

Evidence: `evidence-min/` (both responses, counters, request, summaries).

P1-12 is not closed by this measurement.
