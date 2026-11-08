# EXP170: actual Apex CUDA SSM convolution module

Status:scoped functional acceptance;not full recurrent/Apex/source closure or stable C++ performance parity. EXP169 remains immutable rollback. Exactly two authorized GLM workers;no production or compiler changes.

## Implementation

The entire active SSM convolution TU is replaced with current full self-hosted Goose host/device computation:serial rolling window for decode,tiled shared-memory prefill,all six native d_conv sizes3/4/5/7/9/15,fused SiLU and optional bias,both ADD operand orders,checked tensor/stride/window planning and native launch shape. Fixed POD/local arrays are emitted and mechanically qualified to CUDA;no VM allocation in compute. CUDA scalar exp/storage/barrier/PDL/launch effects remain explicit. No original SSM numerical callback executes on the enabled path. Original hidden oracle remains available only for tests and default-off rollback.

GLM's useful numerical draft was retained. Codex added actual public-ABI projection,checked spans/output-input disjointness,bounded-tail-only policy,typed FFI,CUDA bridge,producer extraction/link integrity,preflight tests and capture checks. The native ragged long-token tile reads unused elements beyond the declared input window. Candidate Goose always bounds those loads;no unsafe native-trace production mode. Native paired tests allocate safe slack;candidate-only ragged cases test the declared exact window against an independent reference. Guard canaries and source-level load bounds are scoped evidence,not a claim that canaries catch read-only OOB or that sanitizers were run.

## Verification

160candidate GPU cases,156native-paired cases,4candidate-only bounded-tail cases,2934272independent double-reference points and14494007checks pass. All finite native comparisons are bitwise;only the independent fp64 oracle uses the fixed predeclared abs1e-5+rel1e-6 tolerance.156original rollback cases and2CUDA graph capture/replay executions per mode pass. Input bytes/output guards/padding unchanged.26malformed metadata/null/type/stride/window/alias/overflow/fusion cases reject before stream/kernel effects. No microbenchmarks,extra model series or broad sanitizer runs.

One Apex32K/128 request:2650.059prefill/27.985decode,exact text/all128IDs/full saved probabilities,127graph reuses. Actual new host0x2/device modes0x7/sizes0x2/cuda_status0 prove fused SiLU,d_conv4 serial and tiled Goose kernels execute in decode/prefill. Other local variants are not claimed exercised by this profile. RSS21330.957MiB,VRAM3246MiB,swap0;all owned exit heaps0. +0.156percent prefill/-0.190percent decode vs EXP169 is small uncontrolled single-sample variation,not stable speed parity/fresh C++ comparison. Saved C++2918.900/29.589 remains archive only. No full Apex or campaign-stage closure.

## Reproduction and rollback

From this directory run `python3 build.py`,then `python3 check.py`. The builder first reproduces the exact EXP169 CUDA hash before replacing only ssm-conv.cu.o,retaining all previous accepted objects. Qualification pins the exact measured artifact. `run_apex.py` records one fixed request and refuses to overwrite an existing process receipt;use an explicit new result directory for a justified subsequent gate. `GGML_GOOSE_CUDA_SSM=0` restores the private original;whole-stage rollback uses immutable EXP169. Internal FFI/native oracle visibility is actually verified with nm. Raw model responses/requests/conversations remain local.

P3-13 is IN_PROGRESS:SSM convolution only is complete here;gated-delta/MMA,scan/other registered recurrence and broad source removal remain. Next actual Apex work:RoPE and remaining attention/recurrent compute,then provider/model/KV/load/server and native-source closure. No compiler source edits,legacy compiler fallback,Qwen run or production change.
