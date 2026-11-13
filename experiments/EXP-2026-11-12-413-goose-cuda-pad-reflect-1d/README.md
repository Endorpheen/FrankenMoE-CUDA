# EXP413: CUDA `pad_reflect_1d` in Goose

`pad_reflect_1d.cu` was the last kernel of the P3-03 family without a Goose
implementation: CONCAT, GET_ROWS, CPY, SET_ROWS, SET, PAD and ROLL already had
one. This experiment ports the device index/reflection arithmetic and the host
launch plan, then gates both against the native code.

## What was ported

Device (`source/device.goose`):

| native | Goose |
|---|---|
| `(i3, i2, i1, i0)` decode from blockIdx/threadIdx | `pad_elem()` |
| `fast_div_modulo(blockIdx.x, ne01)`: `tile0 = x/ne01`, `i1 = x%ne01` | explicit `/` and `%` on non-negative operands |
| early return past `ne0`/`ne01`/`ne02`/`ne03` | explicit branch returning 0 |
| left reflection `-rel` | `reflect_index()` first branch |
| middle copy `rel` | `reflect_index()` second branch |
| right reflection `2*ne00-2-rel` | `reflect_index()` third branch |
| source/destination byte offsets from `nb00..nb03`, `nb0..nb3` | integers in `pad_elem()` |
| `*(float*)` load and store | `ld_f32` / `st_f32` primitives |

Host (`source/host.goose`): packet decode, the `p0`/`p1` non-negativity check,
the native `ne0 == ne00 + p0 + p1` assertion turned into an explicit refusal,
`tiles0 = ceil(ne0/256)`, grid `(ne01*tiles0, ne02, ne03)` and block `(256,1,1)`.

Division/modulo note: `blockIdx.x`, `ne01` and the loop bounds are
non-negative, so Goose's Euclidean `/` and `%` agree with C here. This is the
standing rule from `MODULO_SEMANTICS.md`; it was checked, not assumed.

## Result

| gate | result |
|---|---|
| GPU differential, whole destination buffer including padding sentinels | **82 216 bytes, 0 differences**, 0 launch failures |
| host-plan fixture | **24 checks, 0 failures** |
| compute-sanitizer memcheck | 0 errors |
| compute-sanitizer racecheck | 0 hazards |
| compute-sanitizer initcheck | 0 errors |
| compute-sanitizer synccheck | 0 errors |
| mutation gate | **5/5 caught** |

The GPU cases cover identity, both pads, left pad only, one column, a
multi-plane tensor with padded source and destination rows, and tails at
33/64/300/1000 columns. The oracle is a verbatim copy of
`pad_reflect_1d_kernel_f32`, including `fast_div_modulo` and
`init_fastdiv_values`; the Goose arm receives the same geometry.

Mutations, and the failures each produced:

| mutation | failures |
|---|---:|
| left reflection sign flipped | 562 |
| middle bound `<` becomes `<=` | 112 |
| right reflection uses `+2` instead of `-2` | 316 |
| row selector uses `ne01+1` | 13 332 |
| destination ignores the padded row stride | 14 926 |

One mutation was replaced before counting: the first attempt changed
`i0*nb[0]` to `i0*4`, which is unobservable because `nb[0]` is always 4 for
F32. The observable version ignores the destination **row** stride, and is
caught.

## Integration

Not integrated. `GGML_OP_PAD_REFLECT_1D` is not constructed by any model builder
in this tree; only `tests/test-backend-ops.cpp` reaches it. Under the rule set by
EXP245/246/247, a gated implementation of an unreachable kernel is the
deliverable, and adding it to the shipped DSO would add an unexercised path.
Adoption is mechanical: replace the reflection arithmetic with
`fg413dd_gg_function_1` and the launcher plan with `fg413h_run`.

## P3-03 remainder

This closes the PAD_REFLECT_1D item only. P3-03 stays IN_PROGRESS because the
following are still native (recorded in EXP177 and the task ledger):

- the transposed CPY tile kernel;
- quantized and different-type CPY pairs;
- quantized GET_ROWS dequantizing paths and GET_ROWS_BACK.

## Reproduce

    python3 emit_sources.py
    bash build_fixture.sh
    runtime/exp413/build/host-plan
    runtime/exp413/build/gpu-fixture
    python3 mutate.py

The pinned compiler is `runtime/compiler/current.json`; `emit_sources.py`
refuses to run when its SHA-256 does not match. Requires one free CUDA device.
