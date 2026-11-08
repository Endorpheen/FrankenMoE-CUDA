# EXP171: actual Apex text RoPE/IMRoPE computation

Status:scoped functional acceptance;not whole Apex,full universal RoPE,source closure or stable C++ performance parity. EXP170 immutable rollback preserved;compiler and production unchanged. Exactly two authorized GLM workers continue disjoint GDN work;no Codex subagents.

## Ownership

Current full self-hosted Goose implements normal/NeoX/MRoPE/IMRoPE forward/backward numerical rotation,position/section selection,partial-window tails,strided addressing,F16 conversion policy,YaRN mixing/bounds and checked host launch planning. Scalar CUDA/libm transcendental/floor/ceil operations,storage,conversion,PDL and launches are explicit effects. No original RoPE arithmetic callback on these enabled paths. Original vision,set-rows fusion and RMS+mul+RoPE fusion remain native,explicitly outside this scoped port. Their presence is not claimed Goose/source closure. The actual Apex IMRoPE path is witnessed below.

Useful GLM code retained with parent corrections:device loads/stores must use raw tensor pointers xp/dp,not the packet plan address;YaRN upper correction bound is n_dims-1,not n_dims/2;the native M_PI path is f32 (the draft's double-arithmetic comment was wrong);explicit u64 flags are required;the compiler cannot lower the draft f32->i64 wrapping conversion,so exact scalar floorf/ceilf effects replace it. Host now checks finite derived bounds,checked span/address wrap,output injectivity and safe same-layout inplace versus shifted aliases. Compiler issue is recorded only in ignored local COMPILER_FEEDBACK.md;compiler not modified.

## Tests

68real GPU cases across four modes,17F16,14backward,12frequency-factor cases,YaRN/offset/strided/full/partial/inplace variants;14464independent identity/sincos reference pairs and322578candidate checks pass. Native finite output and complete guarded output buffers compare bitwise;the independent sincos reference alone retains its fixed predeclared16ULP-at-max(abs(value),1) bound. Native comparison is never relaxed.68original rollback cases,one capture/replay per mode and30malformed before-stream rejection cases pass. F32->F16 cross-type conversion is implemented but not claimed covered by this same-type GPU corpus. No microbenchmark,performance series or sanitizer claim.

The worker fixture initially had wrong host input/output index formulas and wrote frequency data into an empty vector when disabled;Codex fixed these before the first run. Added real inplace cases,YaRN,capture,device-witness assertions and independent preflight cases. Immutable read-only inputs/guards and output tails/padding are checked;inplace output mutation is intentional and compared to the original. Original draft artifacts remain labeled worker-* for provenance,not accepted executable sources.

## Actual Apex32K/128

One request:2638.584prefill/28.026decode,exact text/all128IDs/full saved probabilities,127graph reuses. FG171R host1/device modes8/variants0x50/cuda_status0 proves actual F32 forward IMRoPE with inplace partial64of256channels in Apex. RSS21330.973MiB,VRAM3246MiB,swap0;all owned exit heaps0. Relative to EXP170:-0.433percent prefill/+0.147percent decode,small preliminary variation. Saved C++2918.900/29.589 is an archive,not a fresh paired performance comparison. No stable no-regression/acceleration claim. Raw model data/conversations remain local.

## Reproduction/rollback

Run `python3 build.py`,then `python3 check.py` from here. Builder reproduces the exact EXP170 CUDA SHA before replacing only rope.cu.o. Qualification pins the actual measured library and source hashes;private oracle/internal FFI exports are hidden by a verified version map. `run_apex.py` records one request and refuses to overwrite its existing process receipt. GGML_GOOSE_CUDA_ROPE=0 restores the original implementation;whole-stage rollback uses immutable EXP170. Existing accepted objects,production/model profiles and compiler remain unchanged.

P3-06 advances to IN_PROGRESS;softmax/masks/position helpers,fusions/native removal and other Apex GDN/attention/CPU/provider/model/KV/loading/server requirements remain. No universal requirement,campaign stage or full Apex batch newly complete. Next integrate GDN scalar/MMA as one coherent compute path,using returned drafts only after strict parent checks.
