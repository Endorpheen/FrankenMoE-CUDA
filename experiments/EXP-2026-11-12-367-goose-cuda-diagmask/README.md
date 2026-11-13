# EXP367: CUDA `diag_mask_inf` in Goose -- the last unported kernel of P3-06

P3-06 covers softmax, masks, RoPE and position encodings. Softmax is complete: EXP280 (mask-free with alibi),
EXP281 (float and half mask), EXP282 (sinks), EXP284 (backward) and EXP285 (cooperative path) mean every softmax
kernel has a Goose implementation. RoPE was EXP171. What remained unported was the diagonal mask.

`diagmask.cu` is small -- one kernel, one launcher, one op entry -- which is exactly why it was easy to overlook:
every audit of the softmax family named `softmax.cu` and stopped there.

## What was ported

Device, in Goose: the `(row, col)` decomposition from the 2-D block, the `col >= ncols` early exit *before* any
load, the linear index `row*ncols+col`, `limit = n_past + row % rows_per_channel`, and
`dst = x - (col > limit) * FLT_MAX`.

Host, in Goose: the column-block count `ceil(ncols/256)`, with the division refused on a zero or negative size
rather than divided by (the native launcher divides without checking).

Externs are typed primitives only: `ld_f32`, `st_f32`, `fail`.

## Result

| check | result |
|---|---|
| device values compared bit for bit | **12 536, 0 differences** |
| shapes | 6: 256x1, 512x3, 300x2 with n_past=5, 1000 with 2x2x1, 128 with 4x2x2 and n_past=7, 4096x1 at n_past=4096 |
| launch failures | 0 |
| memcheck / racecheck / initcheck / synccheck | 0 / 0 / 0 / 0 |

The oracle is a verbatim copy of the native kernel body, not a re-derivation -- with a kernel this short a
reimplementation would have been a second guess rather than a reference.

## Mutation gate

5 of 5 caught, each breaking one decision the port is responsible for:

| mutation | failures |
|---|---:|
| FLT_MAX replaced by zero (the mask disappears) | 8262 |
| `>` becomes `>=` at the boundary | 26 |
| subtraction becomes addition | 8262 |
| row modulo removed | 100 |
| destination index uses `row` instead of `row*ncols` | 11 launch failures (trap) |

Two lessons worth keeping. A one-ULP change to the FLT_MAX constant is **not** observable: at these operand
magnitudes `x - FLT_MAX` and `x - nextafter(FLT_MAX)` both saturate to `-inf` in the same way, so that mutation
was replaced rather than counted as a pass. And the index mutation surfaced as launch failures rather than value
differences, because the wrong index ran off the allocation and trapped -- the harness counts both channels, so
it was still caught.

## On the live path

Not on it, and this is recorded rather than implied: `ggml_diag_mask_inf` is constructed only by
`tests/test-backend-ops.cpp`. No model builder in this tree emits `GGML_OP_DIAG_MASK_INF`, which is why the accepted
Apex run log contains zero occurrences of it. The module is therefore **OFF_PATH**: emitted, gated, and
deliberately not linked into the shipped DSO, exactly like the other off-path kernels (EXP245-288).

That is a statement about reachability, not about correctness, and it is the honest end state for P3-06: every
kernel in the softmax/mask family now has a Goose implementation with its own differential gate, and the ones the
product does not run are marked as such instead of being presented as live.

## Criterion check for P3-06

| clause | state |
|---|---|
| softmax all kernels | Goose: EXP280/281/282/284/285, bit-identical, mutation-gated |
| masks | Goose: `diag_mask_inf` here, plus the attention-mask arithmetic inside EXP281/282 |
| RoPE and position encodings | Goose: EXP171 (normal/NeoX/MRoPE/IMRoPE, YaRN, F16, offset, strided, inplace, forward and backward) |
| differential with a tolerance fixed before the candidate test | bit-exact for every ported kernel; no tolerance was needed |
| liveness | the softmax/mask kernels that the profile reaches are witnessed; `diag_mask_inf` is proven unreachable by construction site |

The residual native work named in this item's earlier evidence was "vision/set-rows/RMS fusion". Those belong to
other items (P2/P3-03/P3-04) and are not part of the softmax/mask/RoPE criterion; they are not counted here.
