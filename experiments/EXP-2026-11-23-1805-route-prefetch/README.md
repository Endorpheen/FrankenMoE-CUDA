# EXP-2026-11-23-1805 — Expert prefetch from a one-layer-ahead route prediction

Status: **ACCEPTED** (intermediate step toward 100 tok/s decode).

Base: EXP-1786 checkpoint `build/exp1786-split-planes/checkpoint/frankenmoe-infer`
(SHA-256 `2f99a9ebaa4987cdfa75e325f9fbd4147c73224120fa67c2e790351a4b678b59`, worktree
`pure-goose-inkling-mini`, source base `db08ad6c2` plus the accumulated local artefacts).
Only `cuda-driver.o` changes; every image header, `main.o`, the PTX images and the
Goose objects in `libfranken-inference.a` are the EXP-1786 ones.

## Hypothesis

In decode the cold-expert transfer of layer L starts only after the route of layer L is
read back, so it is fully exposed: EXP-1802 measured 2.69 ms per step from the end of
the region before the expert graph to the end of the uploads, which equals the pure
transfer time of the 32.2 decode misses per token (61.6 MB at ~22.4 GB/s). Applying the
router of layer L+1 to the FFN input of layer L predicts most of those misses one layer
early. Copying the predicted experts while layer L computes hides part of the transfer
without touching any arithmetic: the model always uses its own routes, the prediction
only decides which expert weights are copied early.

## Offline evidence (no GPU work beyond one diagnostic dump)

1. The EXP-1772 route replay is reproduced exactly (20,440 calls, 16,459 fills,
   31,474,040,832 bytes, byte-identical JSON). Additional causal eviction policies
   (ARC, S3-FIFO, LFU, decayed LFU, TinyLFU with staging bypass) gain at most -2.55 % of
   traffic (decayed LFU, half-life 32 tokens), i.e. <= ~36 ms of copy time: the eviction
   policy has no meaningful headroom at 6290 MiB (`results/causal2-replay.json`).
2. A diagnostic `cuda-driver.o` (`source/route-dump.patch`) dumped, for the measured
   request, the ids, the FFN input and the post-attention residual of every decode layer.
   The answer stayed exact. The GGUF router reproduces the recorded top-8 set on
   99.995 % of the 20,440 calls.
3. Predictor "router_{L+1} on FFN input of L": recall of the actual **misses** is 67 % at
   top-8, 78 % at top-10, 85 % at top-12, 91 % at top-16 (`results/route-predict.json`).
   Using the post-FFN output of layer L instead gives 82.4 % route recall at top-8 against
   81.3 %, so the earlier input is the better trade (longer window, same accuracy).
4. A FIFO PCIe timing model calibrated on the control (76.98 modelled vs 77.23 measured)
   predicts ~87 tok/s at k=8 (`results/prefetch-sim.json`). Inserting prefetched experts
   at the cold end of probation is worse; touching predicted residents does not matter.

## Mechanism (`source/cuda-driver.patch`, kernel `source/pred2.cu`)

- The router device pointers are learned from the dense F32 256-row matvecs seen by
  `fg_cuda_plane`, in first-seen (layer) order; the layer of a routed FFN call is learned
  from its `up_exps` host pointer. No new VRAM beyond 1 KiB of logits and a counter.
- At the entry of the decode routed FFN of layer L (route ids already read, so the FFN
  input is complete) one fused kernel `goose_pred_route` runs on a non-blocking side
  stream: 256 blocks compute the logits, the last block ranks them and writes the top-8
  ids and a sequence number into host-mapped pinned memory.
- After the FFN of layer L is submitted, the host polls the sequence number, sorts the
  eight ids and runs them through the regular `prepare_expert_offsets` / one-touch SLRU
  path for layer L+1, then `submit_expert_stage`. The copies therefore use the normal
  slot-release fences and the normal `expert_planes_ready` / `expert_uploads_ready`
  events, which the FFN of layer L+1 already waits on.
