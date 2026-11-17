# EXP-2026-11-17-932 — Apex regression with Maxwell present

Status: PENDING (runs only after the Maxwell download completes)

Igor's addendum: adding Maxwell must not regress the models FrankenMoE already
serves. Only Apex-35B is in the required set right now; the shared paths must
stay universal for the rest.

## Reference (identical conditions)

Model `Apex-35B-V1.0-Q4_K_M.gguf`, profile from
`apex-independent-fast-20261116/launch-apex.sh`: `-c 262144`, `-ngl all`,
`--cpu-moe`, `-fa on`, `-ctk/-ctv q4_0`, `-b/-ub 4096`, `-t 8 -tb 16`,
cpu-mask 0f0f/ffff strict, `--poll 0 -np 1 --cache-ram 0 --ctx-checkpoints 32`,
`--load-mode mmap`, `--spec-type none --jinja --temp 1 --top-p 0.95 --top-k 20
--min-p 0`, `--moe-cache-mib 4992`, port 9101.

Accepted B-arm medians (`measurements.json`, verdict ACCEPTED):

| axis | reference |
|---|---|
| prefill | 2625.2066 tok/s (2489.37–2906.58) |
| decode | 55.5176 tok/s (51.76–57.53) |
| RSS | 21350.904 MiB |
| VRAM | 11468 MiB |
| answer sha256 | c2ff42cc1219ea4dfe6d69f02e008d0f90c0fc56260736a0aafb82cec362924f |

## Procedure (no accepted artefact is overwritten)

1. Build a candidate `libllama.so.0` in a new directory from
   `runtime-build-tree-v8`, recompiling only what the change touches:
   `llama-arch`, `llama-model`, `llama-context`, every `models/*.o` (models.h
   changed) and the new `maxwell.o`, with the flags recorded in
   `build-cuda/build.ninja` (`-O3 -DNDEBUG -std=gnu++17 -fPIC
   -fvisibility=hidden ... -fopenmp` and the same include set).
2. Link with `franken-recovery-20261116/link-fixed/commands.json`, replacing
   those objects and adding `maxwell.o`, into a fresh directory; verify with
   `ldd` that it resolves `$ORIGIN` ggml libs.
3. Run the Apex profile through `tools/apex-test.sh` (flock, real pids, one
   server) with the run directory pointed at the candidate, 3 repeats.
4. Compare medians on all four axes and the answer hash. A regression blocks
   Maxwell acceptance until fixed.

## What already makes a regression unlikely, and why it is not enough

The shared edits are additive: one enum row, one name row, one factory branch,
one rope-type case, two new hparams fields read only by `maxwell_moe`, and an
OR in the node-budget condition that only matches `LLM_ARCH_MAXWELL`. No
existing switch changed its behaviour. That is an argument, not a measurement;
the four-axis run above is the evidence the rule requires.

## Second model

Not selected by Igor for this acceptance round (Apex only). Qwen3.8-Flash-Next
stays available with its own reference (7.25 tok/s cold, 8.96 tok/s warm) for a
later regression round.

## Attempt on 2026-11-17 (blocked by the environment)

The candidate build with Maxwell was started with the exact reference flags.
The 32K/512 request failed with `CUDA error: out of memory` at prefill.

Cause, checked with `nvidia-smi --query-compute-apps`: seven unrelated
processes hold about 3.8 GiB of the 12 GiB card (pids 13409, 36605, 748110,
854970, 1049701, 1763052 and one more). The reference profile needs about
11.5 GiB, so it cannot fit while those owners are resident. No unrelated
process was stopped and no parameter of the reference profile was changed.

This is an environment blocker, not a regression measurement. The regression
run still needs a GPU window where the other owners are not resident.
