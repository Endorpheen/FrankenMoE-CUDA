# INVALID RUN - A/B results must not be used as EXP-076 evidence

Status: `INVALID` - the live A/B run stored in this directory did not execute the EXP-076
asynchronous pinned-source code. It loaded the EXP-075 shared object instead.

## What was intended

The run was started with `BUILD_DIR=build/exp076-pin-async`, so it was meant to exercise the
EXP-076 asynchronous pinned-source registration implemented in
`work/llama.cpp-exp076-pin-async` (see `../REPORT.md`).

## What actually happened

The executable `build/exp076-pin-async/bin/llama-server` was copied from
`build/exp075-pinsrc-overlap` and was never relinked. Its `DT_RUNPATH` still named the EXP-075
`bin` directory, so the dynamic loader resolved `libggml-cuda.so.0.22.0` from EXP-075 at run time:

```
readelf -d build/exp076-pin-async/bin/llama-server | grep RUNPATH
  RUNPATH [/home/end0/AI/FrankenMoE-CUDA/build/exp075-pinsrc-overlap/bin:]

ldd build/exp076-pin-async/bin/llama-server | grep ggml-cuda
  libggml-cuda.so.0 => /home/end0/AI/FrankenMoE-CUDA/build/exp075-pinsrc-overlap/bin/libggml-cuda.so.0
```

Because `RUNPATH` is a per-object property, the *process* loaded the EXP-075
`libggml-cuda.so.0.22.0` (sha256 `27137a1a3ea0308c7712a3c15b944438e8c5ff2741d681f2ec685bd331ecd45a`)
even though `BUILD_DIR` pointed at `build/exp076-pin-async`. The EXP-076 async code never ran.

The two libraries are distinguishable statically:

```
strings build/exp075-pinsrc-overlap/bin/libggml-cuda.so.0.22.0 | grep -c "async %s"   -> 0
strings build/exp076-pin-async/bin/libggml-cuda.so.0.22.0   | grep -c "async %s"     -> 1
```

## Log evidence

Every server startup log in this directory (`results/e076b-*.log`, moved here together with the
`ab/` JSON responses) shows the EXP-075 startup line:

```
expert pinned source enabled: direct H2D from the mapped GGUF (budget 24576 MiB)
```

It lacks the `, async on` / `, async off` suffix that only the EXP-076 library emits
(`expert pinned source enabled: direct H2D from the mapped GGUF (budget %.0f MiB, async %s)`).
The log line, the release-counter field set and the log format are therefore the EXP-075 format.

The release counters recorded by the B-arm runs are EXP-075 numbers:

```
expert pinned source: 62742 direct ranges, 7442 split ranges, 14452 fallback ranges,
0 register failures, 17955 intervals released, 28968 physical regions released,
25769775104 bytes released, 0 bytes still pinned, 71896 covered sub-copies,
28968 registered sub-copies
```

## Consequence

- Both arms (A and B) resolved `libggml-cuda.so.0.22.0` from `build/exp075-pinsrc-overlap/bin`.
- The async OFF arm is effectively an EXP-075 split run, and the async ON arm never reached the
  EXP-076 code path.
- The counters and timings in this directory describe EXP-075 behavior, not EXP-076.
- This run must NOT be used as evidence for or against EXP-076. It is archived here only to keep
  the invalid data out of the valid results set.

## Fix

`build/exp076-pin-async` was relinked so that every executable and shared library under its `bin/`
resolves its libraries from `build/exp076-pin-async/bin`. See the "Build note: copied build dirs
carry the old RUNPATH" section in `../REPORT.md`.

## Files in this directory

- `A1..A5-*.json`, `B1..B5-*.json` - A/B HTTP responses (greedy, 48 tokens)
- `request.json` - the fixed prompt request used for every pair
- `e076b-A1..A5.log`, `e076b-B1..B5.log` - server startup and release-counter logs
- `e076b-A1..A5-mon.csv`, `e076b-B1..B5-mon.csv` - resource monitors captured during the run
