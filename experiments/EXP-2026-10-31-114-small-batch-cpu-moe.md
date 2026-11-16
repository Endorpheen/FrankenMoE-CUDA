# EXP-2026-10-31-114: CPU multi-row MoE for the 2-8 token batch

Status: NOT_RUN in this session.

The repeat request uses cache 32764 / prompt 4 and a repeat TTFT of about
0.21-0.26 s (verified Apex runs), so the improvement target is the ~4-token
tail, not the 28-29 tok/s steady decode. A multi-row Q4_K/Q6_K CPU path reusing
unpacked weights/scales while preserving each row's accumulation order was not
implemented or measured. Upper bound is the tail's share of repeat TTFT; small.
