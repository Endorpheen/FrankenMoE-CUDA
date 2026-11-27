# GigaChat4 Goose runtime candidate

Build the isolated server with `bash integration/goose-standalone/gigachat4/build-server.sh`.
Set `GOOSE_BIN` to the installed Fried Goose compiler when needed. The model
remains on its external disk and the server maps it in place.

The candidate builds the decoder and MTP graphs in Goose. The standard
standalone Goose executor runs inference. The C adapter passes mapped tensor
descriptors and server buffers to Goose. Draft cache catch-up is diagnostic and
speculative verification is pending.

The short comparison used a fixed 29-token input, eight generated tokens,
context 36864, batch/ubatch 256/128 and decode/batch threads 8/16. Both
configurations used 1922 MiB of dense weights and 4093 MiB of expert cache.
The experimental prefill planner is selected with
`GOOSE_GIGACHAT4_PLANNED=prefill`. Dense and expert placement are enabled in
the measured configuration with `GOOSE_GIGACHAT4_DENSE=1` and
`GOOSE_GIGACHAT4_EXPERTS=1`.

| Run | Prefill, tok/s | Decode, tok/s | VRAM after request, MiB |
| --- | ---: | ---: | ---: |
| Control 1 | 3.292 | 1.461 | 8256 |
| Control 2 | 3.339 | 1.471 | 8273 |
| Prefill planner 1 | 3.822 | 1.494 | 8277 |
| Prefill planner 2 | 3.814 | 1.493 | 8277 |

All four runs generated the same eight IDs: 1292, 7154, 15166, 347, 4620,
926, 20201, 7919. Process swap was zero and system swap-out was unchanged.
These are short-request diagnostics. The 32K/512 gate and model-specific
OpenCode tool round trips remain pending.

## Resident recurrent state

The CUDA backend retains convolution history and the recurrent state of all
24 GDN layers in 50 MiB. Disable this path with
`GOOSE_GIGACHAT4_GDN_STATE=0`. The graph marks in-place recurrent updates for
the existing Goose CUDA executor.

The fixed 29/8 comparison used the same placement and prefill planner above.
Runs followed resident, control, control, resident order without profiling.

| Run | Prefill, tok/s | Decode, tok/s | VRAM after request, MiB |
| --- | ---: | ---: | ---: |
| Control 1 | 3.794 | 1.470 | 8261 |
| Control 2 | 3.798 | 1.485 | 8261 |
| Resident 1 | 3.832 | 1.645 | 8311 |
| Resident 2 | 3.847 | 1.645 | 8311 |

All four runs matched the eight reference IDs. Process swap and system
swap-out delta were zero. A separate diagnostic run recorded 24 resident
GDN operations per graph. Median decode increased by 11.3 percent.

## YaRN reference parity

GigaChat4 stores the YaRN logarithmic multiplier in conversion units.
The loader normalizes it by 0.1 before deriving the RoPE attention factor,
matching the external llama.cpp reference at 0541e50e2fd463b376a3332fd891243aa5ce9c6a.

The fixed 29/8 request SHA-256 is
`c3b98b3778170ecac6bf43632539968392dc3db635c299956ccfb87dc0aec3f4`.
Before the fix, Goose and the reference matched for two output tokens.
After the fix, they matched for all eight: 1292, 7154, 4008, 88, 57981,
6958, 7780, 391. This gate covers one fixed request.

## CUDA MLA key projection

The loader expands the nine Q5_0 key-projection tensors to F32 using the
existing Goose decoder and registers each head plane in the CUDA weight
table. This moves the supported matrix operation off the CPU. The path is
enabled with at least 6 GiB free VRAM and can be disabled with
`GOOSE_GIGACHAT4_MLA_F32=0`. Expanded weights occupy 72 MiB each in RAM and
VRAM, without changes to the GGUF file.

The fixed 29/8 comparison used the same executable and ran control,
candidate, candidate, control. It used the prior accepted GDN residency
and an enabled prefill planner, dense placement, and expert cache.

| Run | Prefill, tok/s | Decode, tok/s | VRAM after request, MiB |
| --- | ---: | ---: | ---: |
| Control 1 | 3.747 | 1.686 | 8303 |
| Control 2 | 3.681 | 1.660 | 8312 |
| CUDA MLA 1 | 5.976 | 2.709 | 8379 |
| CUDA MLA 2 | 5.969 | 2.749 | 8386 |