- Default on (rule 10); `GOOSE_EXPERT_PREFETCH=0` disables it.
  `GOOSE_EXPERT_PREFETCH_K` must stay 8: the compact one-touch prepare accepts exactly
  eight routes, and any other value makes prepare fail, which resets the whole cache
  (measured 28 tok/s at k=6/10/12 with an exact answer).

## Acceptance A/B (fixed profile, 32K input / 512 output, ctx 262144, cache 6290 MiB)

Each arm is a fresh server, one warm-up request and one measured request, without a
profiler, run strictly one after another (`scripts/arm.sh`, `scripts/report.sh`).
Model resident 100 % in page cache before the series.

| Pair | Decode A -> B, tok/s | Prefill A -> B, tok/s | Full request A -> B, s |
| --- | ---: | ---: | ---: |
| A1 / B1 | 77.62 -> 79.99 | 3082.94 -> 3072.37 | 17.301 -> 17.149 |
| B2 / A2 | 79.70 vs 76.19 | 3082.29 vs 3054.70 | 17.130 vs 17.524 |
| Mean | **76.91 -> 79.85 (+3.8 %)** | 3068.8 -> 3077.3 (+0.3 %) | 17.413 -> 17.140 (-0.27 s) |

- All four responses: 512 ids and the content identical to the EXP-1786 reference
  `apex-ab/candidate/r2.json`, `cache_n` 0.
- Peak sampled VRAM 8770 MiB in every arm (0 %); VmHWM 21,486,192-21,486,944 kB (0 %);
  VmSwap 0 and system pswpout delta 0 in every arm.
- Raw data: `results/ab-measurements.tsv`, `results/{A1,B1,B2,A2}/`.

## What limits the gain (measured, for the next step)

- Same binary, prefetch on/off with graph-stream timing events (diagnostic only):
  FFN span including copy waits 2289 -> 1914 ms per request decode (-375 ms), the span
  from FFN end to the next FFN entry 4329 -> 4468 ms (+139 ms, host work of the prefetch
  delays the next region submission by ~7 us per layer).
- Critical (regular) copies fall from 16.5k to 6.6k experts per request as predicted, but
  the FFN of L+1 still waits for the whole prefetch wave, including the ~40 % wrong
  experts, because the wave shares the FIFO expert stream and the readiness events.
- Rejected variants on the way: deferring the prefetch submission to the next blocking
  read (79.00), a separate copy stream with per-expert events (76.34), per-expert events
  on the shared stream (76.76): per-expert batches cost more than they save.
- Kernel time is ~9.6 ms per token, so 100 tok/s also needs kernel and host-bubble
  reductions, not only hidden transfers.

## Artefacts

- Checkpoint (outside the repository, byte-reproduced from `source/`):
  `/home/end0/AI/claude-apex-100/checkpoints/exp1805-route-prefetch/`
  - `frankenmoe-infer` `844c5a161965463ad21fd5f6fa926c6ee7c3a2db0d43dfb5cb315b4bcb657f4c`
  - `libfranken-inference.a` `59fc73ae03a45eae54cc52a6c92617a2dd9b8b0609f1072b60208760e4d62473`
  - `cuda-driver.o` `d1182e1035f8cefd4c692e42daebfa1e1b173687fc73e7d1489c4bfafec5fca5`
- Rebuild: copy EXP-1786 `build-inputs/`, replace `cuda-driver.c` with the patched file
  and add `source/pred-image.h`, then follow the EXP-1786 relink recipe (gcc-13 13.4.0,
  same flags); `cmp` against the checkpoint passes for the object, the archive and the ELF.
- Kernel: clang-18 18.1.8 `-x cuda --cuda-device-only --cuda-gpu-arch=sm_89 -nocudainc
  -nocudalib -O2 -S`, ptxas 12.4.131 `-arch=sm_89`: 38 registers, no spills.
- Offline scripts: `scripts/replay-causal2.py`, `scripts/route-predict.py`,
  `scripts/route-predict2.py`, `scripts/prefetch-sim.py`, `scripts/slru_core.py`.
  The 335 MB decode dump is not committed; its SHA-256 is in `results/dump-SHA256SUMS`.

Rollback: use the EXP-1786 binary, or run this one with `GOOSE_EXPERT_PREFETCH=0`.
