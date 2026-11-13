# P3-20 census: the candidate CUDA link, and the definition it depends on

Criterion: "Zero owned hand-written CUDA/C++ computation/orchestration in selected link; generated CUDA/C from Goose..."

## The census

The recorded `unchanged-exp165-cuda-link-integrity` link step in the EXP166 receipts names 145 objects. Resolving them against the build
directory and applying the same mechanical marker used for P2-18 and P4-16:

| class | count | detail |
|---|---|---|
| native CUDA kernels | 144 | the `.cu.o` set from flint: elementwise, reductions, `mmq`, `mmvq`, `fattn`, `convert`, `ggml-cuda.cu` and the rest |
| Goose-generated CUDA module | 1 | `runtime/exp160/build/shared-cuda.o` -- and this object is **not present on disk**, so the CUDA library cannot be relinked as recorded without rebuilding it |

So the CUDA candidate is native kernels plus one generated integration object, which is the same shape as the CPU and core links.

## Why the item cannot be closed on this census

The criterion says "zero **owned** hand-written CUDA/C++". That is the same word, with the same ambiguity, that blocks P2-16:

1. Read as "authored by this project", the 144 upstream `.cu.o` objects are not owned and the clause holds -- our own CUDA code is the
   generated module.
2. Read as "shipped in the candidate", the 144 native kernels are hand-written CUDA computation in the link and the clause cannot hold
   without porting kernels the Goose compiler does not emit.

The plan does not define the term, and taking the decision myself would be a scope call. It is the same question as decision 7 in
`OWNER-DECISIONS-20261112.md`, and it now covers both P2-16 and P3-20.

## Separate integrity note

`shared-cuda.o` being absent means a CUDA relink today would either fail or pick up a stale substitute. Nothing in this session needed one,
but it is recorded because the same class of missing intermediate caused a silent failure earlier (the dangling stage symlink in P4-03).
