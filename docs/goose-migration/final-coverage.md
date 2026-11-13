# Coverage of the kernels Apex actually executes

Derived from `results/exp084-apex-profile/analysis.json`, which lists ten distinct kernels across
the prefill and decode phases of the fixed 32K/128 request. Each is matched to the module that
replaces it and to the witness that proves the module runs.

| kernel | module | witness in one Apex pass | state |
|---|---|---|---|
| `mul_mat_vec_q` (Q4_K) | EXP185 | `FG185Q host=0x21,paths=0x1` | Goose |
| `mul_mat_vec_q` (Q6_K) | EXP186 | — | byte-exact, opt-in, blocked on codegen |
| `flash_attn_ext_vec` | EXP173 | `FG175FA host=0xf,vector=0x7` | Goose |
| `flash_attn_ext_f16` | EXP173 | same witness | Goose |
| `quantize_q8_1` | EXP178 | `FG178Q host=0x1,paths=0x1` | Goose |
| `quantize_mmq_q8_1` | EXP180 | — | byte-exact, opt-in, blocked on codegen |
| `k_get_rows_float_vec` | EXP176 | `FG176G host=0x3,paths=0x3` | Goose |
| `rms_norm_f32` | EXP168 | `FG168 host=0x18,modes=0x18` | Goose |
| `concat_non_cont` | EXP175 | `FG175C host=0x100,paths=0x11` | Goose |
| `mul_mat_f` | EXP184 | `FG184M host=0x21,paths=0x1` | Goose |
| `dequantize_block_q4_0` | EXP188 | absent from the live log | byte-exact; route not reached |

Beyond the profile, fourteen further modules execute and were verified: SET_ROWS, CPY, MoE
weighted reduction, MM ids, FWHT, SSM, GDN, RoPE, top-k, broadcast, activations, and the two pool
modules. Forty witness lines appear together in one log.

## What is not ported, and why

Everything above is either Goose or blocked by one of two walls, both recorded with measurements:

- **Code generation.** `mul_mat_q` Q4_K/Q6_K (~40% of GPU time), `quantize_mmq_q8_1`,
  `mul_mat_vec_q` Q6_K and `mul_mat_f` for the shapes the module declines. Four measured attempts
  moved the instruction count by nothing (504→504) or made it worse (664), and the cause is the
  emitter's `int64_t` address parameters, which stop nvcc folding `base + i*k` into a load's
  immediate offset. Out of bounds for this work: it is a compiler change.
- **No SIMD.** The CPU quantized dot products (`ggml_vec_dot_q4_K_q8_K`, 58 SIMD instructions;
  `ggml_vec_dot_q6_K_q8_K`, 91) cannot be expressed in Goose, which has no SIMD type.

The Q4_0 row dequantizer is the one kernel where the port is byte-exact and the profile still
attributes 20 prefill instances to it while the live selector counter is zero; that is recorded
in `census-status.md` as a stale-profile question, not a port defect.

## Cross-check

`test-backend-ops test -b CUDA0`: rc=0, 0 failures, 2/2 backends. That covers 18753 cases across
the whole operator matrix, not only the ten kernels above, so a module cannot be "covered" for
the profile while wrong elsewhere — which is exactly what the harness found five times.