All four runs matched the eight reference tokens from the YaRN gate. Process
swap and system swap-out delta were zero in all four runs. Median peak RSS
increased by 80 MiB. Median prefill increased by 60.8 percent and median
decode increased by 63.1 percent.

## CUDA F16 MLA attention

A separate Goose kernel computes F16 attention for 576-wide keys and
512-wide values. Its dispatcher preserves the Q4 path and selects the F16
kernel for the MLA layout. Disable the F16 path with
`GOOSE_GIGACHAT4_MLA_ATTENTION=0`.

| Run | Prefill, tok/s | Decode, tok/s | VRAM after request, MiB |
| --- | ---: | ---: | ---: |
| Control 1 | 5.875 | 2.619 | 8379 |
| Control 2 | 6.054 | 2.733 | 8377 |
| CUDA F16 1 | 8.840 | 4.057 | 8379 |
| CUDA F16 2 | 8.908 | 4.037 | 8379 |

All eight output IDs matched the external reference in each fixed 29/8
run. The two medians increased by 48.8 percent for prefill and 51.2 percent
for decode. Median peak RSS increased by 58 MiB. Process swap was zero
in every run. The first pair recorded 73 and 91 pages of system swap-out,
and the reverse pair recorded none.

A diagnostic run exercised eight CUDA attention nodes per model step.
The existing Q4 CUDA test matched byte for byte with one and 29 queries
and 257 keys. The existing Q4 PTX instructions also matched after
label normalization in both generated images.

## Skip redundant short-context DSA ranking

When the attention cache has at most 2048 cells, DSA ranking would select
every cell. The decoder uses the original additive mask and still writes the
index keys. Larger graph shapes continue to rank and select 2048 positions.

The fixed 128/8 request SHA-256 is
`ec82485b4a39190f1b9c195c589bf62565cb52a7a6ad624748a264cb3eecffad`.
The configuration and memory placement were held fixed, with two runs per
variant and identical generated IDs matching the external reference.

| Run | Prefill, tok/s | Decode, tok/s | VRAM after request, MiB |
| --- | ---: | ---: | ---: |
| Control 1 | 5.939 | 2.891 | 8408 |
| Control 2 | 5.914 | 2.848 | 8397 |
| Skip ranking 1 | 16.594 | 4.761 | 8405 |
| Skip ranking 2 | 16.622 | 4.634 | 8397 |

The median speed ratios were 2.80 for prefill and 1.64 for decode. Peak
RSS was approximately 18.58 GiB in all four runs. Process swap was zero.
System swap-out rose by 81 pages in the first candidate run and was
unchanged in the other three.

A graph check at 2048 and 2049 live rows found eight index-cache writes
in both cases and zero versus eight ranking nodes, respectively. It
validated the graph transition without executing a long request.

## Prompt MMQ and planned decode

GigaChat4 enables the existing CUDA MMQ path and execution planning by
default. Set `GOOSE_MMQ=0` to disable MMQ or
`GOOSE_GIGACHAT4_PLANNED=prefill` to plan only the prompt. Any explicit
environment value takes precedence over the model's defaults.

The fixed 128/8 request was run in control, candidate, candidate, control
order in one build, without profiling. The control used the prefill
planner and disabled MMQ. The candidate kept the planner for both phases
and enabled MMQ. Dense and expert placement were identical.

| Run | Prefill, tok/s | Decode, tok/s | VRAM after request, MiB |
| --- | ---: | ---: | ---: |
| Control 1 | 16.503 | 4.744 | 8407 |
| Control 2 | 15.986 | 4.587 | 8409 |
| MMQ and planned decode 1 | 19.670 | 8.047 | 8409 |
| MMQ and planned decode 2 | 20.256 | 8.304 | 8406 |

All four runs matched the eight reference tokens. Median prefill increased
by 22.9 percent and median decode by 75.2 percent. Process swap was zero.
System swap-out was zero in the first pair and rose by 34,798 and 37,528
pages in the second candidate and control runs, respectively.

## CUDA DSA score reduction

For longer caches, a fused Goose CUDA node evaluates the weighted ReLU
reduction directly into the score matrix. This avoids materializing a
64-head intermediate. The existing top-k and additive mask remain in the
graph. The CPU graph remains available when CUDA is unavailable or
`GOOSE_GIGACHAT4_DSA_CUDA=0` is set.

