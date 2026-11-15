# REPORT — EXP-2026-11-15-660 (P10-06, CUDA artefact policy)

## Result

| item | value |
|---|---|
| slices | **3** (arch `arch_`, flags `tgt_`, install `inst_`) |
| gates | **3/3 green**, 0 failures, identical at `-O1` and `-O0` |
| cases / values | 60 / 60 (arch), 34 / 34 (tgt), 30 / 30 (inst) -> 124 |
| mutations | **36/36 caught**: 12 per slice, two halves 18/18 |
| native oracle | executed native text: `cmake -P`, a real `cmake` configure, a real `cmake --install` |
| model / GPU / server / benchmark / nvcc | none started |
| repository changes | none (work under `/tmp/opencode/sub-660/exp/`) |
| base commit | `7bc37253`; vendored llama.cpp `0b5be7e4`, `ggml/` clean |

## Method

1. **Inventory.** Read `P10-06-BUILD.md` (the already-proven `--repro`,
   `dist/SHA256SUMS`, `manifest.json` half), `NVIDIA-ARCH-POLICY.md`, the
   vendored `ggml-cuda/CMakeLists.txt`, `ggml/src/CMakeLists.txt`,
   `ggml/CMakeLists.txt`, `tools/package-main-runtime.sh` and
   `tools/build-main-runtime.sh`. `upstream/` is gitignored, so the native
   inputs are the vendored checkout at the `UPSTREAMS.md` pin, and the oracles
   read only the byte-identical copies under `tests/native/`. Rejected
   candidates (already owned by P10-05, needing `nvcc`, or not decisions) are
   listed in `README.md`.
2. **Three independent slices, one Goose module each.** `arch_` covers the
   default architecture list, the `12X` -> `12Xa` rewrite, the native probe
   adoption, the list regex and the suffix/sm tables. `tgt_` covers
   `CUDA_FLAGS`, the `-Xcompiler` join and the define and link sets. `inst_`
   covers `ggml_add_backend_library()`: kind, destination, versioning, installed
   names, defines, the available-backend list and the `GGML_BACKEND_DIR` guard.
3. **Differential gates that execute the native text.** `cmake -P` runs the
   three arch blocks with the CUDA/CMake/native inputs supplied and the result
   is read back; a synthetic CMake project inlines the define, link and flag
   sections in source order and `COMPILE_OPTIONS`, `COMPILE_DEFINITIONS` and
   `LINK_LIBRARIES` are read back; another synthetic project inlines the native
   function, is configured, built and installed, and the real prefix listing is
   compared. The host-compiler branch is deterministic through a
   `ggml_get_flags()` shim and a fake `CMAKE_CUDA_COMPILER`. The `-Xcompiler` cut
   point is the literal element the native text appends.
4. **Mutation run.** 12 mutations per slice, split 6/6, every mutated module
   generated at `-O0` and linked against the unmodified fixture. One survivor
   was found and closed on the first pass (no case covered CUDA 12.7, so the
   `12.8` threshold could move to `12.7` unnoticed); a `default 12 7` row was
   added and the mutation now dies with one failure. Recorded, not hidden.

## Gate output

```
FG660_ARCH cases=60 values=60 failures=0
FG660_TGT cases=34 values=34 failures=0
FG660_INST cases=30 values=30 failures=0
```

Same three lines from the `-O0` run (`results/gate-o0.log`).

Two native quirks were reproduced rather than assumed, and both are now pinned
by cases:

- the list regex `^[0-9]+(a|f)?(-real|-virtual)?(;[0-9]+(a|f)?(-real|-virtual)?|;)*$`
  accepts a trailing or doubled separator, so `89;` matches, while `;89` does
  not;
- the `foreach` rewrite ends with an unquoted `set(${ARCHS} ${FIXED_ARCHS})`,
  which drops empty entries: `;89` becomes `89` and `89;;86` becomes `89;86`,
  and that normalisation happens *before* the native-probe adoption test.

## Mutation halves

`results/mutate-1.log`: 18/18 caught. `results/mutate-2.log`: 18/18 caught.
Each line names the rewritten decision and the gate line that killed it, e.g.

```
CAUGHT   arch  rewrite matches 13X instead of 12X             FG660_ARCH cases=60 values=60 failures=5
CAUGHT   tgt   compress threshold becomes 12.9                FG660_TGT cases=34 values=34 failures=9
CAUGHT   inst  module install gets a version suffix           FG660_INST cases=30 values=30 failures=3
```

## Memory and runtime impact

No runtime path changes: the three modules are pure decision functions with no
allocation, no global state and no I/O, and nothing in the current binary calls
them yet. Generated C and objects, for the record:

| slice | source lines | generated C | object `-O1` | exports |
|---|---:|---:|---:|---:|
| `arch_` | 372 | 194 614 B | 267 712 B | 10 |
| `tgt_` | 224 | 182 483 B | 263 688 B | 4 |
| `inst_` | 274 | 180 774 B | 262 328 B | 10 |

Together: 870 lines of Goose, 557 871 bytes of C, 793 728 bytes of object.

Gate cost: `arch` 0.4 s, `tgt` 0.9 s, `inst` 2.9 s per run at `-O1` (the
install gate configures, builds and installs five times).

## Integration

The three modules were composed with the 107 accepted modules of
`tools/build-main-runtime.sh` outside the repository: `typechecked ok` (3756
specializations), 18 625 369 bytes of C, 4 777 536-byte `-O1` object, 998
globals, **0 duplicates**, 99 extern. Prefixes are free, no `RENAMES` entry is
needed, and the three modules also check and compile as a standalone unit. The
consumer wiring (feeding CUDA version, probe, options and ggml version in, and
using the arch list, flags, link set and installed names) is the remaining
P10-06 tail.

## Artifacts

```
source/cuda-arch-policy.goose             arch_ module
source/cuda-target-flags.goose            tgt_ module
source/cuda-install-layout.goose          inst_ module
tests/cases/{arch,tgt,inst}.tsv           the single input of both arms
tests/oracle_{arch,tgt,inst}.py           executable native oracles
tests/{arch,tgt,inst}_fixture.cpp         the two-arm comparison
tests/build.sh                            -O1 / -O0 gate build
tests/mutate.py                           12 mutations per slice, halves 1 and 2
tests/native/*                            byte-identical native inputs
results/gate-o1.log, gate-o0.log          gate output
results/mutate-1.log, mutate-2.log        mutation output
results/native-sha256.txt, hashes.txt     SHA-256 of inputs and of the ports
README.md                                 interfaces, integration, assumptions
```

No generated C, object, gate binary or `__pycache__` is part of the evidence:
`build/` is reproducible from the sources and `tests/build.sh`.
