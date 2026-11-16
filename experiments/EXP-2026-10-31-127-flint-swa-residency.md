# EXP-2026-10-31-127: SWA-specific KV residency on Flint

Status: NOT_RUN.

EXP111 established that the generic lazy-KV path is numerically compatible with
Flint on cold, 24576-branch and 4096-early requests (delta 0, VRAM 3264 -> 2536
MiB). Whether SWA layers retain pages beyond the useful window was not analyzed,
and no SWA-specific release was implemented. Global-attention layers were not
touched.
