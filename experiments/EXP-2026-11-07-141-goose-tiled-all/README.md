# Whole Goose AVX2 tiled subsystem

Functional experiment ACCEPTED; performance audit deferred. Production unchanged.
The same actual Goose source replaces the complete common tiled computation for
13 K/IQ families in both native CPU backends. This is not a complete CPU backend
or complete runtime migration. The current owner priority is full functional
migration first, then a systematic performance audit. No compiler sources changed.

## Replaced computation

Q4_K, Q5_K, Q6_K, Q3_K, Q2_K, IQ4_XS, IQ2_XXS, IQ2_XS, IQ2_S, IQ3_XXS,
IQ3_S, IQ1_S and IQ1_M: weight unpack/repack, activation gather/pad and bias,
seven16x16 microkernel specializations, K accumulation, narrow/standard routed
windows, dense/broadcast scheduling and result scatter. Q8_K conversion invokes
the existing actual Goose quantizer. Tensor metadata, native workspace sizing,
physical buffers, OS threads/barriers/atomic work claims and other operations are
still external services. This source qualifies AVX2; other ISA falls back to native.
GGML_GOOSE_TILED_GENERAL defaults to1; set0 to preserve the previous tiled path.
Original baseline and all prior compute objects are retained for rollback.

Producer: latest completed current C11 flow Stage3 at experiment start,
SHA2560424b7bce8ddfbf308d2208612cef01de1780ada26337ff0f556a6786f9ea364.
Its three-stage fixed-C bootstrap result does not certify full C11 lifecycle support.
Canonical Goose SHA256317ec22ace25d50217fabe0f6db9a8a4b91cfd73ece879e1ddf58cbcd142853a.
Frozen emitted C, ABI assertions, native bridge, fixtures and rejected inputs included.

## Functional evidence

-1950serial+104eight-thread unpack cases,39641280semantic bytes exact.
-403gather cases/24960blocks,390microcases/2275slabs+104eight-thread cases exact.
-169serial+13eight-thread complete MoE-driver cases,1609436output values exact;
  26disabled/smallbatch fallbacks and1024workspace layouts.
-Each native backend:104actual GGML graphs (52dense+52MoE),4946253values,
  19785844serialized bytes equal native and disabled controls. All13formats,
  2D/4D/broadcast, F32/Q8_K, valid padded rows/weights, zero inputs, K/window tails.
  Forced dispatch and counter-free Apex controls also exact. Every new stage/type
  has actual graph execution witnesses.
-Scoped ASan/UBSan/LSan new Goose normal and forced corpus passes both backends.
  Native disabled fallback fails original IQ3_XXS unaligned uint32_t load at
  tiled.cpp:402:32 (offset66). All four failure logs retained without suppression.
  Earlier math/quant/base/native microkernel objects incompletely instrumented;
  no full-runtime safety, TSan or comparative superiority claim.
-Actual Apex128/4 and2048/16 preserve original text/IDs/top5. Loader warmup off:
  dense9824/MoE313008new calls, Q4/Q6microtiles66517504/14190592,
  original tiled fallback0. New computation executes in actual inference.
-Actual Qwen native versus integrated Goose512/16 preserves text/IDs/top5 and
  identical request. Cold expert profile selects previous raw path: new tiled
  dense0/MoE0. Thus actual-model IQ2 tiled execution is NOT proven; graph coverage
  and Apex execution are separate evidence. No extra model reloads to force a path.
-Nine production/frozen runtime hashes unchanged. Own servers stopped after tests.

## Recorded performance; no optimization campaign

All single-response, preliminary, externally loaded; no speed/memory acceptance.

| Apex CPU2048/16 | Previous tiled | Whole Goose tiled |
|---|---:|---:|
|Prefill tok/s|102.718288|81.188562|
|Decode tok/s|10.158121|10.606306|
|Anonymous MiB|786.195|786.098|
|VRAM MiB / swap KiB|0/0|0/0|

Prefill -20.95997% recorded, migration continues.104whole-operation timing
profiles are retained as already completed evidence, not repeated acceptance.
Example Q6dense4352/out257/rows273/t1 native4547.8us/previous4438.8/new21495us.
Qwen incidental native/new pp21.234/23.563,tg4.519/4.252; original processswap1440KiB,
new0 and system swapout62485/4081pages confound comparison; no benefit claimed.
A pre-existing inline-flags variant built but was never validated or model-tested;
its records are explicitly BUILT_VALIDATION_PENDING and it is not the selected runtime.

## Reproduction and remaining scope

python3 tools/reproduce_local.py --build-root /home/end0/AI/LlamaServer/franken-goose-exp141-reproduce-20261107

This local tool verifies pinned external headers/objects, rebuilds four objects per
backend, links isolated CPU libraries and checks104graphs each including all13new
stage witnesses. It uses frozen emitted C and local external native dependencies;
not a portable clean-clone build or compiler bootstrap. Source/emitted provenance,
full original build commands, raw logs, rejected source and SHA256SUMS retained.
Model/compiler/runtime binaries, private instructions/feedback and unrelated work excluded.

Remaining native CPU math/repack/thread services, the full GGML CPU/CUDA backends,
memory/scheduler/KV, GGUF/loading/tokenizer/server/API/infrastructure still need
functional migration. Advance whole modules; defer local tuning until full port.
