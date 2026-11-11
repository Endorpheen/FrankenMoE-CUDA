# CUDA_MATVEC: module correct, rebuild chain missing

EXP186 (Goose Q6_K CUDA matvec) is functionally complete and was measured byte-exact: 4/4 outputs identical,
Apex content sha256 `e68df05b`, memcheck clean. It was rejected on speed (-3.66% decode) and therefore left opt-in,
so the symbol never entered the active `libggml-cuda` (verified: 0 occurrences of `GGML_GOOSE_CUDA_MMVQ_Q6K` in the
staged library, and a run with the flag set produced no Q6_K witness).

Under the owner's decision that speed is not a veto and that accepted improvements are enabled by default, the
source flag was changed from opt-in to opt-out (`GGML_GOOSE_CUDA_MMVQ_Q6K=0` disables). Rebuilding requires
`build_runtime.py`, which reads `runtime/exp185/build/commands.json`, `runtime/exp185/build/native-mmvq.o` and
`runtime/exp185/run/libggml-cuda.so`. In this restored tree only `runtime/exp185/measured/` and
`runtime/exp185/results/` exist, so the object chain is gone and the module cannot be relinked as-is.

Next: reconstruct the exp185 build chain (native-mmvq.o + host.o + base library) from the surviving
`measured/.../run/libggml-cuda.so` and the sources, then enable and verify.