On a synthetic shape of 2176 keys and 128 queries, one old score graph
took 28.669405 seconds and the CUDA node took 0.114242 seconds. All
278528 scores matched exactly, including 39790 masked positions. This
comparison measures the score operation and excludes model execution.

The real 2176/8 request crossed the threshold with a full 128-token
prefill block. All eight output IDs matched the external reference:
2759, 481, 87544, 15344, 6716, 4437, 1942, 20201. The request SHA-256
is `cdf9a3e8a8de1a9fdb3b12eece9c0eead3f7817a30fffd935854aba3d6150daf`.
Goose recorded 21.47 prefill and 6.63 decode tokens/s in this diagnostic,
with 8429 MiB VRAM after the request and a peak RSS of 21.99 GiB. Process
swap was zero. System swap-out increased by 575570 pages during the
request. The numbers are not a paired end-to-end speed comparison.

## Exact wide DSA top-k

GigaChat4 selects a heap-based exact top-k on DSA score rows wider than
4096 cells. The heap reuses the sort workspace and emits the same stable
ordering, with earlier indices first for equal scores. Other graph
operations retain their existing sort dispatch.

On 32768 keys and eight queries, the previous scoring-plus-selection
probe exceeded its 30-second limit. The bounded-heap probe completed in
0.131 seconds, and the GigaChat4-tagged build repeated it in 0.142 seconds.
All 16384 selected indices matched an independent stable sort.
The output SHA-256 is
`90b2c19ef326692ac85016e4994ca8aca775d6a1664b7c5f5913e4626465f031`.
This is a synthetic DSA operation test. A 32K model run is pending.

## Packed exact top-k heap keys

The wide DSA heap stores an ordered score key with its stable index
tie-breaker in the existing 64-bit index workspace. Each input score is
read once. Heap comparisons no longer reload scores. Signed zero maps
to a common key and equal scores retain their original index order.

On a warmed synthetic 32768-key, one-query graph, scoring and top-k
together took 2.468 and 2.062 ms in the control and 1.338 and 1.310 ms
with packed keys, in control/candidate/candidate/control order. All
2048 output indices matched byte for byte. The result SHA-256 is
`1fed0858dfba9f0e35aed5b42329068d66cc91decd1b0d4d8692602b274aa482`.
The measurement covers one DSA layer and excludes model execution.

## Warm the GGUF before CUDA placement

The GigaChat4 server path now calls the existing Goose residency routine
before registering expert pages for CUDA. The shared implementation
checks the file mapping and warms missing pages. The original weights
remain mapped from the same file.

A startup observed only 19862 of 22175 MiB resident and no experts
registered. Its short HTTP request timed out. With the warmup and the
accepted SwiGLU path, startup reported all 22175 MiB resident and
4050 MiB of pinned expert pages. The diagnostic 128/8 request finished
at 20.89 prefill and 8.54 decode tokens/s and matched all eight external
reference IDs. Process swap and system swap-out delta were zero.
This is a startup correctness check rather than a paired speed comparison.

## CUDA clamped SwiGLU with precise down projections

GigaChat4 routes clamped SwiGLU through one Goose CUDA vector operation.
Its down projections use the existing direct path without MMQ activation
packing. Other projections retain the existing selection policy. The CPU
implementation remains available for execution without CUDA.

The four 128/8 runs used the same placement and request, in
control/candidate/candidate/control order without collecting logprobs.

| Run | Prefill, tok/s | Decode, tok/s | VRAM after request, MiB |
| --- | ---: | ---: | ---: |
| Control 1 | 21.259 | 8.337 | 8425 |
| Control 2 | 21.182 | 8.162 | 8393 |
| CUDA SwiGLU 1 | 29.906 | 9.823 | 8396 |
| CUDA SwiGLU 2 | 29.743 | 9.970 | 8389 |

Median improvements were 40.5 percent for prefill and 20.0 percent for
decode. Process swap and system swap-out delta were zero. Median peak
RSS rose by approximately 49 MiB. All eight tokens matched the external
reference in every measured run and on an independent 29/8 input.
The initial all-MMQ CUDA SwiGLU candidate differed at the eighth token
and was excluded from this checkpoint.

## Restore MMQ after CUDA clamped SwiGLU

