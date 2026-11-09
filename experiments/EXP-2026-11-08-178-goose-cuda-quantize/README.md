# EXP178: actual Apex CUDA Q8_1 activation quantization in Goose

Hypothesis: replace the CUDA `quantize_q8_1` kernel and its host planning with
actual Goose for the profile Apex runs, keeping every earlier module and the
native implementation behind one switch. No production or compiler-source
changes. Current state: qualified on the fixed Apex profile and preserved as a
checkpoint, not promoted. P3-07 remains open.

## Verdict

**ACCEPTED** — byte-exact Q8_1 quantize; paired Apex inside threshold.

This is a verdict on the transfer in the experimental runtime, which is the only thing
this work decides. Installation into the production working runtime is left to Igor and
Codex, which is why the state line below says "not promoted" for several stages: it means
"not installed", never "not accepted".

## Scope transferred

`quantize_row_q8_1_cuda` is the non-MMQ Q8_1 activation quantizer, called with
`ids == nullptr`. The MMQ variants (D4/DS4/D2S6 layouts) and the scatter form are
a different kernel and are not part of this module.

| Native | Goose |
|---|---|
| `quantize_q8_1` kernel body | `fg178q_quant` in `device.goose` |
| `ids == nullptr` and `ne0 % QK8_1 == 0` asserts | `run()` in `host.goose` |
| `init_fastdiv_values(ne2)` packing | `fastdiv()` |
| grid `(ceil(ne0/256), ne1, ne2*ne3)`, block 256 | `run()` |

Goose owns the flat index, the fastdivide of `blockIdx.z` into `(i2,i3)`, the
guarded source load, the absolute max, both warp reductions over offsets
16/8/4/2/1, the delta `amax/127`, the int8 quant, the block index and the two half
stores. Extern symbols are only the raw float load, int8/half stores, the
`__shfl_xor_sync` primitive, `roundf`, `fabsf`, `__umulhi` and the PDL primitives.
The only compiler limitation worked around is that the producer rejects a
float→int conversion, so the cast goes through a primitive import, which is the
same lowering the native code uses.

## Defects in my own work, found and fixed

1. `fastdiv` wrote its `<mp, L, divisor>` triple into plan slot 0, which holds the
   `x` and `vy` pointers. The pointers were overwritten before the launch.
2. The launch adapter then read the triple from slot 0 while the host wrote it to
   slot 6 — a slot mismatch that made every fastdiv value garbage. Both sides now
   agree on slot 6.
3. The stream was published as the address of a caller local variable, so the
   adapter dereferenced stale stack memory. It is now a typed packet field.
4. `ld_f32` was given an element index where the extern takes a byte offset, so
   reads landed on misaligned addresses (caught by compute-sanitizer as
   `Address 0x…21 is misaligned`).
5. `objcopy` targeted the `.cold` companion of the mangled symbol instead of the
   callable definition, leaving the Goose entry undefined at link time.

A diagnostic direct-division build was used to prove that defect 2, not the index
arithmetic, was responsible for the missing planes; that change was reverted
before qualification and the fastdiv path is the qualified one.

## Local gate

| check | result |
|---|---|
| `gpu-fixture` Goose | 10 checks / 0 failures / 5 cases |
| `gpu-fixture` native rollback | 10 / 0 / 5 |
| byte-for-byte Goose vs native `block_q8_1` arrays | 5 of 5 identical |
| compute-sanitizer memcheck | `ERROR SUMMARY: 0 errors` |
| export surface vs EXP177 | exactly the two diagnostics added |

Cases cover contiguous rows, a wide row, two planes, and strided sources with
row padding (one- and two-plane, `ne2`/`ne3` both non-unit) so the fastdiv decode
and the stride arithmetic are exercised. Sources include an all-zero row (the
`amax == 0` branch) and a saturated row.

## Apex result

Two opposite-order pairs, only Q8_1 quantization toggled inside one DSO:

| Pair | Native pp/tg | Goose pp/tg | Δ prefill | Δ decode |
|---|---|---|---|---|
| forward | 2627.24 / 25.597 | 2615.33 / 27.046 | -0.453% | +5.661% |
| reverse | (same) | 2614.89 / 25.663 | -0.470% | +0.255% |
| mean | 2627.24 / 25.597 | 2615.11 / 26.354 | **-0.462%** | **+2.957%** |

All three runs return exactly the saved original C++ 128 token IDs, response text
and every returned top-5 probability (sha256 `e68df05b`). Prefill is stable at
-0.46%; the decode improvement is positive in both pairs but its spread is wide,
so it is reported as an observation rather than a speed claim.

Actual execution on the model is witnessed by `FG178Q_CUDA_Q8_1 host=0x1,
cuda_status=0, paths=0x1, guards=0`, with `FG177C_CUDA_CPY host=0x4` and
`FG176G_CUDA_GET_ROWS host=0x3` still active.

## Rollback and reproduce

`GGML_GOOSE_CUDA_Q8_1=0` restores the private-renamed native
`quantize_row_q8_1_cuda`. The immutable EXP177 runtime remains the previous
checkpoint. Reproduce with `python3 emit_sources.py`, `python3 build_runtime.py`,
`python3 test_local.py`, then `FG_STAGE=exp178 FG174_MEASURE_MODE=goose` and
`native-rollback`. `FG178Q_DUMP=<dir>` writes raw `block_q8_1` arrays for an
external byte comparison.

## Remaining scope

Still native: `quantize_mmq_q8_1` with its D4/DS4/D2S6 layouts and scatter form,
the CUDA dequantization module, SET/PAD/ROLL, the transposed CPY tile, quantized
and different-type copies, quantized GET_ROWS and GET_ROWS_BACK. P3-07 and P3-03
therefore stay in progress.
