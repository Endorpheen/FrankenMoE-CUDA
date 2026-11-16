# EXP-2026-10-30-105: specialized Q4_K/Q6_K CPU vec-dot for MoE decode

Status: NOT_IMPLEMENTED (no measured headroom established; CPU attribution is
blocked on this host). No code was built.

## Investigation

- The decode expert path uses `ggml_vec_dot_q4_K_q8_K` and
  `ggml_vec_dot_q6_K_q8_K` (`ggml/src/ggml-cpu/arch/x86/quants.c`). The AVX2
  implementation already processes four 256-value blocks per iteration, uses
  fused multiply-add for the scale/min correction and keeps the 4-bit unpack in
  registers. There is no scalar fallback in this build.
- `ggml_compute_forward_mul_mat_id_one_chunk()` reuses one quantized activation
  column for 16 output rows per block, so the q8 operand is already shared
  across rows and should stay in L1.
- CPU attribution would require sampling the server threads. `perf_event_paranoid`
  is 4 on this host, so unprivileged `perf record` of the server process is
  denied. The EXP084 trace only shows the GPU side: decode window 4.4189 s, GPU
  kernel union 0.9716 s, so at most 78% is outside GPU kernels, but that
  remainder includes the CPU scheduler, graph setup, tokenization, sampling and
  the expert matmuls together. It does not establish the vec-dot share.

## Why no implementation

The task requires a measured share before proposing a new internal
representation with its RAM/startup cost and fallback. That share is not
available here, and the existing AVX2 kernels are already the specialized path;
a speculative rewrite would violate the pre-stated evidence rule and could
easily regress. No quantization, GGUF or layout change was made.

## Reopen condition

Enable CPU sampling (lower perf_event_paranoid or run under an instrumented
build with per-op CPU timers) and measure the vec-dot share of warm decode. If
the two dot kernels account for a large enough fraction that an IPC/memory
improvement can plausibly move >=5% of decode, implement a bounded variant with
a bit-identical regression test and a fallback.
