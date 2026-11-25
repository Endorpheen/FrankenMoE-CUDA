# EXP-2026-11-25-1995 — Bonsai prefill loss map and 5000 tok/s engineering budget

**Measured baseline, engineering target open.** This card uses the
EXP-1992 64x64 PQ2 MMQ, EXP-1991 V32 GDN, and EXP-1988 single-buffer
async Flash path on RTX 4070. A separate executable only adds three
NVTX ranges and per-phase process CPU accounting. The existing
CUDA images and computation remain unchanged.

## Conditions and measurements

Same local `Ternary-Bonsai-2.5-27B-PQ2_0-mtp.gguf`, Q4_0 K/V, 36864
context, ubatch 1024, one CUDA device, CPU mask 0f0f, greedy and EOG
suppressed, MTP off. Prefixes of the exact EXP-1977 IDs. One diagnostic
profile per length, 16 returned IDs. Startup, prefill and decode use
separate NVTX ranges. These profiling runs are evidence for attribution
and are outside uninstrumented A/B scoring.

| Metric | 4096/16 | 32768/16 |
| --- | ---: | ---: |
| Prefill wall | 6.936457 s | 68.834911 s |
| Measured prefill tok/s | 590.50 | 476.04 |
| Target at 5000 tok/s | 0.819200 s | 6.553600 s |
| Time to recover | 6.117257 s | 62.281311 s |
| GPU timeline union in prefill | 6.930516 s | 68.742679 s |
| GPU idle in prefill | 5.940 ms | 92.232 ms |
| GPU copy time in prefill | 0.050 ms | 0.117 ms |
| CPU time charged to process during prefill | 6940.266 ms | 69049.745 ms |
| Decode wall, 16 returned IDs | 978.529 ms | 1337.935 ms |
| Model-load wall | 1288.241 ms | 1223.281 ms |
| Resident payload upload on GPU timeline | 867.921 ms | 816.415 ms |
| Peak process VRAM (sampled) | 8960 MiB | 8960 MiB |
| Peak board VRAM (sampled) | 11427 MiB | 11420 MiB |
| System swap-out delta | 0 | 0 |

All 16 long-request output IDs match the first 16 IDs of the EXP-1977
PrismML 32768/512 result. This is a prefix check. It leaves the full
512-output gate open. The exact 4096 IDs are saved as an additional
native control; EXP-1985 uses 4097 inputs and is a different request.
No shared Apex or Inkling path was changed. Peak RSS was not sampled
in these two traces; the earlier unprofiled short A/B has RSS evidence.

### CPU and transfers

At 32768, the CUDA API intervals on the main thread union to 68.809 s,
but only 91.997 ms intersects GPU-idle intervals. The inclusive API
charges are 53.356 s in launches, 12.067 s in synchronous H2D calls,
3.316 s in context synchronization, and 69.3 ms in D2H calls. The GPU
copy engine processed just 662,460 H2D and 993,280 D2H bytes during
prefill, taking 77.7 and 39.4 microseconds. The large synchronous API
wall charge is concurrent with GPU work and largely reflects waiting
behind queued work. It is not PCIe data service time or an additive
speed opportunity. The process CPU counter also overlaps GPU time.
The trace does not resolve which driver stacks are spinning.

At 4096, CUDA API intervals intersect 5.887 ms of the 5.940 ms GPU gap.
The 32768 OS Runtime trace attributes 1.033 ms to main-thread locks
at the capture threshold, including just 1.7 microseconds in GPU
gaps. Linux perf samples are unavailable to this user
(`perf_event_paranoid=4`). CPU attribution comes from CUDA API/OSRT
timelines and process CPU clocks. Nsight Compute reports
`ERR_NVGPUCTRPERM`; DRAM utilization, SM issue stalls and tensor-pipe
saturation are not measured in this card. No OS or driver settings
were changed.

### Context scaling

| Ending input ID offset | 4K block wall, ms | Attention stage, tiles, merge, ms |
| ---: | ---: | ---: |
| 4096 | 6878 | 210.1 |
| 8192 | 7314 | 629.2 |
| 12288 | 7792 | 1100.9 |
| 16384 | 8288 | 1595.7 |
| 20480 | 8794 | 2100.9 |
| 24576 | 9347 | 2651.7 |
| 28672 | 9885 | 3191.2 |
| 32768 | 10537 | 3783.9 |

Milestone windows use millisecond-rounded CLI logs. NVTX phase totals
use exact timestamps. The last block adds about 3.66 s of wall versus
the first, with attention adding about 3.57 s. This is dense causal
QK and PV over a longer live KV cache, plus repeated stage, tile,
partial writes and merges. The time growth is GPU work. This trace
does not equate every attention millisecond with avoidable overhead.

