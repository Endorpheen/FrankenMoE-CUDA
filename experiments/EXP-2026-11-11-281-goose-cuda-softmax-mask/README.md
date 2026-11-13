# EXP281: CUDA softmax with a mask in Goose (F32 and F16 mask)

`soft_max_f32` for mask `T = float` and `T = half`, extending EXP280's mask-free
path. Sinks and the cooperative/backward variants remain unported.

## Verdict

**OFF_PATH** — bit-exact against both native mask instantiations; the fixed
32K/128 Apex profile runs Flash Attention with its own internal softmax. No
performance claim, shipped DSO unchanged.

## Scope transferred

- mask pointer arithmetic exactly as native:
  `mask += (i11*nb11 + i12*nb12 + i13*nb13) / sizeof(T) * (mask != nullptr)`,
  with `i11 = blockIdx.x`, `i12 = blockIdx.y % ne12`, `i13 = blockIdx.z % ne13`.
  The F16 variant divides the byte offset by 2, the F32 variant by 4.
- value pass `x[col]*scale + slope*t2f32(mask[col])` (the mask product replaces
  the `+0.0f` term).
- everything else is EXP280's body: alibi slope, value pass, block max, exp pass,
  shared reuse, block sum, `1/sum`, normalize.

The host launch plan is mask-independent, so `source/host.goose` is EXP280's plan
unchanged; its gate is included.

## Gate

| check | result |
|---|---|
| device: 5 shapes × 2 mask types (ncols 32/100/128/1024/1000; broadcast and full mask dims; one alibi `max_bias=8` case) | 15 584 values bit-identical |
| device failures / launch failures | 0 / 0 |
| host: EXP280 plan gate | 45 checks / 0 failures |

Oracle: the same verbatim struct+kernel copy as EXP280 (source sha256 recorded).

## Mutations — and a harness hole

| mutation | failures |
|---|---:|
| F16 mask offset divides by 4 instead of 2 | 1 663 |
| mask row index drops the `% ne12` | 362 |
| mask term drops the alibi slope | 826 |
| F16 mask loaded at 32-bit width | 9 (launch failures) |
| final pass skips normalization | 4 620 |

The 32-bit-read mutation first reported **0 failures** and looked missed. The
misaligned 4-byte read traps, so the signal was `launch_fail`, not `failures`, and
the harness summed only `failures=`. The harness now counts `launch_fail=` too.
This is a variant of the EXP279 lesson: the gate must count every failure channel
the fixture exposes, not just the value-mismatch one.

## Reproduce

    python3 emit_sources.py
    python3 test_local.py
    python3 mutate.py
