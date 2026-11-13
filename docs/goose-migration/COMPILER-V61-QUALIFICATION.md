# Compiler v61: runtime qualification on the used path

Candidate: `/home/end0/AI/Fried-Goose/.cache/compiler-exp-handoff/runtime-candidate-v61/stage3`
SHA256 `b87af17a8b06bed5ad3f4ebfdce5edd7539d3e75f7fb1712d19189ff9dd4cedf`, source commit
`d710bb0920de7caaf4bf5a49eb0a457bd6af43ce`. The IR/ABI of the generated host code differs from v58 (53,232 vs
66,517 bytes of C), so it was qualified functionally rather than assumed compatible.

| check | result |
|---|---|
| binary SHA | matches, no compiler rebuilt |
| Q6_K CPU module (EXP412), gate | **240/240 bit-exact** vs the dispatched AVX2 kernel |
| Q6_K CUDA module, harness | **IDENTICAL** at ncols_dst = 1 and 2 (nrows 8) |
| Q6_K CUDA, accepted profile | `first_sha e68df05b`, probabilities equal, restore ok, **636** single-column sub-calls (all 324 Q6_K matvec calls) |

One portability fix was needed in our CUDA host launcher: v61 emits `_Alignas` in device code, so
`fg186q-runtime.cu` defines `_Alignas(x)` as `alignas(x)` for C++ before including the generated header. That is a
shim in our file, not a change to the compiler.

Pinning: `runtime/compiler/current.json` now points at v61, with the previous pin preserved in
`runtime/compiler/current-v58-backup.json`, so rollback is a single copy. The compiler binary was not touched or
rebuilt.
