# EXP-2026-11-07-137: Complete Goose compute blocks

Apex now executes a complete CUDA RMSNorm compute block and complete CPU Q8_K
activation quantization from current self-hosted Goose. Exact correctness, runtime
execution and rollback are verified; performance remains preliminary.

Artifacts and reproducible CPU check: [EXP-2026-11-07-137-goose-compute-blocks](EXP-2026-11-07-137-goose-compute-blocks/README.md).
No production promotion. Both negative variants and same-flags C controls are kept.
