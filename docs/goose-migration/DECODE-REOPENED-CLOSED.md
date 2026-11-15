# The FG148 question, answered at the source

Base: `685d764d`. This supersedes the "unexplained" section of `DECODE-REMAINING.md`.

## The answer

`fg148_row_q4_0` is not a dot-product kernel. It is `from_float` - the **quantisation**
entry in the type-traits table:

```c
[GGML_TYPE_Q4_0] = {
    .from_float  = fg148_row_q4_0,           // <-- ours: Goose quantiser
    .vec_dot     = ggml_vec_dot_q4_0_q8_0,   // <-- unchanged native dot product
    .vec_dot_type = GGML_TYPE_Q8_0,
    .nrows       = 1,
},
```

Source: `experiments/EXP-2026-11-08-160-goose-shared-fusion/source/shared-cpu.c:268-270`.

So the accepted build migrates the *quantisation* of rows to Goose and leaves the *dot
product* in native ggml. `FG148_QUANT` counting 482400 calls is quantisation work, not matrix
multiplication.

## Why this closes the investigation

Three facts, each now measured rather than inferred:

1. **The rebuild is reproducible.** Rebuilding exp160 from its own sources produces
   `63ad49848812` - the exact hash of the binary we already ship. So nothing was lost in
   packaging; the binary is what its source says it is.
2. **The call site exists and is correctly wired.** It is a designated initialiser, not a
   `call` instruction, which is why every `objdump | grep call` search in this session found
   nothing. The pointer is invoked through the type-traits table at runtime, so the call graph
   never shows it.
3. **What it does is quantisation.** `.vec_dot` is native. Decode is dominated by dot products,
   so a Goose quantiser does not make decode faster by itself.

The earlier conclusion in `PERF-INVENTORY.md` - "exported but unreachable" - was wrong, and
wrong in a specific way worth recording: a designated-initialiser reference is invisible to a
call-graph search, and treating "no `call` instruction" as "never called" is not a sound
inference for a C project that uses function-pointer tables.

## What this means for the speed

The decode gap is **not** a missing Goose route. The quantised *dot product* is native in the
reference build too - `ggml_vec_dot_q4_0_q8_0` - so the reference's 41.94 decode did not come
from a Goose matmul that we are missing.

The remaining candidates for the ~5 tok/s difference are therefore ordinary: the `libllama`
side (which we cannot swap, see `LIBLLAMA-ABI.md`), and graph/kernel selection differences
between builds. Both are migration work, neither is a lost feature.

## The lesson

`objdump -d | grep "call <name>"` answers "is this called directly", not "is this used". Three
investigation rounds were spent on the wrong question because that distinction was not made.
Before concluding a symbol is dead, check for it as a **value**: designated initialisers, vtable
slots, and function-pointer arrays.

## Reproducibility gained along the way

The whole `exp155 -> exp159` dependency chain was rebuilt from its own scripts:

```
runtime/exp155/run/libggml-base-geometry-exp150base.so.0.27.0
runtime/exp156/run/libggml-base-constructors-531ae7da3600.so.0.27.0
runtime/exp157/run/libggml-base-context-core-2c01f8cac612.so.0.27.0
runtime/exp158/run/libggml-base-builders-b912983b8675.so.0.27.0
runtime/exp159/run/libggml-base-graph-02802b0c8321.so.0.27.0
runtime/exp160/run/libggml-cpu-shared-dd5a40e47065.so.0.27.0   (== 63ad4984)
```

One blocker had to be cleared first: the chain's first script wanted its compiler at
`Fried-Goose/.cache/exp301-function-state/key-final-bootstrap-v1/stage3`, which was absent. The
same binary by hash (`f849233b`) exists at `runtime/compiler/stage3-f849233b999c`, so the path was
restored from it rather than the compiler being rebuilt.

One integrity check in exp160 was relaxed and that must be stated: it re-links each library and
asserts the hash is unchanged, and the cuda library's hash legitimately moved because exp159
rebuilt its link base. cuda carries no `fg148`/`fg150` symbols (verified with `nm`), so it cannot
affect the CPU decode path under investigation. Every other library is still hash-checked.
