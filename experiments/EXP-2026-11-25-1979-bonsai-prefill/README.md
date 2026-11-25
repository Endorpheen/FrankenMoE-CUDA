# EXP-2026-11-25-1979 — Resident Bonsai microbatch prefill

**Diagnostic baseline.** The complete Goose trunk processes token-major
microbatches using EXP-1978 PQ2 MMQ, the frozen batched GDN kernel and
frozen RoPE, Q4_0 cache writer and two-pass attention. The same graph
returns to the original FP32 matvec path for each decode step. The
model-specific assembly does not change the shared runtime or GDN image.

## Results

One unprofiled request per configuration on RTX 4070. All used the same
local PQ2_0-mtp GGUF, context 36864, microbatch limit 1024, Q4_0 K/V, greedy
generation with EOS suppressed, raw token IDs and MTP disabled. Shared inputs
were the exact [EXP-1977 request](../EXP-2026-11-25-1977-bonsai-prism-32k-reference/request.json)
or the first 1025 IDs of that prompt with eight requested outputs.

| Request | Engine | Prefill tok/s | Decode tok/s |
| --- | --- | ---: | ---: |
| 1025/8 | FrankenMoE diagnostic CLI | 233.16 | 15.22 |
| 1025/8 | PrismML llama.cpp HTTP | 1168.65 | 52.04 |
| 32768/512 | FrankenMoE diagnostic CLI | 56.73 | 9.90 |
| 32768/512 | PrismML llama.cpp HTTP | 1093.21 | 47.97 |

All 512 IDs matched on the long request. On the short request, the engines
differed at output position four, Goose 314 and Prism 310, with the other
seven IDs equal. This remains an open numerical parity issue. The preceding
short gates matched scalar Goose on 4/4 and 16/16 greedy outputs across
one and multiple microbatches. That test used ubatch 128 and a 273-token
prefix crossing the 256-key boundary. The short pair uses the same units as
the full reference and counts the first returned token in decode throughput.
Goose physically computes one fewer decode step because prefill evaluates the
first token's logits.

The 32K run took 577.587 s prefill and 51.722 s decode. Observed peak
VRAM was 8788 MiB for the process and 11420 MiB for the board. Process peak
RSS (VmHWM) was 7,418,448 KiB. System swap-out delta and process swap were
zero. The complete 7,481,217,024-byte GGUF payload including inactive MTP
tensors and 1,569,405,696 B of workspace were allocated on CUDA.

## Critical path

One separately instrumented 1025/8 run with Nsight Systems, partitioned
after the first vocabulary projection. The reported GPU spans and sums
over kernels exclude weight upload and allocation at startup.

| Prefill component | GPU ms | Decode component | GPU ms |
| --- | ---: | --- | ---: |
| PQ2 MMQ | 2433.28 | PQ2 FP32 matvec | 365.23 |
| Gated delta recurrence | 1061.02 | Q4 attention tiles | 75.81 |
| Q4 attention tiles | 341.48 | Normalization | 48.43 |
| F32 decay/beta projections | 87.88 | F32 decay/beta projections | 14.90 |
| All kernels | 4144.71 | All kernels | 517.03 |
| First-to-last GPU kernel span | 4146.50 | First-to-last GPU kernel span | 527.59 |

The decode copies within this GPU span took roughly 0.24 ms across seven
physical decode steps. A CUDA API launch can block while the queue is
full, so its host duration overlaps GPU execution. Summing host API
duration with GPU time overstates the total. The GPU kernel budget and
the stationary VRAM account rule out weight migration as the main loss.

The first 4096-token block took 21.811 s and the last 4096 took 124.641 s.
The resident tiled attention currently launches per query head over KV
segments, repeatedly rereads Q4 keys and values, and stages partial softmax
results. The other major operations have fixed dimensions for a given
microbatch. This explains the expected context dependence, but the short
trace does not assign an exact long-context kernel budget. The existing
Apex Flash MMA shape gate assumes an 8:1 Q:KV ratio. Bonsai uses 6:1 and
currently takes the generic tiled path.

Next short investigations, in order of measured savings potential:

1. Adapt tiled Flash Attention for Bonsai's 6:1 Q:KV using groups padded
   to 8 during the first qualified implementation. Compare Q4 outputs and
   masked rows against the generic path before testing short generation.
2. Reduce repeated PQ2 unpack and loads in the FP32 matvec. First try
   four ordered lane updates per quant block with one scale conversion,
   preserving the FMA and warp reduction order. Compare projection bytes
   before testing decode.
3. For prompt MMQ, profile repeated packed weight traffic across token
   tiles and scale/unpack pressure. For the 48 GDN layers, qualify the
   existing chunked Tensor Core route with the 16/48 head geometry.
   Confirm recurrent state and generation across chunk-to-decode transitions.

## Reproduction

Rebuild with the bounds, CUDA image and bootstrap of the preceding
Bonsai experiments. Use `BONSAI_UBATCH=1` for the scalar control.

```sh
exp=experiments/EXP-2026-11-25-1979-bonsai-prefill
bash "$exp/source/build.sh" build/bonsai-prefill-repro
BONSAI_UBATCH=128 build/bonsai-prefill-repro/bonsai-batch-infer \
  "$HOME/AI/LocalAI/Models/Ternary-Bonsai-2.5-27B-PQ2_0-mtp.gguf" \
  build/bonsai-prefill-repro/prefill.cubin build/bonsai-prefill-repro/frozen-goose.ptx \
  36864 4 --ignore-eos 1511 17 248046 1511
```

Pass the first 1025 IDs from EXP-1977 as CLI arguments with `BONSAI_UBATCH=1024`
and output budget 8 for the short comparison. Use the same shortened JSON
for PrismML. The full settings and artifact hashes are in [results.json](results.json).
The trace summary is in [profile-summary.json](profile-summary.json). The
local build, raw outputs, RAM/VRAM samples, nsys-rep and SQLite remain in
`build/bonsai-prefill-1979-final`.

The phase follows short requests until throughput improves. Standard HTTP,
MTP, the outstanding numeric differences, and the accepted performance
baseline remain open.
