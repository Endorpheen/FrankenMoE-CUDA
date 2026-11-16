# EXP-2026-10-31-128: CUDA device code / module residency

Status: ALREADY_PRESENT (lazy module loading), small bound.

`cuModuleGetLoadingMode` on driver 580.178.04 returns mode 2 =
CU_MODULE_LAZY_LOADING. The 212 MB libggml-cuda file is file size, not VRAM;
unused kernel families are not loaded eagerly. No per-module VRAM number was
measured, so no code change and no saving claim. Artifacts:
results/opencode-vram/EXP128/output.json.
