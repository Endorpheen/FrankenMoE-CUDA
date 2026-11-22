# EXP-1715 — Refresh the critical path after four-head attention

RTX 4070, Apex-35B-V1.0 Q4_K_M, 32,768 input and 512 output tokens,
the EXP-1713 profile, default API without probability output. GPU
and CPU traces were captured in separate server lifecycles with
one full warm-up before each capture. The measured requests had
zero prefix reuse and identical IDs and response text.

The standalone ordinary benchmark remains the throughput reference.
The CUDA trace adds overhead. Gprofng samples CPU execution and
does not assign the entire host wall time to stack functions.

| Decode metric | Earlier Goose trace, no probabilities | EXP-1713 trace | Saved Hybrid trace, five alternatives |
|---|---:|---:|---:|
| Wall window, ms | 13637.631 | 12152.053 | 8452.662 |
| GPU busy, ms | 9228.905 | 8045.059 | 6512.834 |
| GPU idle, ms | 4408.725 | 4106.993 | 1939.827 |
| Vector Attention, exclusive ms | 2471.305 | 1498.611 | 1218.477 |
| MMVQ, exclusive ms | 2803.861 | 2564.754 | 2484.396 |
| Bulk H2D, exclusive ms | 1425.655 | 1339.308 | 1764.149 |
| Pointwise and GLU, exclusive ms | 753.070 | 739.460 | 138.850 |
| RMSNorm, exclusive ms | 362.447 | 362.507 | 123.781 |
| GDN, exclusive ms | 352.263 | 353.831 | 56.156 |

The kernel figures describe disjoint exclusive time in those traces.
The saved Hybrid used a different probability payload and runtime
state. Its wall window is historical context. The current target
remains the independently measured 71.17 decode tok/s in no-probability mode.

## Current GPU idle

| Disjoint decode attribution | Milliseconds |
|---|---:|
| Before the next submission, outside CUDA API | 2599.612 |
| During 32-byte route readback | 767.552 |
| During graph launch | 439.851 |
| Outside CUDA API with next work already submitted | 98.052 |
| During async memcpy API | 84.461 |

The largest measured interval outside CUDA API is 1449.037 ms from
logit download completion to the next 8192-byte input upload, a
subset of the 2599.612 ms row. A further 609.208 ms occurs between
async memcpy and graph launch, and 359.893 ms between route readback
and the next async memcpy. These pairs describe event boundaries.
They do not establish the function-level cause of the intervening time.

The CPU capture reports 256 ms of sampled execution during decode,
including 158 ms attributed to the CUDA driver. The timer sample
profile cannot reconcile the 1449.037 ms host-side interval. Targeted
absolute boundary timing is needed around sampling, graph setup, and
the next token submission. Compared with the old Goose profile,
the four-head checkpoint removes 972.694 ms from exclusive Vector
Attention and leaves CPU-to-GPU handoffs as the larger opportunity.

The compact report is `profile-summary.json`. Full GPU and CPU
reports, capture scripts, raw responses and checksums are stored
under `build/exp1715-profile-quad`.
