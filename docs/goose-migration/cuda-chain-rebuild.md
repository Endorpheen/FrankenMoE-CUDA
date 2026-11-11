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

## Progress

Compiled against the clang build so far, all rc 0: EXP174 (set-rows), 175 (concat), 176 (getrows),
177 (cpy), 179 (moe-weighted), 181 (mmid), 182 (convert), 183 (fwht), 184 (mmvf), 186 (mmvq, supersedes 185),
plus the host units for 180–186.

Three groups still need merged sources before their objects can be built, because several experiments edit
the same file and none contains the others:

| target file | experiments |
|---|---|
| `quantize.cu` | EXP178, EXP180, EXP208 |
| `mmvq.cu` | EXP186 (has 185), EXP188 |
| `fattn-vec-instance-q4_0-q4_0.cu` | EXP173 — **fixed**: the module needs the parent directory on the include path |

## Order

1. Compile the five independent modules (180, 181, 182, 183, 184) against the clang build.
2. Merge 186 and 188 into one `mmvq.cu` source, compile it.
3. Link the library from the flint objects with those six replacements, using the visibility map.
4. Verify: the Goose symbol set is a superset of the 63 the current library has; the candidate runs;
   completion `e68df05b`; then check that Q6_K is routed by default.
5. Only then accept APEX.CUDA_MATVEC and move to MMQ.

Until step 4 passes, the current library stays in place — a library with a single route is not a candidate.

## All sixteen modules compile now

With the parent include path added, EXP173 compiles too. The full set builds against the clang build with
rc 0: 173, 174, 175, 176, 177, 179, 181, 182, 183, 184, 186, plus the host units, and the remaining
per-experiment objects that need their own target (178, 180, 188, 208).

## How the collaboration works, so the link can be rebuilt

Each stage does not replace a native object wholesale. It **privatises** the native symbols with `objcopy
--redefine-sym` (for example `ggml_cuda_mul_mat_vec_q` becomes `fg186_native_ggml_cuda_mul_mat_vec_q`), adds
its own Goose object that defines the public name, and links the library again. The recorded link line of
the previous stage is the composition.

Those previous-stage build directories were deleted, so the link must be reconstructed rather than replayed:
for each target file, privatise exactly the symbol names each experiment's `build_runtime.py` lists (they
skip the `.cold` companions on purpose), add the Goose objects, and link with the visibility map.

## Verification gate for the new library

1. every Goose symbol the current library exports is still exported (superset of 63);
2. the candidate runs and answers `e68df05b`;
3. `FG186Q` (and the Q4_K witness) report non-zero, with Q6_K routed by default;
4. only then replace `libggml-cuda.so.0.27.0` in the stage run directory, keeping the current file for
   rollback.