The CUDA SwiGLU path uses a range-reduced exponential and explicitly
rounded division before the MMQ activation pack. The regular vector
formula remains active on shapes that do not enter MMQ. The experimental
down-projection exclusion from MMQ is removed.

The fully precise formula matched all 786432 CPU test values bit for bit
on the synthetic tensor. Two runs of the shape-selective candidate on
the fixed 128/32 request matched all 32 external reference IDs. The
previous checkpoint first differed at token 18. An independent 29/8
request matched all eight reference tokens.

| 128/32 run | Prefill, tok/s | Decode, tok/s | VRAM after request, MiB |
| --- | ---: | ---: | ---: |
| Previous checkpoint | 29.903 | 11.165 | 8405 |
| Restored MMQ down 1 | 32.846 | 10.664 | 8406 |
| Restored MMQ down 2 | 33.153 | 10.625 | 8408 |

Process swap and system swap-out delta were zero. Peak RSS increased by
about 86 MiB. The decode streams diverge after token 17 in the control,
so these decode rates do not isolate kernel latency on equal token streams.

## Reduce large routed expert products in one node

The GigaChat4 graph uses the existing Goose weighted expert reduction
when materializing the expert products would exceed 32 MiB. Smaller
graphs retain the previous route reduction. MMQ and the clamped SwiGLU
path are unchanged.

On the fixed 512/32 input with identical Q4/Q6 reference MMQ flags, two
control runs measured 46.322 and 46.337 prefill tokens/s and 10.007 and
9.765 decode tokens/s. Two fully fused candidate runs measured 47.767
and 48.501 prefill tokens/s and 10.006 and 9.778 decode tokens/s.
All four produced the same 32 IDs as the external reference. Process
swap and system swap-out delta were zero.

The final large-only variant also matched all 32 IDs at 47.874 prefill
and 10.214 decode tokens/s, with 8476 MiB VRAM after the request and zero
swap delta. In the Nsight diagnostic, prefill device-to-host copies
fell from 2096736256 to 796502016 bytes. All 93 large product-window
copies disappeared.

### EXP-2026-11-26-2143 — CUDA GDN Q/K head repetition

The GigaChat4 graph tags the two GDN head-repeat nodes. Batched nodes matching
the tagged F32 layout use the existing CUDA vector broadcast. Decode and
untagged graphs keep their original dispatch.

On the fixed 512/32 request, the control measured 47.88/9.84 tokens/s and
the tagged candidate measured 142.61/10.20 tokens/s. Two earlier shape-only
probes measured 140.19/9.95 and 143.29/10.24 tokens/s. All 32 generated token
IDs matched the llama.cpp reference on this fixture. Post-request GPU memory
was 8481 MiB versus 8490 MiB for the control. Process swap was zero and
system swap-out did not increase.

### EXP-2026-11-26-2144 — CUDA logit softcap

The GigaChat4 graph tags the logit softcap tanh for CUDA dispatch. The
surrounding scales and sampling settings are unchanged. The CUDA expression
uses an odd polynomial near zero and an exponential form elsewhere.

On the fixed 512/32 request, alternating control and candidate trials measured
control prefill 142.24 and 134.70 tokens/s, candidate prefill 143.26 and 138.37
tokens/s, control decode 10.27 and 10.24 tokens/s, and candidate decode 11.00
and 11.03 tokens/s. All four responses matched the reference token IDs.
With a fixed seed and temperature 0.6, all 32 sampled IDs and the returned
top-five IDs matched the control. Maximum absolute log-probability error
was 3.82e-6 across 192 values. Post-request GPU memory was 8460–8469 MiB across
both arms, and process swap and system swap-out growth were zero.

### EXP-2026-11-26-2145 — CUDA GDN head repetition for single-token decode

Extend the tagged EXP-2143 dispatch to the single-token layout. The existing
batched condition is unchanged. The diagnostic predicate accepted additional
short batches, but the final predicate retains their original dispatch.

On 512/32 the paired control/candidate trials measured prefill
145.14/136.90 and 143.59/143.27 tokens/s, with decode 11.03/13.13 and
11.11/12.93 tokens/s. The final single-token predicate measured 142.96/13.06
tokens/s. Every run matched all 32 reference token IDs, with no process swap
or system swap-out growth. Post-request GPU memory was 8459–8469 MiB in the
paired runs and 8483 MiB in the final check.

### EXP-2026-11-26-2146 — Stage small GigaChat4 expert waves

