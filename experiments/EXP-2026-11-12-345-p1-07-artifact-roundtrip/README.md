# EXP345: Goose decoders against the artifact -- native encoder, native decoder, Goose decoder

Every decoder ported in the P2-13 series was validated against a fixture I wrote. This experiment removes that dependence for the types
whose quantizer is exported: it quantizes with the **native** encoder, then compares the **native** decoder's output with the **Goose**
decoder's output on exactly those bytes. What is compared is therefore the artifact against the port, with no transcription in between.

## Method

`rt.c` is compiled once per type with `-DQUANT / -DDEQUANT / -DGOOSE / -DQK / -DBLK`, links the type's Goose bridge object and
`libggml-base`, and runs three sizes (one, two and ten super-blocks) with one all-zero leading block. The IQ types that need it call
`ggml_quantize_init` first -- without it the native quantizer asserts, which cost one round and is worth recording.

## Results

| type | checks | failures |
|---|---:|---:|
| Q4_K | 3328 | 0 |
| Q5_K | 3328 | 0 |
| Q6_K | 3328 | 0 |
| Q2_K | 3328 | 0 |
| Q3_K | 3328 | 0 |
| IQ4_NL | 416 | 0 |
| IQ4_XS | 3328 | 0 |
| MXFP4 | 416 | 0 |
| NVFP4 | 832 | 0 |
| TQ1_0 | 3328 | 0 |
| TQ2_0 | 3328 | 0 |
| IQ3_XXS | 3328 | 0 |
| IQ2_S | 3328 | 0 |
| IQ3_S | 3328 | 0 |

Fourteen types, thirty-seven thousand comparisons, zero failures.

## What this does not cover

Four types -- IQ2_XXS, IQ2_XS, IQ1_S, IQ1_M -- have no exported quantizer (only `iq2_s`, `iq3_s`, `iq3_xxs`, `iq4_nl`, `iq4_xs` and
`nvfp4` are exported), so the round trip cannot be driven through the artifact for them. Their decoders are still validated, but against
the native decoder on random bytes, in their own experiments (EXP320, EXP321, EXP325, EXP326); that is a decoder-versus-decoder
differential and is not the same strength as this one.
