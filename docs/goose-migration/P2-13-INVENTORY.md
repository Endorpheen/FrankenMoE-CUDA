# P2-13 inventory: type registry coverage

Criterion: "Every non-removed type registry entry has Goose quant/dequant/traits where native supports."

## Registry size

`ggml/include/ggml.h` declares 44 `GGML_TYPE_*` entries. Of those, the metadata/integer types (`I8`, `I16`, `I32`, `I64`,
`F64`, `F32`, `F16`, `BF16`) have no quantized block layout, so "quant/dequant/traits" applies to the block types.

## What is covered today

| family | coverage | evidence |
|---|---|---|
| decode (`to_float`) | 23 native-supported public types through the CPY path | EXP143 |
| encode (`from_float`) | 10 kinds: Q1_0, Q2_0, Q4_0, Q4_1, Q5_0, Q5_1, Q8_1, Q8_0, Q8_K and the Q8 special slot | EXP148, table in `source/quant/unit.c:17-42` |
| traits table | installed for those kinds, witness `FG148_QUANT` live | Apex log, 456800 calls / 172640 values |

Two entries have no native public `to_float` at all (Q8_1, Q8_K), so there is nothing to port for them and no oracle to compare
against; that is recorded rather than filled with a fabricated reference.

## Gap list

The block types with a native `to_float`/`from_float` that are not yet in the Goose tables:

- K-quants: Q2_K, Q3_K, Q4_K, Q5_K, Q6_K;
- I-quants: IQ2_XXS, IQ2_XS, IQ2_S, IQ3_XXS, IQ3_S, IQ1_S, IQ1_M, IQ4_NL, IQ4_XS;
- MXFP4, TQ1_0, TQ2_0 and the repack variants Q4_0_4_4/4_8/8_8, IQ4_NL_4_4/4_8/8_8.

Each needs the same treatment as EXP148: a Goose encoder/decoder, a byte-exact gate against the native reference over several
sizes including a degenerate block, and a live witness on a path that actually calls it. The lesson from EXP190 applies first:
read the executed implementation and check the type-trait table before porting a `from_float`, because a duplicate port of an
already-Goosed function is invisible in the counters.

## Why this is not closed yet

The criterion is "where native supports", so the K- and I-quant families above are in scope, and they are exactly the families
whose reference code is SIMD-specialised. Before porting, each must be checked the way EXP190 did: if the executed symbol
contains no vector instructions and is a plain scalar algorithm, it is a clean port; if it is SIMD, it falls under the recorded
compiler-scope exclusion instead of being ported badly.

That decision per family is the remaining work, not the enumeration.

## The K-quant decoders are scalar in the executed build

The EXP190 check was applied to the K-quant family before deciding whether it may be ported. The symbols live in
`ggml/src/ggml-quants.c`, which is linked into `libggml-base` as `base-quants.o`, so that object is the executed implementation:

| symbol | vector instructions in the object |
|---|---|
| `dequantize_row_q2_K` | 0 |
| `dequantize_row_q3_K` | 0 |
| `dequantize_row_q4_K` | 0 |
| `dequantize_row_q5_K` | 0 |
| `dequantize_row_q6_K` | 0 |
| `dequantize_row_iq4_nl` | 0 |

Counting `%xmm`, `%ymm` and `%zmm` over each function body gives zero, so these are plain scalar algorithms in the build that
runs, and they are legitimate port candidates under the same rule that EXP190 used. The compiler-scope exclusion does not apply
to them.

They are also the types Apex actually uses: the model is Q4_K_M, so Q4_K and Q6_K are on the executed path, which makes them the
right family to port first rather than the exotic I-quants.

The next step is the EXP148-shaped port: a Goose decoder for Q4_K (then Q6_K), a byte-exact gate against
`dequantize_row_q4_K` over several sizes including a degenerate super-block, and a live witness on a path that calls it.

## Correction: the authoritative list comes from the executed table

Two errors in the earlier gap list are corrected here.

First, the repack types are **not a gap**: `Q4_0_4_4`, `Q4_0_4_8`, `Q4_0_8_8`, `IQ4_NL_4_4`, `IQ4_NL_4_8` and `IQ4_NL_8_8` are
marked `REMOVED, use ... with runtime repacking` in the type table and have no codec of their own. Listing them as remaining work
was wrong.

Second, the list of types that actually decode is read from the executed `libggml-base` rather than from source text. A probe
calling `ggml_get_type_traits` for every entry reports 27 types with a non-null `to_float`:

    f16 q4_0 q4_1 q5_0 q5_1 q8_0 q2_K q3_K q4_K q5_K q6_K iq2_xxs iq2_xs iq3_xxs iq1_s
    iq4_nl iq3_s iq2_s iq4_xs iq1_m bf16 tq1_0 tq2_0 mxfp4 nvfp4 q1_0 q2_0

Against that list, Goose decoders exist for sixteen types (the five K-quants, IQ4_NL, IQ4_XS, MXFP4, TQ1_0, TQ2_0, IQ2_XXS,
IQ2_XS, IQ2_S, IQ3_XXS, IQ3_S, IQ1_S, IQ1_M), and EXP143 covers the scalar and simple block types through the CPY path (f16,
bf16, q4_0, q4_1, q5_0, q5_1, q8_0, q1_0, q2_0). The one entry with no Goose decoder on either list is **nvfp4**, which uses the
UE4M3 scale and the same doubled e2m1 table as MXFP4.

So the remaining decoder work is exactly one type -- nvfp4 -- plus the encoder direction and the wiring, not seven repack
variants that do not exist.
