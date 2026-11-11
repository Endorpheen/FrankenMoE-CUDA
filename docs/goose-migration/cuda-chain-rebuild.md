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

## Where the reconstruction stands after the first link attempt

`runtime/reconstructed/tools/link_cuda_chain.py` privatises the overlapping public symbols by
`objcopy --redefine-sym` (keeping `.cold` companions) and relinks. All fifteen module objects compile and
are picked up; the tool found and fixed three collisions on the way:

| collision | resolution |
|---|---|
| three experiments all target `quantize.cu` and two of them define the same two symbols | only EXP208 is linked; 180 and 178 are dropped because 208 supersedes them for those names |
| a stage with no overlapping symbol was still copying the native object | module-only path added |
| EXP182 and EXP188 both intercept `ggml_get_to_fp16_cuda` | **unresolved** |

## The one remaining merge

`ggml_get_to_fp16_cuda` is a single dispatch function: each of EXP182 (Q4_K dequant) and EXP188 (Q4_0
dequant) installs its own version, and neither contains the other. Linking both is a duplicate definition,
and linking one drops a route. They need one combined dispatch that routes Q4_K and Q4_0 to their
respective Goose converters and everything else to native. That is the last piece before the library can be
built and the item verified.

## The chain links

`link_cuda_chain.py` now produces `/tmp/libggml-cuda-chain.so.0.27.0` (159 MB) with **rc 0**, after three
more fixes: privativisation runs over **all** native objects (EXP173 defines `fattn.cu`'s symbols, not only
its own file's), one priv copy per target instead of one per module, and the exclusion list covers every
replaced native object.

It exports **32** Goose routes; the current library exports 63. The 41 missing ones are fg166–fg179, whose
experiments use other source shapes (`cuda-broadcast.cu`, `arg-*`, and so on) rather than the
`cuda-runtime.cu` pattern this tool assumes. The new routes fg186q and fg188d are present, so Q6_K is in the
built library.

## What is left, precisely

Collect the remaining ten or so modules in their own shapes and add them to the link, then run the gate:
symbol superset of 63, completion `e68df05b`, Q6_K routed by default. The library is not installed until
that passes — it currently has fewer routes than the one it would replace.

## The rebuilt library changed the answer — rejected, rolled back

Linking succeeded and the server started, but the completion hash moved:

| library | completion sha256 |
|---|---|
| inherited (in place) | `e68df05b…` |
| rebuilt chain | **`e818303c…`** |

Two consecutive completions with the rebuilt library agreed with each other, so it is not noise: the
library computes something different. That is a correctness regression and the library was **not**
installed; the inherited file was restored and the run again gives `e68df05b…`.

### Likely cause, to be confirmed

The chain now switches on routes that were opt-in in their own experiments. The per-experiment scripts
enabled a route only for their A/B pair; linking them all makes several active at once by default (the
flash-attention instance, the quantize paths and others), and at least one of them does not reproduce the
native result exactly on this profile.

### What the next attempt must do

1. link the chain but force every goose CUDA switch **off** (the disable contract), and require
   `e68df05b…` — this proves the reconstruction itself is faithful;
2. then enable them one at a time, each with the exact-answer gate, and keep only those that preserve it;
3. only then check that Q6_K is among the enabled routes, and accept the item.

Step 1 is the test that should have run first: a rebuilt library that differs with everything off is a
build defect, not a route problem.

## Result of the two experiments: the rebuilt library is not numerically equivalent

Three builds, three different answers, all with every goose CUDA switch **off**:

| build | completion sha256 |
|---|---|
| inherited (nvcc, in place) | `e68df05b…` |
| rebuilt chain through clang | **`e818303c…`** |
| rebuilt chain through clang with `-ffast-math` (nvcc's `-use_fast_math` translated) | **`048eb178…`** |

With the routes switched off, the only difference is the compiler that produced the kernels. So this is not
a routing problem: a clang-built `libggml-cuda` does not reproduce nvcc's arithmetic on this model, with or
without fast-math. The inherited library was restored again and the run gives `e68df05b…`.

## The blocker, stated exactly

The item needs the Q6_K matvec inside a library that is numerically identical to the accepted one. That
requires building the CUDA kernels with the **same toolchain** (nvcc) that produced the accepted library.
nvcc 12.6 does not compile against this system glibc:

    bits/mathcalls.h: error: exception specification is incompatible with that of previous function
                       "cospi" / "sinpi" / "rsqrt"

`-ccbin g++-12`, `g++-13`, `-std=c++03/11` and `-D__GLIBC_USE_IEC_60559_FUNCS_EXT_C23=0` all fail the same
way. This is a functional/correctness blocker, not a performance one, so it is the one class of issue that
still goes to the compiler side with a minimal repro.

## What is not the problem

The Goose modules are fine: they compile, they link, and they export the expected routes. The reconstruction
tool is fine: it now privatises by the names the modules expect and preserves the fg entry points the
per-module `.so` files consume. The missing piece is only the toolchain that reproduces nvcc's numbers.

## Status after the toolchain correction

The compiler side is right that nvcc 12.4 works here: `/usr/bin/nvcc` (12.4.131) compiles a `<cmath>` +
device-`exp` kernel and prints `2.718282`, while `/usr/local/cuda-12.6/bin/nvcc` fails on the glibc clash.
My earlier toolchain blocker was therefore based on the wrong binary.

Switching the rebuild to nvcc 12.4 progressed much further:

| step | result |
|---|---|
| thirteen modules compiled with nvcc | ok |
| link through nvcc with the visibility map and `-lcuda` | ok |
| missing `fattn-vec-instance-*` objects (absent from the old build tree) | compiled from source and added |
| server start with the rebuilt library | **fails**: `RemoteDisconnected`, after the driver symbols resolved |

So the structure is now right (all symbols resolve), but the rebuilt library still does not run cleanly. The
inherited library is back in place and the run gives `e68df05b…`.

## What the next pass should do

Run the rebuilt nvcc library under a debugger or with `CUDA_LAUNCH_BLOCKING=1` to get the actual failure
instead of the connection drop, and compare the object list of the old build tree (71 objects, no
`fattn-vec-instance-*`) with the library the accepted runtime actually ships (220 MB, which does contain
them). That difference means the old `build-cuda` tree is itself incomplete, so the object set has to be
taken from the accepted library's lineage rather than from that stale build directory.

## The real cause: the old build tree is missing instance objects

The start failure is not a link error in my reconstruction, it is a missing definition:

    undefined symbol: mul_mat_q_case<GGML_TYPE_Q6_K, GGML_PREC_DEFAULT>

`mmq.cu.o` in the old `build-cuda` tree defines no `mul_mat_q_case` at all, and the tree contains no
`mmq-instance-*` objects, although `compile_commands.json` lists **22** such translation units (the same is
true of the four `fattn-vec-instance-*` files handled above).

So the old build directory is an incomplete build of the source tree: it was configured without the MMQ and
FA instance variants. The accepted 220 MB library does contain them, which is why it works.

## Next, concrete

Compile the 22 `mmq-instance-*` translation units (plus the FA instances already done) with nvcc, add them
to the link, and re-run the off-switch gate. Only after that library answers `e68df05b` with every goose
route disabled is the reconstruction faithful, and only then do the routes get enabled one at a time.
