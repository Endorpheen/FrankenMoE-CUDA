# EXP-2026-11-15-724 — full performance audit: the bottleneck is expert traffic, not kernels

Base: `5993d662`. `nsys` profiles of the same 32K/512 request at two cache sizes, plus four
ablations and a checkpoint sweep. Where a number is quoted, it came from CUPTI tables.

## 1. The finding that matters: 84 GiB of host-to-device traffic per request

Decode window (63-77 s), 512 tokens generated:

| direction | copies | seconds | avg |
|---|---:|---:|---:|
| **host to device** | **77,581** | **4.241** | 54.7 us |
| device to host | 2,153 | 0.140 | 64.8 us |
| other | 15,894 | 0.044 | 2.8 us |

**86,421 MiB - 84 GiB - moved from RAM to VRAM to generate 512 tokens.** Kernel time in the same
window is 7.93 s, of which 4.24 s is this copying. GPU idle between kernels is 6.43 s of a 17 s
window, and 988 gaps above 1 ms account for 6.40 s of that - they are the CPU waiting on these
transfers, not launch overhead (p50 gap is 1 us).

That is the answer to "where do the 4-5% live": they are not in a kernel. They are in moving expert
weights across PCIe every token.

## 2. Confirmed by varying the cache

| `--moe-cache-mib` | HtoD copies (whole run) |
|---:|---:|
| 4352 | 189,839 |
| 3584 | 214,457 |

13% more traffic with the smaller cache, which is exactly the expected direction and magnitude for
expert thrashing. It also explains the non-monotone speed curve measured earlier: more resident
experts means fewer transfers, not more compute.

## 3. Four ablations, all parity

| arm | decode | vs base |
|---|---:|---:|
| baseline | 39.86 | - |
| `GGML_GOOSE_CUDA_MMQ_DISPATCH=0` | 39.78 | -0.2% |
| `GGML_GOOSE_CUDA_MM_IDS=0` | 39.73 | -0.3% |
| `GGML_GOOSE_CUDA_MMQ_Q8_1=0` + `Q8_1=0` | 39.68 | -0.5% |
| `GGML_GOOSE_CUDA_FA=0` (native FA) | 38.99 | **-2.2%** |

None of the Goose CUDA dispatch routes costs anything measurable; disabling them is neutral to
slightly worse. The one that clearly matters is flash attention, and Goose's is the faster one.

## 4. Checkpoint sweep: not the cause of the gaps

| `--ctx-checkpoints` | decode | RAM |
|---:|---:|---:|
| 0 | **37.66** | 20.7 GiB |
| 32 (reference) | 39.86 | 20.8 GiB |
| 64 | 39.76 | 20.8 GiB |
| 128 | 39.63 | 20.8 GiB |

Disabling checkpoints makes decode **worse by 5.5%**, so the periodic 220-300 ms pauses are not
checkpoint overhead - they are the transfer stalls, and checkpoints are unrelated. The reference
value of 32 is at the optimum of those sampled.

## 5. What the audit says to do next

The measured cost structure of a decode step is:

- 4.24 s expert traffic over 512 tokens = **8.3 ms per token**, against a 25 ms per-token budget at
  40 tok/s;
- 7.93 s of kernel work in the same window;
- the transfers and the kernels overlap only partially, which is why the window is 17 s.

**Reducing expert traffic is worth more than any kernel change available.** The options, in order of
expected value:

1. **More resident experts.** Already at the 4608-5120 OOM ceiling on this GPU, so this is closed
   unless memory is freed elsewhere.
2. **Fewer transfers per expert use.** If the same expert is used by consecutive tokens, it is being
   copied more than once. Whether the runtime keeps a per-expert residency state at all is the
   question - the profiler cannot answer it, the code can.
3. **Overlap.** 6.43 s of the 17 s window is GPU idle; if transfers were pipelined against compute
   rather than blocking, some of that could disappear without touching traffic volume.

Option 2 is a concrete code question and is the next step. Option 3 is a scheduling question in the
GGML_GOOSE expert path.

## 6. Not claimed

That Goose is responsible for the traffic. The reference build moved experts the same way - its
41.94 tok/s came from a larger effective cache and a different patch set. What is measured here is
where *this* build's time goes, which is the only thing that can be optimised.