## Arithmetic feasibility check

The GGUF trunk has **24,326,963,200 PQ2 linear coefficients** across
400 matrices, including 17.113B in the three dense FFN projections.
The single-budget final prompt token currently uses scalar matvec, so
the measured batched MMQ work is `2 * 24,326,963,200 * (N-1)` ops:
199.238 TF operations at 4096 and 1594.243 TF at 32768.
Dense causal attention over 16 layers has `4 * N*(N+1)/2 * 24 * 256 * 16`
logical ops, or 3.299 and 211.113 TF. These counts omit masked dummy
work, quantization, softmax, recurrent work, and launches. A stored
MoE tensor count is not the active work per token; Apex speed cannot
be extrapolated from the relative size of the two weight files.

The **required average MMQ rate alone** at 5000 tok/s is about
243.27 TOPS. The GPU delivered about 36.06 equivalent TOPS on the
32768 MMQ bracket. Hardware bounds use 46 SMs and the recorded
2.775 GHz median at high GPU utilization. The sampled maximum was
2.835 GHz. Ada dense per-SM rates are 512 FP16-to-FP32, 1024
FP8-to-FP32, 2048 INT8 and 4096 INT4 operations per cycle, scaled
from NVIDIA's Ada SM and dense reference tables. At the median these
are 65.36, 130.71, 261.43 and 522.85 TOPS respectively. This is
peak instruction-rate arithmetic, not a benchmark or deadline
guarantee. The official RTX 4070 boost is 2.48 GHz; the short
wall samples cannot guarantee 2.775 GHz under a different implementation.
[NVIDIA RTX 4070 specifications](https://www.nvidia.com/en-us/geforce/graphics-cards/40-series/rtx-4070-family/).
[Ada architecture and throughput tables](https://images.nvidia.com/aem-dam/Solutions/Data-Center/l4/nvidia-ada-gpu-architecture-whitepaper-V2.02.pdf).

| MMQ + attention arithmetic | 4096 optimistic floor, ms | 32768 optimistic floor, ms |
| --- | ---: | ---: |
| Dense FP16→FP32 + FP16→FP32 | 3098.946 | 27623.076 |
| Dense INT8 + FP16→FP32 | 812.598 | 9328.386 |
| Dense INT8 + FP8→FP32 | 787.357 | 7713.308 |
| Dense INT4 + FP16→FP32 | 431.540 | 6279.271 |
| Dense INT4 + FP8→FP32 | 406.299 | 4664.193 |
| Entire prefill target | 819.200 | 6553.600 |

The first three arithmetic routes have no credible path to the long
target through tuning alone. At stock clocks even dense INT4 plus
current FP16 attention has a 7026.201 ms two-stage floor. At observed
median clocks that pair leaves only 274.329 ms for all other work.
The furthest plausible arithmetic envelope shown here requires a
faster numeric route for both dominant stages or a verified reduction
in work. INT4 and FP8 are hypotheses with accuracy gates; the table
does not assume their outputs are acceptable.

Scanning all 6,461,849,600 packed trunk bytes found 7,968,364,988
zero coefficients (32.75%) and no code 3 in nonzero-scale blocks.
**60.30% of contiguous groups of four require a residual** to
represent them with one exact 2:4 group. At least 4,882,417,147
nonzero coefficients would need the residual path. Simply dropping
weights is ruled out. Any sparse candidate must include the cost
of its exact residual, or use a separately validated arithmetic
representation. [Actual weight census](weights.json).

## Per-stage budget and required savings

The table below is a transparent allocation of the exact target,
not an invented forecast. Start with optimistic analytic floors
for INT4 MMQ, FP8-to-FP32 attention, and the F32 decay/beta dot
products at the median trace clock. For a stage with no derived
floor here, zero is an optimistic lower bound, not a claim of
free execution. Distribute the remaining slack in proportion to
each stage's measured time above its floor:
`budget_i = floor_i + (measured_i - floor_i) * lambda`, where
`lambda = (target - sum(floors)) / (measured_wall - sum(floors))`.
Thus the rows sum to 819.2 and 6553.6 ms. If a candidate cannot
meet its envelope, reallocate from measured savings elsewhere.
Recompute after every accepted change. Values are in milliseconds.

| Stage | 4096 measured | 4096 budget | 4096 recover | 32768 measured | 32768 budget | 32768 recover |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| PQ2 batched matrix | 5505.840 | 700.745 | 4805.095 | 44213.409 | 4231.654 | 39981.755 |
| Attention stage, tiles and merge | 221.315 | 37.472 | 183.843 | 15263.486 | 2007.160 | 13256.326 |
| F32 decay and beta | 348.516 | 27.286 | 321.230 | 2771.790 | 125.582 | 2646.207 |
| GDN | 171.573 | 10.703 | 160.871 | 1369.066 | 39.330 | 1329.736 |
| FWHT | 150.526 | 9.390 | 141.136 | 1203.580 | 34.576 | 1169.004 |
| SwiGLU | 117.269 | 7.315 | 109.954 | 935.722 | 26.881 | 908.841 |
| Normalization | 112.225 | 7.001 | 105.224 | 848.254 | 24.368 | 823.886 |
| F16 activation packing | 86.839 | 5.417 | 81.422 | 693.866 | 19.933 | 673.933 |
| Causal convolution | 69.757 | 4.351 | 65.405 | 559.132 | 16.062 | 543.070 |
| Residual adds | 61.284 | 3.823 | 57.461 | 492.609 | 14.151 | 478.458 |
| Attention split and gate | 23.022 | 1.436 | 21.586 | 181.813 | 5.223 | 176.590 |
| RoPE and Q4 cache writes | 20.635 | 1.287 | 19.348 | 165.275 | 4.748 | 160.527 |
| GPU idle | 5.940 | 0.371 | 5.570 | 92.232 | 2.650 | 89.582 |
| Scalar PQ2 projections | 39.781 | 2.482 | 37.299 | 41.733 | 1.199 | 40.535 |
| Final vocabulary projection | 1.740 | 0.109 | 1.631 | 1.749 | 0.050 | 1.699 |
| Embedding | 0.144 | 0.009 | 0.135 | 1.080 | 0.031 | 1.049 |
| GPU transfer | 0.050 | 0.003 | 0.047 | 0.117 | 0.003 | 0.114 |

Ignored precision and scheduling overheads make this an optimistic
feasibility gate, not proof of reproducible 5000. In the long envelope
MMQ must sustain roughly **377 effective TOPS** including its overhead,
and attention roughly **105 effective TOPS** including conversion,
softmax and merge. The current 36.06/13.83 effective TOPS are measured
from the above logical work divided by attributed GPU time.
Anything beyond the model-load range is already covered by the table.
Cold upload is outside the prefill target.

## Priorities and technical checks

1. **PQ2 MMQ, 44,213 ms on 32K, required recovery 39,982 ms.**
   The selected 64x64 tile rebuilds half weights at every CTA K-block.
   A sparse per-CTA clock probe on real QKV, FFN up and down matrices
   measures 52.4–53.2% of sampled CTA cycles in weight loads, unpack and
   stores; 8.3–8.4% issuing input copies; 0.9% waiting and reaching the
   stage barrier; 37.7–38.4% in matrix loads, MMA and the end barrier.
   The probe raises median kernel times by 1.3–2.0% and preserves bytes.
   These are clocks inside sampled CTAs, not additive global wall
   milliseconds or hardware event counters. The experiment proves the
   dominant in-kernel region and leaves memory-vs-ALU details open.
   Full FP16 expansion already lost in EXP-1993; compact on-demand
   planes saved at most 2.4% including preparation.
   First qualify compact integer Tensor Core arithmetic with PQ2 scales,
   activation packing, tails, and independent dots. INT8 is a diagnostic
   stepping stone. Screen INT4 or another architecture-level arithmetic
   reduction with exact weights and calibrated activation/residual
   precision. Measure effective TOPS for the real QKV and FFN tile
   families before spending a full-model run. Reject a route whose
   measured ceiling cannot provide the 32K budget even under an
   optimistic remainder. Treat any explicit mixed-precision fallback
   and its latency as part of the benchmark.
2. **Long attention, 15,263 ms, required recovery 13,257 ms.**
   Keep current FP32 Flash as a correctness control. The new layout
   should keep Q and KV resident, avoid recopying past rows for each
   microbatch where a bounded persistent/paged staging scheme fits,
   and skip causal-empty tile work by construction. Quantify the cost
   with tiles at 4K/8K/16K/32K and fixed query counts. For the long
   deadline test lower-precision QK/PV transport/operands with FP32
   accumulation or a verified alternative computation schedule.
   Measure the whole attention chain, including conversion, masking,
   softmax, Q4 KV storage, and merge. Gate every shape by L2 and
   continuation IDs. EXP-1981 and EXP-1985 already demonstrate that
   isolated speed or small L2 is insufficient to preserve IDs.
3. **F32 decay/beta, 2772 ms, required recovery 2646 ms.**
   The code launches independent warp dot products for each token/head
   over 5120 entries of each F32 weight plane. Batch alpha and beta as
   a single resident two-output projection and fold the learned decay
   and sigmoid epilogue. Compare against the original dot products
   before assigning a fused or mixed-precision path. If arithmetic is
   still F32, the dense arithmetic floor at the observed clock is about
   47 ms. The 125.6 ms envelope includes its launch and activation terms.
4. **GDN, 1369 ms, required recovery 1330 ms, plus causal convolution
   559 ms.** Existing V32 chunks preserve a persistent recurrent state
   but read and write projected data across separate kernels. Test a
   Bonsai-only QKV+convolution/GDN pipeline and measured different
   chunk/prefetch schedules. Record real scene gates, initial nonzero
   state, final state, scalar decode continuation, short tails, and
   reset/replay. Put convolution staging into the recurrent consumer
   if it reduces traffic while preserving accumulation order and
   step semantics. Current images for other models remain the controls.
5. **Activation and transform chain.** FWHT, SwiGLU, normalization,
   activation packing, residuals and attention gate total 4356 ms. The corresponding trace
   names and exact times are recorded above. Tensor staging builds
   intermediate buffers on every layer. Try model-local residual+norm,
   norm+signed FWHT+pack, and SwiGLU+down-projection preparation fusions
   in that order by measured saving. Keep exact signed rotation and
   GDN head permutation. Include independent projection checks for
   each fusion and record any precision change before a model A/B.
6. **KV positions, terminal token and remaining transfers.** Fuse RoPE
   and cache writes only after the larger gates, preserving physical
   Q4 row bytes, causal windows and position indexing. Run prefill
   with a last-row output selection inside the batch to remove the
   separate final scalar trunk pass. The vocabulary projection still
   runs once; compare final logits before accepting. Use a resident
   schedule/CUDA Graph only if a fresh trace shows significant
   GPU-idle benefit. The present GPU gaps give a conservative upper
   bound of 92.2 ms on removing all host-side gaps in 32K prefill.

## Measurement loop

- Keep the frozen IDs, model, precision mode, ctx 36864, ubatch 1024,
  Q4_0 K/V, request sampler, and single-response timer fixed for the
  reference. Record build, source and CUDA image hashes. Change one
  execution subsystem per A/B. Compare like precision and keep the
  short 4096/16 experiment separate from 32768/512 acceptance.
- For each proposed kernel, keep a CPU arithmetic or current CUDA
  reference appropriate to the mechanism, test masked and short
  tails, then run unprofiled native A–B–B–A with the full 4096/16
  workload. Report delta in milliseconds, all greedy IDs, selected
  logits, VRAM/RAM peaks, swap and decode. Compare against the
  matching PrismML profile when claiming parity. Update the
  remaining budget with `new_wall_ms - N/5`; do not add isolated
  kernel wins to estimate model throughput.
- Before promoting long-context features, run the same request
  at 32768/512 and record the fresh complete answer, TTFT, prefill,
  decode, memory and swap. Validate the original 4097/16 and
  1025/16 short fixtures as well, including known close token
  decisions. Any changes to shared CUDA, state or scheduling
  require the existing exact-output and measured 32K/512 regression
  gates for both Apex and Inkling. Bonsai-specific experiments are
  isolated here. Keep plain decode metrics to compare identically
  with existing PrismML and disclose any future MTP settings.

## Reproduction

From the repository root with the EXP-1992 verified build and metadata
inventory available:

```sh
exp=experiments/EXP-2026-11-25-1995-bonsai-prefill-budget
base=build/bonsai-mmq-wide-1992-default
out=build/bonsai-budget-repro
model="$HOME/AI/LocalAI/Models/Ternary-Bonsai-2.5-27B-PQ2_0-mtp.gguf"
bash "$exp/source/build.sh" "$out"
python3 "$exp/source/capture.py" "$out/4096" "$base" "$out/bonsai-profile" "$model" 4096
python3 "$exp/source/capture.py" "$out/32768" "$base" "$out/bonsai-profile" "$model" 32768
python3 "$exp/source/analyze.py" "$out/4096" "$out/32768" > "$out/trace-summary.json"
OPENBLAS_NUM_THREADS=1 python3 "$exp/source/count-weights.py" build/bonsai-integration/inventory.json > "$out/weights.json"
python3 "$exp/source/budget.py" "$out"
```

Compile the CTA clock probe by generating PTX with the same clang-18
and ptxas options as EXP-1992 after running
`source/mmq-instrument.py "$base/pq2-mmq-wide.cl"`. Run
`source/check-mmq-clock.py build/bonsai-integration
build/bonsai-integration/inventory.json /path/to/mmq-budget.cubin`.
Raw Nsight reports, SQLite captures and NVML samples are local under
`build/bonsai-prefill-budget-1995`. Their hashes and settings appear
in [trace summary](trace-summary.json) and [build hashes](build.sha256).
[Machine-readable budget](budget.json). [CTA clock data](mmq-clock.json).
