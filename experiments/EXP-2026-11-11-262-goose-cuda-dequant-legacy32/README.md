# EXP262: CUDA 32-element legacy dequantization callbacks in Goose

The seven two-value callbacks of `dequantize.cuh`: `dequantize_q1_0`, `q2_0`,
`q4_0`, `q4_1`, `q5_0`, `q5_1`, `q8_0`. They are consumed by the generic
`dequantize_block_cuda<qk, qr, fn>` kernel for the 32-element (and QK1_0/QK2_0)
types. With EXP261 this closes every dequantizer in `dequantize.cuh`.

## Verdict

**OFF_PATH** — bit-exact against the native callbacks, but the fixed 32K/128
Apex profile never selects these types through `ggml_get_to_fp32_cuda`. No
performance claim, shipped DSO unchanged.

## Scope transferred

One module, seven entries. Everything algorithmic is Goose: bit/byte index
arithmetic, 2-bit and nibble splits, the q5 high-bit plane, signed-byte
interpretation for q8_0, the affine reconstructions and both destination stores.
Extern symbols are raw byte/word/half loads and a float store. The pair is
written through the same `float2` interface the native callbacks use.

| type | block | layout |
|---|---:|---|
| q1_0 | 18 | d at 0, qs[16] at 2 (QK1_0 = 128, QR1_0 = 1) |
| q2_0 | 18 | d at 0, qs[16] at 2 (QK2_0 = 64, QR2_0 = 1) |
| q4_0 | 18 | d at 0, qs[16] at 2 |
| q4_1 | 20 | dm at 0, qs[16] at 4 |
| q5_0 | 22 | d at 0, qh[4] at 2, qs[16] at 6 |
| q5_1 | 24 | dm at 0, qh[4] at 4, qs[16] at 8 |
| q8_0 | 34 | d at 0, int8 qs[32] at 2 |

The generic `dequantize_block_cuda` kernel itself is a shared template and is
not part of this module; it belongs with the quantize/get_rows host work.

## Gate

Eight random blocks per type, every valid `iqs` (step 2 for QR1_0/QR2_0/QR8_0,
step 1 for QR2), both components compared bitwise.

| check | result |
|---|---|
| pairs compared | 1408 (2816 float values) |
| failures | 0 |
| launch failures | 0 |

## Three defects in my own work, found before commit

1. **Unaligned 4-byte read.** The native `q5_0`/`q5_1` read `qh` with
   `memcpy`, which is unaligned-safe. My helper did `*(uint32_t*)`; the q5_0 qh
   offset is `22*ib + 2`, so `ib >= 2` is not 4-byte aligned and the kernel
   trapped. Fixed by assembling the word from four byte loads.
2. **`ld_i8` returned `u32`.** Negative q8_0 values became ~4.29e9 after the
   cast to i64. Fixed by returning `i32`; the sign extension is now real.
3. **The fixture could not report failures at all.** A device-side failure
   counter read with `cudaMemcpyFromSymbol` returned "invalid device symbol", so
   every mutation reported 0 failures while the run looked green. Replaced with
   an explicit failure buffer passed as a kernel argument and checked every CUDA
   API return. This was the most dangerous of the three: it made a broken gate
   look like a passing one, and it is the same class as the handoff's "suspect
   the generator first" rule.

## Mutations (after the gate was fixed)

| mutation | failures |
|---|---:|
| q1_0 sign drops the `-1` | 512 |
| q2_0 code offset `-1` removed | 224 |
| q4_0 bias 8 -> 7 | 112 |
| q5_0 qs offset 6 -> 7 | 112 |
| q8_0 signed byte read as unsigned | 59 |

## Not integrated

Same rule as EXP245–261: emitted and gated, deliberately outside the candidate
link.

## Reproduce

    python3 emit_sources.py
    python3 test_local.py
    python3 mutate.py

## Remaining scope

Non-F32 destinations of the dequantizers, activation quantization
(`quantize_mmq_q8_1` is blocked on the emitter address typing, EXP180), and the
MMQ/MUL_MAT families themselves.
