# EXP-2026-11-08-154: restore verified EXP150 Apex baseline

The owner stopped EXP153 investigation and requested return to the last correct measured checkpoint,then solo continuation of the existing full Apex Goose migration. All EXP153 sources,failed results and subsequent diagnostic fixes remain quarantined in their original directories/branch;none are imported here. Production and Fried Goose compiler sources are unchanged.

## Exact restoration

Base commit: `87c79741a0c1ec9261fa22f074e0ac3b19b89a65` (EXP150). Branch: `codex/apex-exp150-restored-20261108`. The preserved EXP150 runtime had a later typed-fill ABI correction in libllama. Rebuilding its one changed object from the committed historical `pre-typed-fill-abi/llama-moe-cache.cpp` and relinking with the original recipe recovered the measured libllama SHA-256 exactly:

`926fde348bbce3741a9b899165534793d96c3bb134f21daeb9cb62c54b3f22af`

Other saved runtime libraries and the three inherited Goose preload modules match the original measurement receipt. The historical optional expert-cache ABI is not exercised by the fixed cache-size-zero Apex profile;this restoration is not approval for optional cache modes.

## One real Apex32K/128 request

Same model,input,sampling,context262144,b4096/ub4096,t8/tb16,CPU MoE,GPU dense/attention,Q4 KV,FA,EHSoff and original Goose selections.

| Metric | Historical EXP150 | Restored |
|---|---:|---:|
| Prefill tok/s |2656.285|2631.170|
| Decode tok/s |28.079|27.492|
| Sampled RSS peak MiB |21312.090|21315.605|
| Sampled VRAM peak MiB |3348|3246|
| Graphs reused |127|127|

Text,all128token IDs and the complete saved probability/top5 arrays match the unchanged C++ reference exactly. Process swap is zero. This is one uncontrolled diagnostic sample,not proof of stable speed parity or the >3000/>30 target. Sampling can miss brief VRAM peaks.

## Continuation policy

Continue the same full Apex migration from this restored baseline,without DeepSeek,Crouton,Command Code or Codex subagents. The dedicated old worker factory is stopped and its jobs are retained idle. After every significant implementation change,run the needed scoped correctness checks and one identical full Apex32K/128 performance/correctness gate against the verified prior stage. Small timing noise is allowed;material regressions or correctness mismatches block promotion to the next stage. Do not introduce multiple unqualified subsystems before the real-model gate.

Runtime: `/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108/runtime/run`. Local raw request,response,memory and logs: `runtime/results/restore-exp150-32k128/` (not published). Source and link commands are preserved here;the fixed bootstrap compiler remains unchanged. No migration requirement is newly closed by restoring the baseline.
