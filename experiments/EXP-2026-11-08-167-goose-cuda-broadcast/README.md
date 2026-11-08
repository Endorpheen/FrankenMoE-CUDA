# EXP167: complete active CUDA binary/broadcast/repeat module in Goose

Solo continuation from EXP166. The entire active `binbcast.cu` family now selects self-hosted Goose:add/sub/mul/div/repeat/repeat-back and fused add/mul2..8. Goose owns host dimensional collapse,type/stride validation,fast-div metadata,grid/unravel choice and actual device indexing,arithmetic/fold/reduction order. Original arithmetic is not called by the default implementation. This is not full CUDA/Apex closure;other registered families and providers remain.

## Execution,ABI and ownership

`host.goose` prepares a fixed-width trivially copyable packet using typed tensor/plan projections and already accepted Goose geometry. `device.goose` computes broadcast coordinates,ordered fused operations,grid-stride rows and repeat-back accumulation. Imports are raw typed F32/F16/BF16 load/store,umulhi ISA,PDL,stream access and kernel launches. Fast-div coefficients and quotient/modulo logic are Goose,not calls to the original native helper. Generated scalar C functions and checked division helpers are mechanically qualified/decorated for CUDA;emitted arithmetic is not manually rewritten. Host C is a separate unit with one explicit matching C/C++ FFI header.

The host checks nonzero dimensions,valid operands/types,fuse2..8,shared fused-input layouts,repeat compatibility,stride alignment and grid/address arithmetic before stream/kernel effects. Caller-owned allocated tensor/device-buffer contracts remain required;this is not validation of arbitrary forged pointers or universal thread/device lifetime safety. No allocation/work queues are introduced into the numerical path. Original native functions remain hidden for an independent oracle and `GGML_GOOSE_CUDA_BROADCAST=0` rollback;immutable EXP166 is preserved. Only the binbcast object and new host unit change. The unchanged EXP166 CUDA DSO relinks SHA-exactly first. EXP166 activation code,RMS preload and all earlier CPU/core/runtime settings remain. Production/compiler are untouched,no auxiliary agents.

## Correctness fixes in the inherited active source

These findings describe this pinned native control,not every upstream version.

- F32/F16/F32 and F16/F16/F32 dispatch reinterpret the second input as `float*`;normal F16 stride fails its byte-alignment assertion. A separate original-process fixture reproduces the abort. Goose uses the actual F16 load and byte strides;an independent typed CUDA reference verifies the repaired combinations.
- Native collapse tests input contiguity without destination contiguity. Padded destination rows can be flattened incorrectly. Goose requires contiguous destination before collapse;216cases reproduce native output/padding errors and verify the corrected logical output. Repeat already uses destination as its src0 and avoids that native defect;its padded case remains ordinary parity.
- Native fast-div adds high-product+n in uint32 and shifts that32-bit sum;overflow and shift-by32 lose quotient bits. Goose widens the addition/shift while retaining the hardware umulhi primitive.1792actual GPU cases cover14divisors through UINT32_MAX:Goose matches CPU division,host Goose magic coefficients match native,and245native overflow/shift discrepancies reproduce. This is a correctness test without timing,not a microbenchmark.

## Actual GPU checks

1692complete GPUcases/34282156values/111498checks compare outputs,guards and padding with an independent typed logical CUDA reference.1080valid native cases are also bit-exact;396repaired-type and216padded-destination cases intentionally use logical correctness rather than copying incorrect native behavior.1074native-compatible rollback cases pass. All eight public entries,all three grid/unravel/repeat-back paths and eight dtype mixes are witnessed locally.

The corpus covers partial/full dimensional collapse,multi-axis broadcasting,permuted/sparse/padded source strides,padded output,fused counts2..8,allowed src0 in-place,signed zeros/subnormals/NaNs/infinities,F32/F16/BF16 and large shapes that really exceed3D grid limits and select unravel. Twelve invalid metadata cases reject before stream/kernel effects. Hidden test-only host magic and actual generated device helpers are used for boundary division tests;no corresponding mutable production export exists. No full sanitizer suite or performance microbenchmark was run.

## One fixed real Apex32K/128 gate

| Metric | Latest EXP166 | EXP167 |
|---|---:|---:|
|Prefill tok/s|2631.509|2635.581|
|Decode tok/s|27.171|26.567|
|Sampled RSS MiB|21319.961|21328.164|
|Sampled VRAM MiB|3246|3246|
|Process swap KiB|0|0|

Prefill+0.155%,decode-2.223%in one uncontrolled sample. The decrease is retained;stable parity/acceleration is unproven. Exactly one model request,no repeated series or tuning. Archived same-profile original C++2867.846/27.978remains historical,not a fresh paired control. No concurrent build/delegated workers launched during the run;external host load uncontrolled.

Text,all128token IDs and every saved completion probability/top5 array exactly match original C++;127graphs reused. New host mask `0x4a`,grid operation mask `0xa`,dtype mask `0x1` and CUDA status0confirm actual add/mul/fused-add in inference. Unravel,repeat-back and mixed types are proven locally,not claimed exercised by this fixed profile. EXP166 activation witness remains active and all registry/scheduler/buffer/staging/allocator/graph/context ownership counters return to zero. Exact new CUDA mapping/hash is checked;prior CPU/base/llama/registry hashes remain. Raw request,response,server/memory/process receipts stay local under `runtime/exp167/results/apex-32k128/`.

## Reproduce and remaining scope

`python3 build.py` emits host/device C,checks the original link and links the isolated DSO. `python3 check.py` checks the complete GPU native/typed-reference/rollback corpus and original mixed-type rejection. `python3 check_extra.py` checks full-width fast-div and host preflight. `python3 run_apex.py` runs the fixed profile once and refuses existing receipts;inspect authoritative state instead of restarting blindly. Producer/source/library hashes,commands,findings and summaries are retained.

Next:actual Apex normalization,row routing/reductions,then matrix/attention/recurrent and model/KV/loader/server residuals. Broader snake/softcap/acc/add-id/inline families and strict native-source removal remain explicit in P3-04. No full universal requirement,campaign/delivery cycle or100% Apex completion is falsely declared. Production/compiler unchanged,no Qwen or delegated workers.