The planned GigaChat4 graph enables asynchronous staging for small expert
waves while it computes. The thread-local switch is cleared before graph
commit or error handling. Other callers retain the existing threshold.

Two paired 512/32 comparisons measured prefill 141.17/141.79 and
144.48/143.95 tokens/s, with decode 12.94/14.62 and 13.07/14.02 tokens/s.
All four runs matched the 32 llama.cpp token IDs. Post-request GPU memory
was 8469–8470 MiB and neither process swap nor system swap-out grew.

### EXP-2026-11-26-2147 — Increase MLA parallelism in single-token decode

Use 64 keys per tile for a single MLA query. Batched MLA and ordinary
attention retain 256 keys per tile. The partial-softmax workspace keeps its
existing byte limit.

On 512/32, the control/candidate pairs measured prefill 143.09/143.69 and
141.31/143.88 tokens/s and decode 14.72/16.13 and 14.34/15.93 tokens/s.
All 32 output IDs matched the llama.cpp fixture in every run. Post-request
GPU memory was 8460–8470 MiB and swap-out was unchanged. On the separate
2049/2 DSA boundary fixture, both builds matched both llama.cpp token IDs
and used 8483 MiB after the request without swap-out growth.

### EXP-2026-11-26-2148 — Dense MMA for narrow GigaChat4 projections

Use the existing residual-corrected Dense MMA kernel for width-16 F32
projections in the planned GigaChat4 graph when the CUDA image provides it.
The existing batch floor excludes single-token decode. An explicit
GOOSE_DENSE_MMA setting keeps its existing behavior.

The fixed 512/32 control/candidate pairs measured prefill 142.20/154.38 and
142.06/155.56 tokens/s, with decode 16.00/16.04 and 15.19/15.67 tokens/s.
All outputs matched all 32 llama.cpp token IDs and swap-out was unchanged.
Post-request GPU memory across the four runs was 8459–8469 MiB. On a
second fixture the width-16 probe reproduced all 32 control tokens.
Both Goose builds first differed from llama.cpp at token 31 on that fixture.

### EXP-2026-11-27-2149 — Share MLA K/V reads across two query heads

Use the paired-head principle from Apex EXP-1712 for single-query
GigaChat4 MLA with a shared key head and value head. Both heads keep
separate softmax accumulators and the existing partial-state merge.
Set GOOSE_GIGA_MLA_PAIR=0 to use the original tile.

On 512/32 the same-binary off/on pairs measured prefill 152.65/152.29
and 153.51/151.57 tokens/s, with decode 15.52/16.93 and 15.21/16.26.
All four runs matched all 32 stored reference IDs. The default-enabled
build measured 151.94/17.25 with matching IDs, 8473 MiB post-request GPU
memory and zero swap-out growth. The 2049/2 DSA check matched both
reference IDs at 8485 MiB with no swap-out growth.

### EXP-2026-11-27-2150 — Pair batched MLA heads within the scratch budget

Batched GigaChat4 MLA pairs heads at the same query position. The
paired tile rounds the prior accumulator product before the FMA,
matching the ordinary tile's FP32 association. Compact scratch waves
keep adjacent head slots together within the 32 MiB cap. The
single-query EXP-2149 kernel remains separate. Set
GOOSE_GIGA_MLA_BATCH_PAIR=0 to use ordinary batched tiles.

The 2176/8 DSA off/on pairs measured prefill 130.60/142.56 and
128.10/143.50 tokens/s, with decode 10.79/11.77 and 12.03/10.85.
All four runs matched the eight reference IDs, used 8469–8471 MiB
GPU memory after the request and recorded no process or system swap-out
growth. The default-enabled build measured 142.15/11.99 with matching
IDs at 8469 MiB. The scratch mapping check includes 32K geometry;
a 32K/512 inference benchmark has not qualified this change at that size.

### EXP-2026-11-27-2151 — Assemble the GigaChat4 DSA mask on CUDA

Keep the existing DSA scoring and top-k. Build the final F16 causal mask
directly from the selected indices with a clear and scatter operation.
The Goose dispatch includes a host implementation for unsupported CUDA
shapes. Set GOOSE_GIGA_DSA_MASK=0 to use the original graph.

