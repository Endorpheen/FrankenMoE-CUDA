# Patches applied to the copied ggml-cuda tree (EXP236)

The experiment needed a **whole copy** of `ggml-cuda`, because:
- `mmq-vec-dot.cuh` does `#include "vecdotq.cuh"`, and a quoted include resolves in the includer's
  directory first — an `-I` copy is silently ignored;
- `-D` on the function name renames the call sites too;
- `-include` works but cannot change either of the above.

Three clean builds produced byte-identical cubins before I compared hashes and found this.

These two patches reproduce the copied tree's differences from upstream, so the copy itself does not need
to be committed. Apply against the same llama.cpp-flint-cuda checkout:

    cd <llama.cpp-flint-cuda>/ggml/src/ggml-cuda
    patch -p0 < .../patches/common-cuh-force-dp4a.patch
    patch -p0 < .../patches/vecdotq-q6k-goose.patch

`common-cuh-force-dp4a.patch` adds `&& !defined(GGML_FORCE_DP4A_PROBE)` so the dp4a path can be forced with
`-DGGML_FORCE_DP4A_PROBE=1`. `vecdotq-q6k-goose.patch` splices in the emission and redirects the Q6_K body.

Neither patch belongs in production; both exist only to reproduce the probe.
