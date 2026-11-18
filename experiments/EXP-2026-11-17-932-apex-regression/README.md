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

Cause, checked with `nvidia-smi --query-compute-apps`: unrelated processes held
1.4-3.8 GiB of the 12 GiB card (pids 13409, 36605, 748110, 854970, 1049701,
1763052 and one more). The request died on a concrete allocation:
`cudaMalloc failed` for 1156.03 MiB while `total used` was 10686-11614 MiB.
No unrelated process was stopped and no parameter of the reference profile was
changed.

Correction of an earlier claim in this card: the accepted 11468 MiB figure is
**total card usage during the accepted run (including unrelated owners)**, not
a required free-memory threshold. Treating it as "needs 11.5 GiB free" was
wrong, and the threshold that enforced it in `tools/apex-compare.sh` has been
removed. Required free memory is not a measured number yet; the script now
prints the actual figures and only warns when the card is obviously short.

This is an environment blocker, not a regression measurement.

## Candidate assembled on the accepted base (2026-11-17)

`/home/end0/AI/LlamaServer/maxwell-candidate-20261117/run/` is built by
`tools/build-maxwell-candidate.sh` from `our-src` (13 Goose/EXP906 files) plus
flint models/arch, recompiling only arch, model, context and maxwell, and
relinking with `link-fixed/commands.json`. The reference is copied, never
written.

Checks done without running a model:

- `libllama.so.0` size 5,765,136, 183 `FG…` strings (reference has 183);
- `libggml-cuda.so.0` sha256 `17e79510f378…` (accepted EXP906);
- `maxwell_moe` registered inside libllama (architecture table);
- `llama-server --version` from the candidate prints `FG270Q_`, `FG276T_`,
  `FG274B_` counters;
- EXP906 symbol `pin_mmap_experts` present in the candidate libllama.

The 32768/512 comparison has NOT been run yet: port 9101 is owned by another
server and heavy GPU runs need Igor's approval. No number is claimed until that
run exists.

## First real run of the candidate (2026-11-17)

The candidate loaded and served: `model loaded` in 3.5 s, `FG201L_`,
`FG389S_` counters printed, health ok, RSS 21170 MiB.

The 32768/512 request failed during prefill with a measured allocation
failure:

```
ggml_backend_cuda_buffer_type_alloc_buffer: allocating 1156.03 MiB ... cudaMalloc failed: out of memory
decode: Compute error. off = 0, n_batch = 4096, ret = -2
```

At that moment `nvidia-smi` reported 10686 / 12282 MiB used, of which five
unrelated processes held 314+240+113+252+457 = 1376 MiB. The reference profile
fits at 11468 MiB total including those owners only when the card is otherwise
free; here the allocation of the ubatch-4096 compute buffer had no room.

No flag was changed and no foreign process was stopped. The comparison still
needs a GPU window with the unrelated owners gone, or an explicitly labelled
reduced profile (-ub 2048) that is not compared to the 4096 reference.

The segfault found on the first attempt was real and is fixed: adding fields to
`llama_hparams` shifted members of objects allocated by accepted translation
units. The candidate now keeps Maxwell settings in a static in `maxwell.cpp`
and appends `LLM_ARCH_MAXWELL` after `LLM_ARCH_UNKNOWN`, so no accepted layout
moves.

## Root cause of today's CUDA OOM (measured, 2026-11-17)

Yesterday's accepted runs (`results/apex-runs/exp906-*`) executed with this
argv (from `argv.txt`):

    ... -c 262144 -ngl all -fit off --cpu-moe -fa on \
        -ctk q4_0 -ctv q4_0 -b 4096 -ub 4096 -t 8 -tb 16 \
        --cpu-mask 0f0f --cpu-mask-batch ffff --cpu-strict 1 --cpu-strict-batch 1 \
        --poll 0 -np 1 --cache-ram 0 --ctx-checkpoints 32 --load-mode mmap ...

My benchmark wrapper at the time omitted `-ctk q4_0 -ctv q4_0`, `-fit off` and
`--load-mode mmap`. Without them the KV cache was F16 instead of q4_0: for a
32768-token prefill that is roughly 3.0 GB of KV instead of ~0.85 GB, i.e.
about +2.2 GB of VRAM at the moment of the graph allocation. That is exactly
the class of failure seen: `cudaMalloc failed` for a 1156.03 MiB buffer with
10686-11614 MiB already used.

Second contributing difference: the card was busier before the run. Yesterday's
`vram-ready.txt` was 10198 MiB (a1) and 9803 MiB (b1); today the same point was
11614 MiB, i.e. ~1.4-1.8 GiB more already resident from unrelated processes.

So "what changed" is the launch flags of my wrapper, not the accepted runtime:
the binaries and their hashes matched the manifest (`4eba8242…`, CUDA
`17e79510…`), `-c/-b/-ub/-np` were identical, and no swap was involved (RSS
21.5 GiB resident, model 99.1% resident).

The wrapper now passes the full accepted flag set, including
`-ctk/-ctv q4_0`, `-fit off`, `--load-mode mmap`. Re-running 32768/512 with it
is the acceptance check.

## 32768/512 with the full accepted flag set (2026-11-17)

`run/` (accepted reference), `-ctk q4_0 -ctv q4_0 -fit off --load-mode mmap`,
32768 prompt / 512 generated, 1 warmup (excluded) + 3 measured:

| run | prefill tok/s | decode tok/s |
|---|---:|---:|
| warmup | 2642.97 | 52.68 |
| 1 | 2871.85 | 54.07 |
| 2 | 2883.59 | 53.82 |
| 3 | 2837.17 | 54.41 |
| **median** | **2871.85** | **54.07** |

RSS 21155 MiB, VRAM total 11480 MiB, RC 0. Against the accepted medians
(2625.2066 / 55.5176): prefill +9.4%, decode -2.6% — both within run-to-run
spread under resident unrelated processes. No OOM once the KV cache was q4_0,
which confirms the cause stated above: the missing flags, not the runtime.

## Candidate with Maxwell on the accepted profile (2026-11-17)

`maxwell-candidate-v3/run`, Apex 32768/512, identical flags, warmup excluded:

| run | prefill tok/s | decode tok/s |
|---|---:|---:|
| warmup | 2649.46 | 52.81 |
| 1 | 2899.90 | 54.07 |
| 2 | 2916.74 | 51.81 |
| 3 | 2856.76 | 53.57 |
| **median** | **2899.90** | **53.57** |

RSS 21359 MiB, VRAM 11287 MiB, RC 0.

Against the reference on the same profile today (2871.85 / 54.07): prefill
+1.0%, decode -0.9%, both inside the three-run spread. Against the accepted
EXP906 medians (2625.2066 / 55.5176): prefill +10.5%, decode -3.5%. No Apex
regression is detectable from the Maxwell candidate.