On 2176/8 with batch and ubatch 4096, the off/on pairs measured prefill
121.55/203.40 and 121.16/202.14 tokens/s, with decode 11.91/12.27 and
9.99/11.89 tokens/s. All four responses matched all eight reference IDs.
Post-request GPU memory was 8928–8929 MiB off and 8913 MiB on, with
zero process swap or system swap-out growth. The default-enabled
build measured 204.57/11.67 at 8912 MiB with all eight IDs matching.
The alternate host implementation and 32K/512 throughput have not
been independently qualified in this experiment.

### EXP-2026-11-27-2152 — Produce the GigaChat4 causal mask in F16

Populate the causal mask with the exact F16 representations of zero
and negative infinity. Pass it directly to MLA and CUDA DSA. Widen it
only when the fallback graph specifically needs F32.

On the fixed 4352/2 diagnostic input, ordinary control and candidate
runs measured 147.04/9.80 and 213.38/10.06 tokens/s. Prefill latency
fell from 29.60 to 20.40 seconds and both generated token IDs matched.
Post-request GPU memory was 9213/9212 MiB. Peak RSS was
24,251,124/24,218,836 KiB with no process swap or system swap-out growth.
With the old DSA mask graph forced on 2176/8, all eight IDs matched the
llama.cpp fixture. The 4352/2 input is a deterministic diagnostic
input; this comparison does not establish a 32K/512 result.

### EXP-2026-11-27-2153 — Keep the unranked causal prefix in DSA

For a single-stream batch straddling the 2048-key limit, use the
padded cell count as an upper bound on the number of visible keys.
Score and rank only the suffix that might exclude a visible key.
The mask producer preserves the original F16 prefix. Set
GOOSE_GIGA_DSA_PREFIX=0 to rank the full batch.

On 2176/8 with batch and ubatch 4096, off/on prefill measured
261.85/280.24 and 261.82/278.36 tokens/s. Decode measured
11.70/12.65 and 10.69/10.74 tokens/s. All four responses matched all
eight reference IDs. Post-request GPU memory was 8912–8913 MiB off
and 8904–8905 MiB on, with no swap-out growth. Default-enabled
prefill and decode were 279.02/12.73, with matching IDs, 8912 MiB
GPU memory and no swap-out growth.

### EXP-2026-11-27-2154 — Use Tensor Cores for tiled GigaChat4 MLA QK

For eligible GigaChat4 prefill shapes on SM80-class CUDA images, use
FP16 inputs to the existing m16n8k16 operation for QK. Keep tile scores
in 4 KiB of block shared memory. DSA masking, the FP32 weighted value
recurrence and the existing partial-state merge remain in the tile
path. Set GOOSE_GIGA_MLA_MMA=0 to use the previous paired MLA tile.

On 2176/8 with batch and ubatch 4096, off/on prefill measured
275.54/300.16 and 276.56/299.80 tokens/s. Decode measured
11.06/11.82 and 12.54/11.94 tokens/s. All four responses matched all
eight reference IDs. Post-request GPU memory was 8904–8905 MiB and
no run grew swap. The default-enabled build measured 299.98/11.52,
matched all eight reference IDs and used 8898 MiB after the request
with no swap-out growth. A second 512/32 input matched all 32 IDs
against the same binary without MMA. The 4352/2 diagnostic also
matched both control IDs. No 32K/512 qualification is claimed.

### EXP-2026-11-27-2155 — Use Tensor Cores for GigaChat4 MLA P×V

The eligible prefill MLA tile computes the masked softmax maximum and
mass in F32, packs the probabilities to F16 and applies the same
m16n8k16 operation to P×V. Existing partial-state merge and decode
dispatch stay in place. Set GOOSE_GIGA_MLA_PV_MMA=0 to retain the
EXP-2154 QK-only tile.

On 2176/8 with batch and ubatch 4096, off/on prefill measured
298.39/341.94 and 299.00/332.92 tokens/s. All eight IDs matched the
llama.cpp fixture. The seven-step decode measurements were lower
in the two candidate arms. A separate 2176/32 pair measured
300.76/343.63 prefill and 12.73/12.87 decode, with all 32 IDs
matching between arms. The default-enabled build measured
344.09/13.20 and matched those same 32 IDs at 8911 MiB VRAM after
the request and 23,364,444 KiB peak RSS, without swap-out growth.
A second 512/32 input matched all 32 QK-only control IDs. No
32K/512 or separate cross-architecture qualification is claimed.

