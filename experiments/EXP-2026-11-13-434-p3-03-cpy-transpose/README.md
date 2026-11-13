# EXP434: transposed CUDA CPY tile in Goose (F32)

`cpy_scalar_transpose<float>` stages a 32x32 tile through a double-buffered
shared `float[32][33]` array and writes it transposed. The Goose module owns the
tile indices, the eight-matrices-per-block loop and the double-buffer switch;
the wrapper owns the shared array. One extra barrier after the write is added
for safety (native relies on buffer alternation); it does not change output.

## Gate

The fixture contains a verbatim copy of the native kernel and compares the whole
destination buffer bit for bit.

| case | values |
|---|---:|
| 10 cases: 32x32, tails on x/y, 100x7, 7x100, 1x1, 8/9 matrices (double-buffer wrap), 65x65x3, 17x19x5 | **36 235 values, 0 failures**, 0 launch failures |

Sanitizers: memcheck, racecheck, initcheck and synccheck all 0. Mutations 5/5:
load tile transposed, write tile untransposed, destination offset loses `ne00`,
source offset loses `ne01`, destination column uses `bx`.

The first double-buffer mutation was unobservable because the added barrier
makes a single buffer safe; it was replaced by the column-index mutation rather
than counted as a pass.

## Scope

F32 only; other element types need their own fixtures because the native kernel
reinterprets the shared float tile as `T`. It is implemented and gated, not
linked (OFF_PATH), per the EXP245/246/247 rule. Quantized and different-type CPY
pairs remain separate work in P3-03.

## Reproduce

    bash experiments/EXP-2026-11-13-434-p3-03-cpy-transpose/build_fixture.sh
    runtime/exp434/build/gpu-fixture
    python3 experiments/EXP-2026-11-13-434-p3-03-cpy-transpose/mutate.py
