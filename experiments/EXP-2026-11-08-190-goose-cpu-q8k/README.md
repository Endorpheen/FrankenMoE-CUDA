# EXP190: CPU Q8_K activation quantization in Goose

Hypothesis: the CPU MoE expert path contains work that is genuinely scalar, so it can be
transferred to Goose without needing SIMD support the language does not have.

That hypothesis was tested before any porting, and it holds for this function:

```
objdump -d build-cuda/bin/libggml-cpu.so | awk '/<quantize_row_q8_K>:/,/^$/' | grep -c 'ymm|zmm'
0
```

Zero SIMD instructions. The adjacent dot products do not pass this test —
`ggml_vec_dot_q4_K_q8_K` has 58 and `ggml_vec_dot_q6_K_q8_K` has 91 — which is why they
stay native: Goose has no SIMD, and the compiler documentation places SIMD outside the
current scope. `quantize_row_q8_K` is also registered in the CPU type table at
`ggml-cpu.c:393` (`.from_float`), so it is on the executed path with `--cpu-moe`.

## Scope transferred

`quantize_row_q8_K_ref` in full: the per-super-block abs-max scan, the all-zero early
out, the `-127/max` inverse scale, the rounding and clamp to int8, the sixteen per-group
sums, and the `1/iscale` delta. Everything is Goose; the externs are only a float load,
an int8/int16 store, an int8 load, and the rounding and abs primitives.

## Gate

| check | result |
|---|---|
| byte-for-byte `block_q8_K` | **8 checks, 0 failures** |
| sizes | 256, 512, 2560, 4096 elements |
| zero super-block | included, exercises the early-out path |

The comparison is against `quantize_row_q8_K_ref` from the real `libggml-base.so`, not a
hand-written reference, so the whole block struct — `d`, the 256 int8 quants and the 16
int16 group sums — must match exactly.

## Two defects in my own work, found by measurement

1. **Wrong float stride.** I wrote `src + so + j`, but `x` is a float array so element
   `j` is at byte `4*j`. The symptom was telling: `qs` came out as
   `-127 0 0 0 -85 0` — every fourth byte. Fixed.
2. **`nearest_int` is not `roundf`.** Native adds `12582912.f` (1.5*2^23) and masks the
   mantissa, which is round-half-to-even through the FP32 representation. `roundf` is
   round-half-away-from-zero and diverges on exact `.5`. Replicated exactly.

The fixture caught the second only because the byte comparison covers the quant bytes
and not just the block delta.

## Apex execution check: negative, and why

An LD_PRELOAD shim was built (`source/preload.c`) that overrides
`quantize_row_q8_K_ref` -- the symbol `quantize_row_q8_K` jumps to through the PLT -- and
the shim was verified against the fixture first: with it loaded, two calls went to the
Goose module, zero to native, and the output stayed byte-identical.

It was then loaded into a real Apex run. The output was exact (`e68df05b`) but the
witness never fired: `FG190Q_CPU_Q8K` is absent, so the function is not reached on this
profile. `objdump` on the built library finds no call site to `quantize_row_q8_K` at all;
it is registered in the type table but nothing on this path selects it, because the CPU
MoE graph reaches the expert matmul (op=100) and the dense matmul (op=40) directly, which
the EXP143 Goose modules already cover.

So this transfer is byte-exact and reproducible, but like EXP182 and EXP188 it is not
exercised by the Apex profile. Recorded rather than presented as an active improvement.

## What this does and does not claim

This is a real transfer of the algorithm, verified byte-exactly. It makes no
performance claim: the function is a small part of CPU MoE node time next to the AVX2
dot products, and no Apex pair was run for it. The dominant CPU cost —
38,226 Q4_K dot calls at 176.79 us mean CPU-node time per EXP133 — is behind the SIMD
barrier and remains native.

## Remaining CPU scope

The quantized dot-product families (`q4_K`, `q6_K` and the rest), which are SIMD on this
host, and the non-OpenMP expert work distribution. Both need either SIMD in Goose or a
different execution strategy, and neither is available from this side.


## Duplicate of work already done in EXP148

This module transfers `quantize_row_q8_K_ref`, reachable through the `quantize_row_q8_K`
trampoline. Apex does not call the trampoline, and the reason is that the function it
dispatches to has already been replaced.

EXP148 (`EXP-2026-11-07-148-goose-cpu-quant-batch`) installs Goose implementations into the
CPU type-trait table for ten formats. Its `source/quant/unit.c:43` defines

    void fg148_row_q8_K(const float*x,void*y,int64_t n){ if(!enabled||!fg148_encode(9,x,y,n)) quantize_row_q8_K(x,y,n); }

with `kind=9` at width 256 and stride 292 — exactly Q8_K — and the Apex log carries its
witness:

    FG148_QUANT kind=9 calls=456800 values=172640

So Q8_K activation quantization has been running in Goose for this whole migration, and this
module reimplements it on a path nothing takes. The LD_PRELOAD probe confirmed the negative
directly: loaded into a real Apex run, its `quantize_row_q8_K_ref` wrapper never fired, while
the same probe shape worked in a standalone DSO test.

Two lessons, both already learned the hard way once in this project:

1. **Read the executed implementation, not the first match by name.** The Q4_0 case (EXP188)
   had the same shape: the dequantization on the path lives in the attention kernel, not in
   `convert.cu`.
2. **Check the type-trait table before porting a `from_float`.** If a Goose function already
   occupies the slot, the native one is unreachable and a transfer of it proves nothing.

This directory is kept as a record and its build script still runs, but it is not part of the
migration.
