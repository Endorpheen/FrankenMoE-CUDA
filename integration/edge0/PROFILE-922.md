# EXP922: find the next Edge0 FFN bottleneck

**Historical reference.** This note records an Edge0 experiment from the Flint lineage. See [the model registry](../../docs/goose-migration/MODEL-ARCHITECTURES.md) for current qualification.

Control EXP921 edb893ab89afd8194e75af6e9861e0910e7d957d, immutable six-ELF
checkpoint /home/end0/AI/LlamaServer/edge0-dense-checkpoint-20261117.
Fixed75/256 request, ctx512,b/ub1,t8,ngl48,MTPoff,greedy,officiallocalJinja
thinkingfalse,normalEOS,max256. Current qualified median6.518697 decode,
6.247443prefill,processVRAM5296MiB,RSS11132668KiB,zero process/global swap.

First: one diagnostic profile, at most5minutes, full host categories and
first16 decode CUDA node activities. Exact256 IDs must remain equal to prior
control. No optimisation is assumed before this measurement. Distinguish the
profiling overhead/global swap from a formal unprofiled performance gate.

Then read/measure actual GGML CPU parallelism and decide whether graph/math,
weight I/O or placement is the next limiting resource. For a CUDA arithmetic
probe keep the existing predeclared numerical gate and exact256 token result;
do not relax them after mismatch. Before implementation record the mechanism,
Amdahl bound, RAM/VRAM budget and stop condition. Old controls are immutable,
all models/tests sequential, no Apex work and no system setting changes.

## Before the minimal CUDA probe
OMP interposition observed outer8/active1 only for the first layer's two calls,
then outer1/active0 for the other layers. With GGML's max-task clamp this does
not establish a general nested serialisation bottleneck; do not tune global OMP.
Probe the existing Goose latent expert on CPU-reference and CUDA backend with
identical raw real weights and deterministic F32 input. Compare each stage for
diagnostics; acceptance boundary is the existing final FFN2e-5abs+2e-5rel
criterion, fixed before running, plus finite/nonzero output. First failure
refutes a blanket GPU path. Successful probe alone does not accept the model:
a complete candidate must still pass exact256 token IDs/text and resource gates.
No new kernel/backend, quant format, dropped expert or relaxed post-hoc tolerance.

## Measured decision and candidate boundary
EXP921 profile: decode43.843473s, CPU compute24.016244911s, expert read5.892474669s;
dense D2H and D/U reads zero in decode. CPU fraction54.78%; eliminating it alone
has an optimistic2.21x bound, before GPU transfer/dispatch costs. Ordinary GGML
CUDA fails the fixed gate (final max_abs0.00033486262,1974/2560 outside).
CPU-packed compatible Goose dot probe passes90 real matrices/amplitudes. This is
not model acceptance. Candidate retains CPU Q8_K/Q8_0 activation packing and all
CPU nonlinearities/router/shared/sum order, replacing only50 latent matvecs.
CUDA weights:1415577600 DU bytes +512*1164800 direct-mapped expert record bytes
+bounded input/output scratch, approximately1.88GiB extra VRAM. Host memory caches
unchanged. Cache tags include layer and expert, published after successful upload;
collisions reload, never reuse another record. One owned stream synchronises each
result before CPU consumers. This is native resource/ABI debt; arithmetic integer
pieces reuse Goose. Control uses FRANKEN_EDGE0_GPU_COMPAT=0. Candidate is isolated.
Correctness gate: existing primitive tolerance; exact all256 model IDs and text;
normal EOS on short Russian arithmetic. Stop on numerical/resource mismatch or
non-positive full request gain. No model-result claim from primitive timings.