### EXP-2026-11-27-2156 — Reuse F32 queries across four DSA keys

For eligible GigaChat4 prefill score batches a warp reuses one
loaded F32 query across four keys. Each key keeps the original
dot-product association, warp reduction, and ordered weighted head
accumulation. Small query batches retain the previous kernel.
Set GOOSE_GIGA_DSA_SHARED_Q=0 to use that kernel on all batches.

On 4352/2 the clean off/on pairs measured prefill 274.48/286.19 and
273.57/280.62 tokens/s. The single-step decode observations were
10.51/10.16 and 10.84/9.57. On a distinct 6144/8 input the final
off/default pair measured 265.42/285.88 prefill and 12.12/12.16 decode
at 9207/9198 MiB after the request with no swap-out growth.
Both fixtures reproduced all output token IDs. Full DSA key-ID
captures matched byte for byte on both fixtures, including ordering.
No 32K/512 or separate cross-architecture qualification is claimed.

### EXP-2026-11-27-2157 — Rank wide DSA prefill rows on CUDA

For GigaChat4 DSA batches with at least 128 query rows and width
greater than 4096, a radix cutoff selects the exact 2048 keys using
the same score and index ordering as the CPU heap. The selected
keys are sorted before GPU mask construction. Smaller batches,
including single-token decode, retain the previous route. Set
GOOSE_GIGA_DSA_TOP_WIDE=0 to disable the new path.

Three paired 6144/32 runs measured median prefill 286.49 to 313.72
tokens/s and median decode 12.10 to 12.07 tokens/s. All runs
returned the same 32 token IDs. Peak RSS differed by at most
716 KiB within a pair; post-request VRAM ranged from 9197 to
9221 MiB. Process swap and system swap-out growth stayed at zero.
The default build measured 312.03/11.83 at 9233 MiB and matched
all 32 IDs. A separate 4352/2 diagnostic matched the captured DSA
key IDs byte for byte. No 32K/512 or cross-architecture inference
qualification is claimed.

### EXP-2026-11-27-2163 — Pipeline pageable expert staging

For eligible GigaChat4 prefill staging waves, the CPU submission
thread copies and enqueues the first plane while the existing worker
fills the remaining planes in a disjoint part of the same ring slot.
The worker is joined before its planes or the slot event are enqueued.
The cache, projection order, upload order and 3 × 4 MiB ring are
unchanged. Set GOOSE_GIGA_STAGE_PIPELINE=0 to disable this path.

With pageable staging and its worker enabled, 512/32 A-B-B-A means
improved from 165.15 to 185.99 prefill tokens/s (+12.6%); decode was
15.34 to 15.32. All 32 output IDs matched. VRAM was 8466–8467 MiB,
peak RSS stayed within 296 KiB and no swap-out growth occurred.
A default-enabled diagnostic recorded 3470 parallel batches in
prefill and none in decode. A separate 6144/32 off/default pair
measured 310.80/12.20 and 317.49/12.05 tokens/s, with matching
output IDs, 9215/9207 MiB VRAM and no swap-out growth. That pair
is diagnostic. No 32K/512 or separate cross-architecture inference
qualification is claimed.

### EXP-2026-11-27-2169 — Keep fully resident expert pin ranges

GigaChat4 registers the fully resident ranges within the requested
pin budget and lets pageable staging handle ranges containing
nonresident pages. Pin checks and bootstrap registration order are
unchanged. The selection reserves 12 MiB for the pageable ring.
Set GOOSE_GIGA_PARTIAL_PIN=0 to use all-or-nothing selection.
The host pin limit can be set with GOOSE_EXPERT_PIN_MIB.

On fully resident 512/32 A-B-B-A requests at a fixed 4093 MiB GPU
expert cache, raising the host pin request from 4096 to 18601 MiB
registered 4050/18545 MiB. Prefill rose from 186.16 to 268.96 and
decode from 15.58 to 18.15 tokens/s. Peak RSS increased by 1.25 GiB,
post-request VRAM by 27–30 MiB, and no swap-out growth occurred.
All 32 IDs matched. These gains measure the larger pin limit.

The range selection test preserved both eligible ranges when the
middle range was unavailable. A live 512/32 diagnostic injected one
missing-page report and registered 0/18343 MiB with the old/new
policy. Both responses matched all 32 IDs and neither grew swap.
A separate default-enabled run at the high pin limit returned
269.18/18.49 tokens/s at 8505 MiB post-request board memory.
No 32K/512 or separate cross-architecture inference qualification
is claimed.

