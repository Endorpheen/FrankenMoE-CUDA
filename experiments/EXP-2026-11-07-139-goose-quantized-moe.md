# EXP-2026-11-07-139: Whole quantized Goose MoE math

Real Qwen executes five quantized computation families and common Q8 packing in
self-hosted Goose, with exact correctness and native rollback. Source optimization
recovers substantial decode speed, but performance remains below native: production
promotion REJECTED. Research continues; working runtime unchanged.

[Evidence and reproduction](EXP-2026-11-07-139-goose-quantized-moe/README.md).
