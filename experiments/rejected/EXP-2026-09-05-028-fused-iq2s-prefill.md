# EXP-2026-09-05-028-fused-iq2s-prefill

Status: `REJECTED`.

## Hypothesis

For multi-token MoE prefill, decode each IQ2_S weight block once and reuse it across several Q8_K activation rows. The existing single-token vector-dot loop remains unchanged, so decode throughput should not be affected.

## Isolated implementation

The implementation is confined to `work/llama.cpp-exp028` and the build to `build/exp028-fused-mmid`. It is guarded by `GGML_CPU_IQ2S_FUSED=1`. No file in the production integration tree was changed.

The full CUDA correctness gate passed:

```
13644/13644 tests passed
Backend CUDA0: OK
```

## End-to-end result

Both arms used the same 64K MTP server configuration, 12 CPU threads, CPU MoE, EHS off, the same fixed 395-token prompt, and a deterministic 16-token completion. The output content SHA-256 was identical in all five control requests and all five retry requests:

```
24122785e34e88b37d241f7c4f1f303575ab88c7065d05cbc6c0f9a7c60085b7
```

The warm control median was 131.676 prefill tok/s (3000.499 ms). The warm fused median was 130.191 prefill tok/s (3034.126 ms), a -1.13% change. This misses the +3% acceptance threshold and is within ordinary variability.

The first candidate attempt also aborted on request four at CUDA graph instantiation with an out-of-memory error. A clean retry completed five of five requests, but does not change the negative performance result.

## Decision

Do not merge or enable the fused path. The direct AVX2 implementation did not improve end-to-end prefill and initially exposed a CUDA-graph OOM in this tight 64K VRAM profile.

## Revisit conditions

Revisit only with a lower-register-pressure kernel or a blocked layout that improves cache locality without increasing CUDA graph pressure. Start with a microbenchmark that represents the exact Qwen expert matrix shapes, then repeat the same end-to-end protocol.

## Reproduction artifacts

- Fixed request: `benchmarks/inputs/exp028-prefill.json`.
- Raw responses and logs: `/tmp/exp028/` for the current machine session.
- Isolated source diff: `git -C work/llama.cpp-exp028 diff -- ggml/src/ggml-cpu/ggml-cpu.c ggml/src/ggml-cpu/ggml-cpu-iq2s-grid.h`.
