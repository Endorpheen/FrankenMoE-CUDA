# EXP-2026-09-15-064: pinned ring depth 4 - build and model-free gate

Status: candidate built, model-free gate PASSED. No model and no server was started.
No commit was made. This is the build/gate record that precedes any A/B.

## Goal

Follow-up to the accepted depth-3 ring (EXP-2026-09-15-062). Build a depth-4
candidate that differs from `work/llama.cpp-exp062-ring3` by exactly one
compile-time constant (`GGML_CUDA_EXPERT_RING_SLOTS` 3 -> 4) and pass the same
model-free gate before any performance run.

## Single variable

- Source tree: `work/llama.cpp-exp064-ring4`, a copy of `work/llama.cpp-exp062-ring3`.
- Build: `build/exp064-ring4`, same configure flags as `build/exp062-ring3`
  (Unix Makefiles, Release, shared libs, CUDA arch 89, server on).
- The recursive tree diff reports exactly one differing file (`common.cuh`) and,
  inside it, exactly one differing line:

```
-static constexpr size_t GGML_CUDA_EXPERT_RING_SLOTS      = 3;
+static constexpr size_t GGML_CUDA_EXPERT_RING_SLOTS      = 4;
```

- `ggml/src/ggml-cuda/ggml-cuda.cu` is byte-identical in both trees
  (sha256 `fe2b22edf438ad3d1c4c756f645f404cac64441da5d706dc38ddbc2c548ef928`).
- `common.cuh`: ring3 `d7905127b866f3421a089879169a7e08ac9c2c3d613171b31aba4e5f175c8275`,
  ring4 `b0ca072cd012e7f60c2296b43ab9b83e1c958d0440af2903bc5cd982392f8ef5`.
- Full diff: `tree-diff-vs-ring3.txt` (sha256 `cd6b0a9ebe052595d4f9326052856d49fe51fef94c58438665b4ff44e162f636`).

## Build identity

- Configure: `cmake -S work/llama.cpp-exp064-ring4 -B build/exp064-ring4
  -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=ON -DGGML_CUDA=ON
  -DCMAKE_CUDA_ARCHITECTURES=89 -DLLAMA_OPENSSL=ON -DLLAMA_BUILD_SERVER=ON`
- The resulting `CMakeCache.txt` equals the depth-3 cache byte for byte after
  normalizing only the build path
  (normalized sha256 `fd924ecccd45f6858fe25a5bdb23a62bc86cb248ac4ed4f109c6dbc3b20bdf1b`),
  so the configure flags are reproduced exactly. That also proves the two arms
  differ by the constant alone.
- Build: `cmake --build build/exp064-ring4 --target llama-server -j10`, exit 0,
  wall clock 4:04.92, max RSS 693964 KiB. Full log: `build-log.txt`.
- Binary: `build/exp064-ring4/bin/llama-server`,
  sha256 `2ecb65a5752bc706c9b013fc35e7799d23c02e24ce605674efa82fa00e070026`.
- Libraries: `libggml-cuda.so` `88733ee7e4680d152245c59252f81be693ba47a855b8af60853a24f22b2ab98d`,
  `libggml.so` `de1eb8d07a85da10858f45c4035e58df5615d7f714c592c746b3991aaa26ddd6`,
  `libggml-base.so` `690bd2d0e7a31baa21f8db4069c67c6fe527a0cb8bcc4864736c6486b6684ee4`.
- See `build-summary.txt` for the same data in plain text.

## Model-free gate

The depth-3 harness `results/archive/EXP-2026-09-15-062/tests/test-exp062-ring.cpp`
was adapted to depth 4 and stored as `tests/test-exp064-ring.cpp`
(sha256 `87060ee1e9a35ad24d4c3a4014203d75db23e31d81b3ca05a9c8086d9c1830d2`):

