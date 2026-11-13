# Common Goose CPU MoE control across Apex and Qwen

**Production verdict: REJECTED for promotion; isolated research continues.**
One larger control module is actual Goose in both backends. This does not complete
the CPU MoE/backend/runtime goal or establish a speed or memory advantage.
The working/default runtime and Fried Goose compiler sources remain unchanged.

## Computation and policy moved

The same43-field Plan and six borrowed slices implement checked workspace offsets,
alignment and overflow guards; strided expert-ID grouping; cold-mask filtering and
real-ID-to-bank-slot mapping; quantization partitions; reset/barrier/work-claim
policy; explicit IQ2_S verification-window shape eligibility; tiled delegation and
chunk/scatter scheduling. Apex raw Q4_K/Q6_K fallback chunk/scatter remains Goose.
Apex retains prior Goose matrix/tiled/Q8/vector objects. Qwen retains EXP139 math,
common Q8 packing, ready hook, native tiled/repack, base/allocator/CUDA/shared MTP.
A native disable fallback is retained behind GGML_GOOSE_MOE_CONTROL=0.

The self-hosted producer is the latest completed EXP299 v8 class-reset Stage3
at experiment entry, frozen SHA256
7ab1a1ca1cdb82bd45c447cafc9a8143cbcb23dab29bae3e813f66069c5d0846.
Kernel SHA256 is dd9c1d59dfc528fd38ef62b5e94a11c67ceddc2e4d98c46422b19292d91e6c98.
No old C++ Goose compiler or compiler modification was used.

Native services still own tensor metadata/physical buffers, atomics/barriers,
threadpool/OS, ready hooks and remaining math/tiled/repack kernels. Qwen control
covers ten weight types, but only the earlier five EXP139 math families are Goose
in that bundle; Q4_K/Q6_K/F32/F16/BF16 math there remains native. Full kernel,
backend, CUDA, scheduler and application replacement remain future work.

## Verification

- 43 C/Goose field offsets and Plan/Row sizes asserted.
- 4096 workspace layouts;90 service-fixture cases;29317compared service events and
  4080grouped routes across1/2/8threads. Strided IDs, cold/bank, zero experts,
  variable base alignment and invalid overflow/workspace/thread/alignment plans.
  These services are mocked; they are not model or timing evidence.
- Apex58graphs/336464values and expanded114graphs/2171536values match unchanged
  native and previous controller byte-for-byte; retained Q4/Q6 Goose calls witnessed.
- Qwen290graphs and expanded412graphs/389732values/1562224serialized bytes match
  unchanged native, previous and new control exactly. Ten types include dense and
  quantized weights, prequantized activations, padded IDs, zero masks, multiple
  real IDs mapping one bank slot and explicit positive/negative fused windows.
  Native fused_dispatches60 counts worker eligibility;40executed fused chunks is
  a different scope. All native statistics and output bytes match.
- Scoped ASan/UBSan/LSan pass Apex114 and Qwen412 graphs on/off. New controller,
  modified CPU core/bridge and fixtures are instrumented; Qwen native cold fallback
  also instrumented. Inherited math/base/other CPU objects are not fully instrumented.
  No TSan, independent full audit or memory-safety superiority claim.
- Separate controller-counter-free bundles repeat all114/412 graphs exactly.

Real Apex128/4 and2048/16 preserve original text/IDs/top-5 probabilities.
Process trace includes warmup plus completed request:8640controller entries,
126504grouped routes,538608chunks,79440tiled delegations,1599744Q4/806784Q6calls.
Cumulative layout_bytes4851524544 is a schema-size sum, not resident RAM.

Ten actual Qwen requests preserve original text/IDs/top-5 exactly. Loader warmup
is disabled:33504entries/28830grouped routes/372096chunks therefore come from
real32/4+128/16 requests. Short stock specnone/EHS0 model requests execute no fused
or tiled chunks; those paths have graph coverage, not claimed physical model use.

## Preliminary performance

All numbers are single-response throughput, not aggregate. Other work was not
stopped; no controlled quiet acceptance. Both controls retain identical prior
Goose compute objects. Models use context4096, batch/ubatch4096, no repack/MTP.
Apex dedicated CPU profile has t8/tb16; Qwen CPU experts/GPU attention has t12/tb16.

| Model/profile | Previous controller | Common Goose controller |
|---|---:|---:|
|Apex CPU2048/16 prefill tok/s|102.254|102.819|
|Apex CPU2048/16 decode tok/s|10.103|10.230|
|Apex anonymous MiB|786.184|786.156|
|Apex VRAM/processswap|0/0|0/0|
|Qwen warm128/16 prefill tok/s|76.610|81.671|
|Qwen warm128/16 decode tok/s|8.186|7.835|
|Qwen anonymous MiB|624.742|621.902|
|Qwen VRAM MiB|6198|6198|
|Qwen processswap|0|0|

Qwen is one matched warmed pair:32/4 then128/16warm then128/16measured, cache_n0.
New decode is about4.29% lower; different external load and one sample do not
establish causality. It remains open and does not halt larger isolated experiments.
An initial pair was cache-confounded (pp15.250/69.388,tg4.511/7.568); both are
retained and not called an acceleration. File RSS/caches differ; no memory advantage.
New controller diagnostics removed in timing bundles; prior EXP139 Qwen math/quant
diagnostics are identical in both arms. Existing EXP139 math overhead remains open.

Separate58Apex and412Qwen whole-operation timing profiles compare unchanged native,
previous and common controller. Persistent poll0 pools, setup excluded, initial
compute plus two warmups,5samples*20computes and byte stability after each sample.
These include retained math, not pure control cycles or model throughput. Separate
noisy series include potential regressions; all raw profiles are retained.
For Apex width2048/output65/tokens128/8threads Q4:1373.4/1549.9/1705.8us and
Q6:1483.7/4845.15/4744.65us(native/previous/new). No performance promotion.

## Preserved failures and contracts

The first custom C ABI header lacked primitive prototypes; uint8 return conflicts
stopped host compilation. Failed header/log remain. Adding explicit prototypes
fixed integration without compiler edits. Real-ID cardinality is distinct from
bank-slot count:5IDs and3slots is valid; suppressed negative LUT entries are not read.
A first premature Apex request returned503 before load completed; no inference ran.
The same live process reached ready and was checked without an observation-driven
restart. All own test servers stopped. Nine original runtime hashes reverified.

## Local reproduction

Run python3 tools/reproduce_local.py --build-root /home/end0/AI/LlamaServer/franken-goose-exp140-reproduce-20261107
with a fresh destination inside LlamaServer. The tool verifies recorded external
header/native-object dependencies, rebuilds five objects across two native backends,
links isolated CPU libraries and repeats114Apex plus412Qwen actual GGML graphs.
Both output hashes and new controller/retained-compute witnesses must match.
No model, compiler producer or production runtime is started. This is local
reproduction with pinned external dependencies, not a portable clean-clone build.

Frozen source/emitted C, ABI failures, fixtures, raw text outputs, full commands,
compiler receipt, external hashes and SHA256SUMS are included. Historical helpers
retain original local paths. Models/runtime/compiler binaries, compiler source,
private feedback/instructions and unrelated user work are excluded. Continue the
remaining CPU MoE tiled/backend work after the owner handoff, then the full goal.
