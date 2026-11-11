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

## Reconstruction progress

`EXP-2026-11-08-185-goose-cuda-mmvq-q4k/build_runtime.py` does run here and produced
`runtime/exp185/build/cuda-mmvq-q4k.o` and `runtime/exp185/build/host.o` before stopping, because it consumes
`runtime/exp184/build/commands.json`, which is also missing.

So the dependency chain is 180 -> 181 -> 182 -> 183 -> 184 -> 185 -> 186, each experiment's `build_runtime.py`
relying on the previous one's artefacts. Each link must be rebuilt in order (nvcc), starting at EXP180, before the
Q6_K module can be relinked into the active library. The sources and scripts for every link are present; only
their build outputs are missing.

## Chain reconstruction attempted and stopped

Ran every `build_runtime.py` from EXP170 upward, in order. Result:

- EXP170, 171, 172: **no `build_runtime.py` at all**, so their build outputs cannot be produced here;
- EXP173: fails with an empty message (its own prerequisites are missing);
- EXP174 and 175: stop on the missing `runtime/exp174/build/commands.json`;
- EXP176..186: `IndexError: list index out of range`, because they read the previous link's artefacts.

So the chain cannot be reconstructed in this tree by running the scripts: its foundation (EXP170-172) has no
build scripts, and everything above consumes artefacts that were never committed. What survives is the built
`libggml-cuda.so` under `runtime/exp185/measured/.../run/`, which contains the Q4_K module but not Q6_K.

Consequence for CUDA_MATVEC: the Q6_K module is functionally verified (byte-exact, `e68df05b`) but cannot be
linked into the active library from what is present. Options, none of which I will choose unilaterally: (a) treat
the surviving exp185 library as the base and splice the Q6_K module into it at the binary level, (b) rebuild the
CUDA backend from the flint sources with both modules compiled in, or (c) record the item as blocked by missing
build infrastructure.
