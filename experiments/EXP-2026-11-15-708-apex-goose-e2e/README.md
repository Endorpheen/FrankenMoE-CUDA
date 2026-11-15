# EXP-2026-11-15-708 — real Apex end-to-end on the integrated Goose runtime

First end-to-end run of Apex on the reconstructed Goose runtime, with the reference flags.
No synthetic tensors, no flint libraries, no old EXP244 binary.

## 1. Result

```
prompt_n   = 32768
prompt     = 855.71 tok/s
predicted  = 128 tokens at 18.21 tok/s
content    = 646 chars
sha256     = 9d90a010775f2e72fbea3fec7e0d34b8e57b52aab9dd16e90eb9cd5151ba7560
http       = 200, wall 45.3 s
```

**The content hash is `9d90a010775f2e72fbea3fec7e0d34b8e57b52aab9dd16e90eb9cd5151ba7560` -
byte-identical to the 2026-11-13 reference.** That is the strongest agreement statement
available: not a similar answer, the same answer.

## 2. Which runtime ran

The binary is `runtime/reconstructed/build/stage-run/llama-server`, and the libraries
resolved out of that same directory - verified by reading `/proc/<pid>/maps`:

```
build/stage-run/libggml-base.so.0      build/stage-run/libllama-common.so.0
build/stage-run/libggml-cpu.so.0       build/stage-run/libllama-server-impl.so
build/stage-run/libggml-cuda.so.0      build/stage-run/libmtmd.so.0
build/stage-run/libggml.so.0
```

No `llama.cpp-flint-cuda` entry appears. That matters because the first attempt at this run
did resolve there - the binary has no RUNPATH, so without `LD_LIBRARY_PATH` it silently picks
up the flint build and the run is *not* Goose at all. The reference profile records
`LD_LIBRARY_PATH` and 18 `GGML_GOOSE_*=1` variables for exactly this reason, and both are
reproduced in `env-run.json`.

## 3. Evidence that Goose code executed

`libllama.so.0.7.0` in the stage-run carries 327 `fg*` symbols. The server log records
1111 module markers across 77 distinct modules, led by:

```
    358 FG387
    323 FG401
    264 FG398
     23 FG147
     15 FG405
     15 FG404
     11 FG150
      7 FG199
      6 FG149
      4 FG201
      3 FG376
      3 FG200
```

FG401 at 323 calls and FG387 at 358 are the
highest; FG392/FG396/FG398 are the state and layout routes. A run that merely linked the
modules would show none of these.

## 4. Performance against the reference

| metric | reference 2026-11-13 | this run | delta |
|---|---:|---:|---:|
| prefill tok/s | 2228.76 | 855.71 | -61.6% |
| decode tok/s | 41.62 | 18.21 | -56.3% |

**Stated plainly: the throughput is lower than the reference.** The identity of the output
is exact and the execution path is Goose, but this is not yet a performance match. The
baseline document already explains this class of gap: the reconstructed tree does not carry
the full patch set of the accepted build, and its own `performance_qualification` field says
the prefill "is known to be below the accepted build".

## 5. Honest scope

Proven: Apex runs end to end on the integrated Goose runtime, produces the reference output
token for token, and executes Goose modules throughout prefill and decode.

Not proven: performance parity with the 2026-11-13 reference. Nothing in this run should be
quoted as an Apex speed figure until the patch set is reconciled.

## 6. Reproduction

```
argv-run.json   the 61 flags (reference set + --moe-cache-mib 3584)
env-run.json    LD_LIBRARY_PATH=stage-run plus the 18 GGML_GOOSE_* variables
request.json    32768 prompt tokens, 128 predicted, seed 123
server.log      the run, with the Goose module markers
response.json   the full response
goose-modules.txt  module marker counts
```
