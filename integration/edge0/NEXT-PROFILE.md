# EXP921: profile the accepted cached Edge0 path

**Historical reference.** This note records an Edge0 experiment from the Flint lineage. See [the model registry](../../docs/goose-migration/MODEL-ARCHITECTURES.md) for current qualification.

Reference commit4f4cdeb235178475bba61a8444aeb0cdd3b0a1bd, accepted binary set
/home/end0/AI/LlamaServer/edge0-cached-checkpoint-20261117. Do not overwrite it.
Current fixed75/256 gate: prefill4.178624,decode4.5936155,processVRAM5296MiB,
RSS10783148KiB,zero process swap and zero system swap-out delta. Same256 IDs
and text as uncached control. Target15-17tok/s or better, if feasible.

First action: one profiled cached request, unchanged model arithmetic/cache
quota/profile/template/prompt. Fifteen-minute maximum is unnecessary; five
minutes is the initial limit. Capture the first16 decode evaluations with
Nsight CUDA node tracing, then continue the normal capped generation. Keep
normal EOS and the explicit max256 output budget. This is diagnostic timing,
not a replacement for the unprofiled performance gate.

Separate D/U/expert/Ple reads and application cache hit/miss, physical process
read_bytes, CPU FFN, host weight copies, CUDA device kernels/memcpy, stream
synchronisation and graph construction. Follow the actual owned CLI, not the
Nsight launcher. Record missing proc samples as missing, not a pass or zero.

Only after those results, select a targeted mechanism and record its Amdahl
bound, RAM/VRAM budget, correctness gate and minimal falsifying probe. Preserve
all accepted changes; no expert dropping, quant change, EOS suppression or
blanket tolerance loosening. All tests sequential; no Apex tests.

## Measured choice, before implementation
Cached profile58.075204s decode: CPU24.574678s, weight copies11.220692s,
expert reads7.914849s, graph0.510233s, PLE0.355369s, D/U reads0.
Immutable GPU dense weights are repeatedly copied:94069785600 bytes across255
steps (368901120 bytes/step). Retain exact raw bytes once per model/layer.
Additional RAM bound384MiB; expected unprofiled RSS <11GiB, VRAM unchanged,
owned process swap0 and system swap-out delta0 required. No arithmetic change.
Amdahl ceiling from the profiled copy fraction19.32%: about1.24x, not a promise
of15tok/s. Additional allocation/sync effects must be measured.
Use normal Gooses/quant/route/sum order and readonly model-weight contract.
Invalidate retained bytes on an explicit layer rebind. Validate GPU-weight copy
miss/hit against an independent CPU graph before a256-token A/B.
