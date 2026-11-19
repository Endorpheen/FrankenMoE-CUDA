# Which quantisation types this runtime's own model contains

The removal plan is blocked by row types nobody has ported. A native body may only be deleted when no supported scenario
reads it (rule 28), so the first question is which types the Apex model uses. Reading the tensor list needs no weights and no
GPU:

```
apex model: tensors=753
  f32             310 tensor(s)
  q4_K            379 tensor(s)
  q6_K             64 tensor(s)
apex model: distinct types=3
```

Three types, and the row-size gate then says which of them were already ported:

| type | tensors | ported before | ported now |
|---|---|---|---|
| q4_K | 379 | quant-rows2 | - |
| q6_K | 64 | quant-rows2 | - |
| f32 | 310 | **no** | quant-rows |

So the Apex path was blocked by exactly one type, and it is the largest single group of tensors after q4_K. `f32` and `f16`
were added to `goose/model/quant-rows.goose` as unquantised types - one element per block, row size = element size times the
count - which the gate confirms:

```
quant rows: types=35 port1=9 port2=16 neither=10 row_size_failures=0
```

Ten types remain answered by neither, and they matter for other model families rather than for Apex: `q8_1`, `iq2_xxs`,
`iq2_xs`, `iq1_s`, `i8`, `i16`, `i32`, `i64`, `f64`, `bf16`. That list is what a Qwen or a legacy port has to cover.
