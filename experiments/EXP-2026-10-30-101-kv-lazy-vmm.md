# EXP-2026-10-30-101: lazy physical KV allocation via CUDA VMM

Status: MEASURED_APEX_CANDIDATE, saved for manual use; broader compatibility pending. Author reported ACCEPTED memory; this delivery preserves the measured code and independent qualification without claiming a fully validated all-model default. Reference EXP090, projectb5cc6c8f737e007b42e7dc8a8d16a4c70b61e95d.

Hypothesis recorded by OpenCode before implementation: keep logical KV capacity1440MiB/context262144 and stable addresses/strides, map physical pages only for the used KV range. At32K the theoretical requirement180MiB gives roughly1260MiB possible savings. The existing compute-reservation and mmap-registration optimizations remain active. New source files are ggml-cuda.cu and llama-kv-cache.cpp/.h; only CUDA/libllama libraries change.

Author five balanced loaded pairs: sampled process VRAM4428 ->3188MiB in every pair (-1240MiB/-28.0036%), exact cold/repeat selected IDs/top5/probabilities. Heavy CPU load/system swap-out mean timings are descriptive, not clean speed acceptance. Author branch24576 and full261632/128-output correctness checks are retained; full capacity matched archived accepted output, peak6388MiB. Flint, failure injection and full fallback matrix not validated.

Codex independent A-B qualification, two starts/four requests after Ghostty exit: exact256 IDs per arm/ordered top5/probabilities, delta0. All four request resource gates pass: process swap/system swap-out/major faults0. Cold single-response decode29.71974 ->28.88707tok/s, repeat28.06800 ->28.11341tok/s. Observed sampled post-ready block peaks4428 ->3188MiB. This is not a five-pair speed claim. TTS was stopped only afterward; Ghostty was already idle at its initial measurement, so no causal slowdown claim.

GGML_KV_LAZY_VMM=0 disables; source documents unsupported-mode fallback. Mapping allocation failure currently aborts. Physical memory may grow and remain retained after longer requests. Do not infer universal validation or full-context savings. No model math/context/KV-precision reduction is used.

Working EXP090 restored and nine hashes verified; user explicitly requested commit/push and a manual EXP101 launch command, so no installed profile/service promotion is part of this delivery. Patch: patches/kv-lazy-vmm.patch. Metrics, manifests, commands and raw-data hashes: results/exp101-kv-lazy/.
