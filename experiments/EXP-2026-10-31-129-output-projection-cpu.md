# EXP-2026-10-31-129: minimal-VRAM profile with large GPU weights on CPU

Status: NOT_RUN.

Estimated bound only: a 248320x2048 output projection is ~286 MiB at Q4_K or
~417 MiB at Q6_K, i.e. a modest VRAM saving; the per-token logits GEMM
(about 500M MACs) would move to the CPU and is expected to cost decode heavily.
No placement probe was run and the default was not changed. This remains a
potential tradeoff profile, not an accepted one.