### EXP-2026-11-27-2177 — Grow the GigaChat4 wide activation arena

For eligible GigaChat4 wide activation growth, use 768 MiB when
at least 2 GiB remains after the allocation. Retain the existing
384 MiB growth on low memory or allocation refusal. An explicit
GOOSE_CUDA_ACTIVATION_MIB still takes precedence.
Set GOOSE_GIGA_WIDE_ACTIVATION=0 to retain the original growth.

On 6144/32 with 18545 MiB pinned host weights and 4093 MiB expert
cache, the explicit 768 MiB A-B-B-A means were 341.43 to 406.60
prefill and 14.20 to 14.20 decode tokens/s. All 32 output IDs
matched and no swap-out growth occurred. Post-request board VRAM
was 9234–9235 versus 9466–9467 MiB, and peak RSS was lower by about
473 MiB. Paired diagnostic traces recorded 12499 to 2320 MiB D2H
and 54512 to 42060 MiB H2D in the estimated prefill window.
The default-enabled build selected 768 MiB and measured 400.40/14.05
at 9505 MiB post-request board memory, with matching IDs and no swap.
No 32K/512 or separate cross-architecture inference qualification
is claimed.

### EXP-2026-11-27-2178 — Retain more wide GigaChat4 activations

Select 1024 MiB at eligible GigaChat4 arena growth when at least
2 GiB remains after allocation. Retain the 768 and 384 MiB
fallbacks and the explicit GOOSE_CUDA_ACTIVATION_MIB override.

On 6144/32, explicit 768/1024 MiB A-B-B-A means were 405.07 to
425.30 prefill and 14.08 to 14.22 decode tokens/s. All 32 IDs
matched, no swap-out growth, post-request board memory 9467–9473
versus 9507 MiB and peak RSS about 511 MiB lower. Diagnostics
recorded 2320 to 272 MiB D2H in the estimated prefill window,
eliminating eight 256 MiB downloads. The automatic path selected
1024 MiB, matched the 32 IDs and measured 420.53/14.35 at 9545 MiB
post-request board memory. A 512/32 correctness check retained the
small arena, matched 32 IDs and did not grow swap. No 32K/512 or
separate cross-architecture inference qualification is claimed.

### EXP-2026-11-27-2179 — Use Dense MMA for wide MLA projections

Reuse the residual-corrected F32 Dense MMA kernel for the GigaChat4
K128/M512 and K512/M128 MLA projection planes on 2048 and 4096
job batches. Set GOOSE_GIGA_MLA_PROJECTION_MMA=0 to disable the
additional shapes. A broader candidate diverged on 512/32, so
the final gate admits only the measured large-batch geometry.

On 6144/32 with 18545 MiB pinned, A-B-B-A means were 420.88 to
448.95 prefill and 14.15 to 14.15 decode tokens/s. All four output
sequences matched, post-request VRAM 9553–9554 MiB, peak RSS
23,605,160–23,610,092 KiB, and no swap-out growth. The scoped
default measured 448.08/14.35 at 9523 MiB and matched all 32 IDs.
The scoped 512/32 check reproduced all 32 prior IDs. No 32K/512
or separate cross-architecture inference qualification is claimed.

### EXP-2026-11-27-2181 — Share visibility across GigaChat4 MLA heads

For eligible tensor-core MLA prefill, scan the real F16 mask once
per eight-query/256-key tile. All heads share the flag. Fully
masked tiles publish the original empty partials; mixed tiles
use the unchanged QK/PV body. Force inlining of the shared body
so adding a kernel entry does not slow the original entry.
Set GOOSE_GIGA_MLA_VISIBILITY=0 to disable the map.

On 6144/32 the A-B-B-A means were 451.17 to 478.20 prefill
and 14.16 to 14.01 decode tokens/s. The sum of both phases fell
from 15.808 to 15.061 seconds. All 32 tokens matched, board
VRAM after the request was 9523–9538 MiB and swap did not grow.
The default build measured 480.18/14.22 on 6144/32 at 9538 MiB
and 270.51/18.32 on 512/32 at 8493 MiB. Both matched all 32 IDs.
No 32K/512 or separate cross-architecture qualification is claimed.
