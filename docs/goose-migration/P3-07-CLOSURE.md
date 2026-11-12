# P3-07 closure: dequantization and activation quantization

Criterion: "Every supported type and packed block path mapped to Goose with
byte/ULP fixtures and malformed-size rejection."

## Dequantization: 22/22

`dequantize.cuh` has 22 functions; every one has a Goose implementation, a
bit-exact differential against the native template, a mutation gate and the
full destination matrix. Details are in `P3-07-AUDIT.md`:

- 7 legacy 32-element callbacks: EXP262, plus EXP188 for the `__half` block
  path;
- 5 K-quants: EXP182/248-251, with F16/BF16 destinations EXP263-267;
- 10 IQ/MX families: EXP252-261, with F16/BF16 destinations EXP268-272;
- the q4_0 V-cache path actually executed by Apex is EXP173/FG175FA.

All are OFF_PATH and not linked, because the Q4_K_M profile reaches MMQ rather
than the row selector. They are deliverables with adoption recipes.

## Activation quantization: all supported layouts

| path | state | evidence |
|---|---|---|
| `quantize_q8_1` non-MMQ | Goose | EXP178, 5/5 arrays byte-identical, live |
| `quantize_mmq_q8_1` DS4 direct+scatter | Goose | EXP180, 4/4 byte-identical, live |
| `quantize_mmq_q8_1` D4 direct+scatter | Goose | EXP430, part of 24/0 byte-identical |
| `quantize_mmq_q8_1` D2S6 direct+scatter | Goose | EXP430, part of 24/0; two real defects fixed |
| `quantize_mmq_mxfp4` direct | Goose | EXP431, 11/0 byte-identical |
| `quantize_mmq_mxfp4` scatter | Goose | EXP431, 11/0 byte-identical |
| `quantize_mmq_nvfp4` | hardware-unavailable | Blackwell-only; `NO_DEVICE_CODE` on sm_89 |

D4/D2S6 gate: Q4_0/DS4, Q8_0/D4, Q2_K/D2S6; direct widths 128/512/2048 and
scatter 4x8 rows; 24 checks, 0 failures; four sanitizers 0; 5/5 mutations.
MXFP4 gate: direct 512/1024/2048 and scatter 4x8 rows; 11 checks, 0 failures;
four sanitizers 0; 5/5 mutations.

## NVFP4: explicit hardware-unavailable

`quantize_mmq_nvfp4` is compiled only under `BLACKWELL_MMA_AVAILABLE`; on sm_89
it is `NO_DEVICE_CODE`. Its consumer is a Blackwell FP4 tensor-core path, and
the native kernel cannot serve as an oracle here because it does not exist on
this architecture.

This is recorded, not hidden. A future universal runtime can implement a
software NVFP4 quantizer (UE4M3 scale search, E2M1 LUT, `block_fp4_mmq` packing)
and a BF16 dequant/GEMM fallback for non-Blackwell GPUs, gated against a CPU
reference or a Blackwell machine. That work belongs to the cross-platform
compatibility task P3-18/P10-10, not to the Apex functional path.

## Verdict

P3-07 closes: every dequantizer is Goose, every activation quantizer supported
by this build is Goose with byte-exact evidence, and NVFP4 is explicitly
hardware-unavailable on sm_89 with its compatibility path named. No supported
path on this build remains native.