- the context mirror arrays are now `[4]` (slots, events, pending);
- the range set covers every expert 0-13 and adds `H reuse wrap 0-8`, so the
  8 staged calls total 12 chunks, i.e. exactly three full passes over the four
  slots; the wrap check now requires `ring_n_chunks >= 12`;
- no "uncovered expert" check is present - that check was obsolete once every
  expert is written by some range; the expert 12 padding-tail spill check and
  the sentinel guards are kept.

Run script: `tests/run-model-free-exp064.sh`
(sha256 `c0ff8446b57c32b77a7df86d8747c865d09c85806470fdd2d03c4d548f4b10f8`),
8 runs: ring default (ON), ring ON, ring OFF, ring alloc-fail, then the same
four modes for the scheduler branch. Result: 0 FAIL lines in all logs.

- All ring checks PASS: byte equality, padding tail, sentinel guards, flag
  gating, ring counters (8 calls / 12 chunks), three full 4-slot reuse wraps,
  capture guard, pinned-memory teardown.
- Alloc-fail mode confirms the all-or-nothing fallback and `ring_state == -1`.
- Scheduler output sha256 is
  `39505119b144ea286e865119b397eecff75578881c24c4aa1cec9bd642e91510`
  for default, ON, OFF and alloc-fail, i.e. it reproduces the accepted
  EXP-038/040/041/043/062 reference.
- Harness binary: `tests/test-exp064-ring`,
  sha256 `707be090ca2ad833cd847ba3b2ae1d58cf5b0b2c2779633fcaf5365a56f88c32`
  (links against `build/exp064-ring4/bin`).

Because the mirror reads `ring_n_calls`, `ring_n_chunks`, `ring_state` and the
context stream pointer at the depth-4 offsets, a passing run also proves that
the libraries were built with four slots; a stale depth-3 library would have
failed the capture guard and the counters.

## Files created

- `work/llama.cpp-exp064-ring4/` (tree copy, only `common.cuh` changed)
- `build/exp064-ring4/` (fresh build directory)
- `results/archive/EXP-2026-09-15-064/REPORT.md` (this file)
- `results/archive/EXP-2026-09-15-064/build-summary.txt`
- `results/archive/EXP-2026-09-15-064/build-log.txt`
- `results/archive/EXP-2026-09-15-064/tree-diff-vs-ring3.txt`
- `results/archive/EXP-2026-09-15-064/tests/test-exp064-ring.cpp`
- `results/archive/EXP-2026-09-15-064/tests/run-model-free-exp064.sh`
- `results/archive/EXP-2026-09-15-064/tests/test-exp064-ring`
- `results/archive/EXP-2026-09-15-064/tests/log-ring-{default,on,off,fail}.txt`
- `results/archive/EXP-2026-09-15-064/tests/log-sched-{default,on,off,fail}.txt`
- `results/archive/EXP-2026-09-15-064/tests/log-sched-sha.txt`
- `results/archive/EXP-2026-09-15-064/tests/out-sched-{default,on,off,fail}.bin`

## Untouched

- `patches/ring-depth-3.patch` sha256 `3da9825d5e321120806b40dbf9d55209b339eac99501af47c68540af28b5371c`.
- Depth-3 tree and build: `common.cuh` `d7905127...`, binary `f4b13f6b...`.
- Depth-2 tree and the working build were not modified.
- No model was loaded, no server was started, no request was sent, no commit
  was made.

## Memory note (design, not measured)

Depth 4 raises the pinned ring from 3 x 16 MiB to 4 x 16 MiB, i.e. +16 MiB
host pinned (64 MiB total). Nothing is allocated on the device. This is a
design consequence of the constant, recorded here for the A/B design that may
follow.

## Next step

Not part of this task: a paired A/B of `build/exp064-ring4` versus the accepted
depth-3 runtime on the fixed 395-token prefill request, using the protocol of
EXP-2026-09-15-062. That run needs explicit approval and a separate experiment.
