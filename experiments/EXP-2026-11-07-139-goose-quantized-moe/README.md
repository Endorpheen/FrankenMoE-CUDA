# Whole quantized Goose MoE computation family

**Production verdict: REJECTED for promotion.** Retain this functional engineering
checkpoint and continue the larger migration. Working/default runtimes and the
compiler source are unchanged. No full CPU backend completion is claimed.

## Computation moved

Self-hosted EXP299 v8 class-reset Stage3 (SHA256
7ab1a1ca1cdb82bd45c447cafc9a8143cbcb23dab29bae3e813f66069c5d0846)
emits the actual Goose computation, not calls to the old C++ math:

- Complete Q2_0/Q5_0/IQ4_NL/IQ2_XXS/IQ2_S dot, matrix and routed-chunk computation.
- Existing IQ2_S two/three-activation weight reuse, respecting the original explicit
  target-verification window; ordinary small batches do not bypass eligibility.
- All five dequantizers, including real Qwen IQ4_NL GET_ROWS embeddings.
- Common complete Q8_0 and Q8_K activation packing. C retains only bit-copy and
  FP16 primitives, constant tables and ABI/metadata marshalling.

Native cold/bank lookup, expert-ready hooks, threadpool/scheduling, router,
large-group tiled dispatch, repack, physical ownership, CUDA and shared MTP remain.
The new base preserves the previously tested Qwen-native Goose allocation objects.
Earlier Apex Q4_K/Q6_K/tiled/elementwise modules are retained separately.
GGML_GOOSE_MOE_MATH=0 and GGML_GOOSE_MOE_QUANT=0 select native rollback;
experimental computations are enabled by default in the isolated bundle.

## Verified scope

Each native precision profile passes1024 matrix/dequant/routed/fused/concurrent
cases plus30 null-input empty spans. Inputs and output canaries are preserved.
Scoped ASan/UBSan/LSan passes982 cases/profile (inherited native backend is not
fully instrumented; no TSan/full memory-safety comparison claim).
Each selected integration passes290 actual GGML graphs /335780values and1345440
serialized bytes against unchanged original, module-off and module-on. It covers
stock/cold/bank placement, output tails,1/2/8threads, native tiled dispatch and24
legal IQ2_S fused calls. The same complete serialized hash is checked by reproduction.

Common quantizer:252 Q8_0 plus126 Q8_K serial cases and256 eight-thread calls,
1430240input values. Independent sanitizer fixture passes841 cases, including
rounding, NaN/Inf and preserved zero-block sums. Real Apex CPU128/4 matches original
content/IDs/top-5; Goose Q8_K833760calls/168672processed blocks, rollback0.
Those Apex expert weights do not exercise all five new Qwen formats.

Real Qwen32/4 and128/16, then final32/4 plus two128/16 per arm, exactly match
unchanged original content/IDs/top-5. Both Q8 packers, all five CPU formats and
base IQ4_NL execute Goose; loaded-library paths and traces are retained. Actual
model fused execution is not claimed with spec none; its separate graph proof is.

## Preliminary performance and memory

No quiet controlled acceptance or language-superiority claim. All figures below
are single-response throughput, not aggregate. Model context4096, batch/ubatch4096,
t12/tb16, no repack, CPU experts/GPU attention, Q4 KV, no MTP, fixed inputs and
sampling. Two measured128/16 requests per final arm, cache_n0, other work untouched.

| Qwen128/16 | C++ rollback | Selected Goose |
|---|---:|---:|
| Prefill tok/s range |74.33-80.04|67.91-75.03|
| Decode tok/s range |17.65-18.44|7.43-7.80|
| Decode median tok/s |18.044|7.618|
| Peak process anonymous MiB |638.828|620.082|
| Peak process file+anonymous RSS MiB |23496.660|23477.871|
| Peak VRAM MiB |6218|6198|
| Process swap KiB |0|0|

No memory advantage is claimed from these different peak/cache observations.
The first Goose model candidate decoded3.265tok/s. Application source and GCC
inline-budget changes improve it about2.33x, but the selected module is still
57.78% below adjacent native rollback: it must not replace production.
The earlier cold original run swapped; its slow prefill is not an acceleration baseline.

| Width2560/640rows operation, us | First Goose | Selected Goose | Adjacent native |
|---|---:|---:|---:|
|Q2_0|1348.671|454.362|58.959|
|Q5_0|8549.658|1939.084|73.330|
|IQ4_NL|6692.124|867.654|63.423|
|IQ2_XXS|765.894|554.150|94.128|
|IQ2_S|818.886|699.409|101.149|

Counter-free operation fixture:15configurations, one pinned CPU, five alternating
rounds and exact outputs. First/branchless/inlining/word-byte series are separate
preliminary measurements. Terminal SIMD lowering does not erase scalar preparation
and aggregate-call cost. Selected Q2 uses Goose u32 OR/shift/mask expansion; sign
preparation uses wrapping byte masks. No generated C math was manually rewritten.

Common quantizer2560values: original Q8_00.373273us, Goose7.050826us; original
Q8_K2.058990us, Goose1.599410us. Q8_0 float-to-int casts are unsupported by v8;
the bounded Goose bit-projection workaround is correct but expensive. This is
not an identically compiled C superiority control; older such controls are separate.
Apex128/4 functional probe pp83.547/89.828 and tg10.462/10.440(native/Goose),
VRAM0, anonymous424.676/424.637MiB: too short for performance acceptance.

## Preserved failures and precision contracts

Rejected float-cast and castbang sources/diagnostics remain. IQ4_NL scalar tail
uses explicit fused/unfused arithmetic matching original Qwen GCC15/Apex GCC13
objects, whose byte-identical C had different FMA contraction. The1ULP first
failure and emitted code are retained. Idle native worker first35,last33 is a
valid empty range; initial rows>=0 assertion was wrong. Goose now explicitly
returns without data access for empty routed spans. Failed source/output log remains.
A sanitizer summary filename collision was repaired from intact separate raw logs
and recorded; no rerun is implied by that repair. No compiler patch was made.

## Reproduction and provenance

Run `python3 tools/reproduce_local.py --build-root /home/end0/AI/LlamaServer/franken-goose-exp139-reproduce-20261107`.
The destination must be new and inside the allowed LlamaServer build root.
This local reproduction requires the exact recorded GGML headers/native objects
and GCC13/GCC15 installation; external dependency hashes are verified first. It
compiles eight objects from the snapshot or pinned native sources, links fresh
CPU/base libraries, runs290 actual GGML graphs and checks the recorded output hash
and all-five/base/fused witnesses. It does not load a model or compiler producer.
It is not a portable clean clone build of all FrankenMoE.

Frozen source/generated C, rejected variants, exact tests, raw textual responses,
command/environment records, compiler receipt and external dependency hashes are
included. Historical helper scripts retain their original local paths; do not
interpret them as a portable installer. SHA256SUMS covers the checkpoint. Models,
compiled runtime/compiler binaries, private feedback/instructions and unrelated
work are excluded. Further CPU MoE/tiled/backend migration and the independent
comparative safety audit remain active future work.
