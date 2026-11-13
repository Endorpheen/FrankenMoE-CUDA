# EXP166: real CUDA activation/GLU/scale/clamp compute in Goose

Solo continuation from EXP165.40public paths now use current self-hosted Goose for actual device arithmetic/indexing and host metadata,validation,pointer selection and launch planning:26unaries,five gated variants,OAI/clamped SWIGLU,xIELU,SiLU backward,leaky ReLU,scale/clamp and fused unary-mul/ReLU-square. All native-supported F32/F16/BF16 combinations remain. This replaces three CUDA objects in the actual DSO;it is not an unused auxiliary module or a Goose call back to original arithmetic.

## Compiler,ABI and execution

`device.goose` contains nonlinear formulas,gate/strided/row indexing,casts through typed storage primitives and grid-stride scale execution. `host.goose` owns shape/type checks,strides,packed/separate/swapped sources,fusion selection,grid/block selection and params. Raw effects are CUDA scalar math/FMA,typed load/store,PDL,existing accepted Goose geometry,param projections,stream access and kernel launch thunks. No original activation algorithm is imported.

Both files compile with the frozen completed full self-hosted producer. Host C compiles separately against one explicit C/C++ FFI header. Device lowering mechanically selects emitted scalar functions,qualifies identifiers and decorates them with CUDA device/forceinline. Emitted checked divide/modulo helpers are copied unchanged;their fatal effect maps to a device trap. No emitted arithmetic is manually rewritten. A common trivially copyable fixed-width packet carries fields;the active CUDA context header SHA exactly matches the compile tree. Numerical primitives and runtime/driver are explicit external dependencies,not claimed rewritten.

Original independent CUDA functions remain under hidden prefixed symbols for rollback/oracles. Default-on `GGML_GOOSE_CUDA_ACTIVATIONS=0` selects these originals;immutable EXP165 also remains. Only superseded SiLU/scale preloads are removed from this experimental model profile because their whole-TU exports would mask new functions. Accepted RMS preload and all prior CPU/core/allocator/scheduler/registry settings remain. Production binary,profile and compiler source are untouched. Default activation code does not call the retained native arithmetic. Other translation-unit inline activation copies and numerical backends remain separate unfinished scope.

The original CUDA DSO relinks SHA-exactly before replacing unary/scale/clamp objects and adding the generated host unit. New public exports are only read-only host/device witnesses;native fallback symbols are local via the link map. Actual device counters are sampled by first-use-registered atexit before CUDA teardown. The initial ELF-destructor attempt returned CUDA status4;it was corrected before model testing.

## Correctness findings and GPU checks

An independent native source oracle and candidate execute on the real RTX4070 with the same compile flags.896GPUcases/1059072values/58252checks compare full output bytes and guards/padding across F32/F16/BF16,odd tails,contiguous/strided/padded rows,separate/packed/swapped gates,fused SiLU/sigmoid/softplus,allowed in-place paths,NaN payloads,signed zero,infinities and subnormals. All40host entries and all10device modes are witnessed. Native rollback has no new execution witness. Eight invalid metadata/bounds/NULL cases reject before stream creation/kernel effects. Internal CUDA APIs retain ordinary caller-owned allocated-tensor/buffer contracts;this is not universal validation of arbitrary forged device pointers. No full sanitizer suite or microbenchmark was run.

Initial xIELU differed by1ULP (`0x450aceb8`native versus`0x450aceb9`Goose). PTX showed NVCC contracted opposite products in `alpha_p*x*x+beta*x`. Explicit FMA calls in Goose reproduce native positive,negative and final-selection rounding. Final complete GPU corpus is bit-exact. The compiler was not changed. Findings and initial arithmetic instruction excerpts are retained in `precision-findings.json` and `initial-xielu-*-arithmetic.txt`.

## Fixed real Apex and one justified concern pair

Initial single EXP166request:2642.061prefill/26.286decode,with exact128token IDs,text,all saved probabilities and127graph reuses. Decode was4.06percent below the previous saved EXP165sample,so one sequential EXP165->EXP166pair was run without code changes. This is a concrete performance concern,not an unrequested tuning series.

| Metric | Fresh EXP165control | EXP166candidate |
|---|---:|---:|
|Prefill tok/s|2638.737|2631.509|
|Decode tok/s|26.943|27.171|
|Sampled RSS MiB|21315.059|21319.961|
|Sampled VRAM MiB|3246|3246|
|Process swap KiB|0|0|

Pair -0.274%prefill/+0.846%decode:the initial decrease did not reproduce. All samples are retained;this uncontrolled single pair does not prove stable acceleration or fresh-C++ parity. Archived original same-profile C++2867.846/27.978remains historical. Three total model requests:two candidate and one previous-stage control. No concurrent build/delegated workers launched during measurements;external host processes uncontrolled.

All three model responses exactly match original C++ text/all128IDs/full saved top5-probability arrays;all reuse127graphs. Actual new host mask `0x5012000400` and device masks show sigmoid/softplus,SWIGLU/fused unary-mul and scale execution with CUDA status0. Other family modes are proven locally,not falsely claimed used by Apex. Inherited registry/scheduler/buffer/staging/allocator/graph/context counters return to zero. The exact immutable new CUDA DSO is mapped;previous libllama/base/CPU/registry hashes remain. Raw requests,responses,logs and memory receipts remain local under `runtime/exp166/results/`.

## Reproduce and next scope

`python3 build.py` emits host/device code,checks ABI/context/source and unchanged link,then links the isolated DSO. `python3 check.py` rebuilds local GPU/preflight fixtures and checks native parity/rollback. `python3 run_apex.py` performs one fixed profile and refuses an existing receipt. `run_concern_control.py` and `run_concern_candidate.py` retain the specifically justified pair,with separate immutable result directories;do not repeat merely for reassurance. Build/source/library/producer hashes and command receipts are saved.

Next actual Apex modules:CUDA binary/broadcast,normalization/routing/reductions,then matrix/attention/recurrent paths and model/KV ownership. P3-01/P3-02remainIN_PROGRESS. Full CPU/CUDA/model/loading/server/native-source closure is not claimed. No production changes,auxiliary agents,Qwen work or speed-tuning series.
