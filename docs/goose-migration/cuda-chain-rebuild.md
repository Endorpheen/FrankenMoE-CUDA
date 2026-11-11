# Rebuilding the CUDA Goose chain: what works, what is missing, in what order

## What now works

`libggml-cuda` builds in this tree through the clang wrapper (documented in `cuda-build-unblocked.md`), and
the EXP186 Q6_K module compiles and links: its object plus `host-unit.o`, linked with
`--version-script=source/visibility.map`, exports `fg186q_cuda_host_seen` and `fg186q_cuda_device_read`.

## What is missing

The library this candidate loads carries **63 Goose routes** (fg168…fg185). Those objects were produced by
a chain of per-experiment link steps whose build directories were deleted in the disk cleanup
(`cleanup_2026_11_11` in `runtime/active-exp244-backed-up.json` kept only `exp244`). Linking a fresh flint
build plus the Q6_K object gives a library with **only** fg186 — a regression of 61 routes.

So the chain must be rebuilt from the experiment sources, not patched.

## The chain, from the sources that exist

| experiment | replaces | exports |
|---|---|---|
| EXP180 | `quantize.cu` | fg180q |
| EXP181 | `mmid.cu` | fg181m |
| EXP182 | `convert.cu` | fg182d |
| EXP183 | `fwht.cu` | fg183f |
| EXP184 | `mmvf.cu` | fg184m |
| EXP186 | `mmvq.cu` | fg185q + fg186q (it supersedes 185) |
| EXP188 | `mmvq.cu` | fg188d (dequant q4_0) — must be merged with 186, they touch the same file |

The last row is the only real work beyond mechanical compilation: two modules edit `mmvq.cu` and neither
contains the other, so they need a single combined source before that object can be built.

## Order

1. Compile the five independent modules (180, 181, 182, 183, 184) against the clang build.
2. Merge 186 and 188 into one `mmvq.cu` source, compile it.
3. Link the library from the flint objects with those six replacements, using the visibility map.
4. Verify: the Goose symbol set is a superset of the 63 the current library has; the candidate runs;
   completion `e68df05b`; then check that Q6_K is routed by default.
5. Only then accept APEX.CUDA_MATVEC and move to MMQ.

Until step 4 passes, the current library stays in place — a library with a single route is not a candidate.
