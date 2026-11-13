# Practical decode candidates for Qwen3.8-Flash-Next

Research date: 2026-09-09. Prepared by the senior helper in Codex. Igor selects experiments and authorizes baseline promotion.

Scope: RTX 4070 12 GiB (CUDA sm_89), Ryzen 9 5950X (AVX2/FMA), CPU-offloaded experts, GPU dense operations, split MTP n_max=2, and the accepted EXP-046 runtime. Prefill optimization is deferred. The useful target is >=2% sustained single-response decode tok/s, not aggregate throughput.

These are three distinct directions, not three authorized experiments. Igor selected candidate 1 for implementation in a separate tree/build, without running tests. Candidates 2 and 3 are future options. Previous graph-cache and draft-thread experiments are not reopened.

## 1. CUDA radix top-k for QSA: selected as EXP-053

Benefit: select the QSA attention subset without sorting every context position. Potentially improve generation inside long documents/dialogues and reduce temporary sorting storage. This is not the sampler's top-k setting.

Practical evidence: Inovello reports approximately 9-12% higher decode at about 119k populated context on Flash-Next, 2x RTX 3090, DDR4 and CUDA 12.0. Three seed medians were 30.2/33.7, 30.2/33.3 and 30.4/33.1 tok/s (control/candidate, 42 requests per median). A bounded quality screen is documented, not universal output equivalence. These results have not been reproduced here.

The implementation uses radix selection above an 8192-column threshold when native DeviceTopK is unavailable. Our CUDA 12.6/CUB 2.5 baseline has the older argsort fallback. The algorithm does not require dual GPUs. A newer DeviceTopK build is a different comparison.

Sources:

- [Pinned code and reproduction notes](https://github.com/Inovello/llama.cpp/blob/dd64a3db0c453b0e03de15574ff874c2dc77cb28/examples/flashnext-topk/README.md)
- [Related CUDA proposal, not the exact measured implementation](https://github.com/ggml-org/llama.cpp/pull/28366)
- [Radix selection predecessor](https://github.com/ggml-org/llama.cpp/pull/27466)

Local scope: only the top-k delta, no expert cache, MTP-depth, checkpoint, quantization or launcher change. See [EXP-053](../experiments/EXP-2026-09-09-053-cuda-radix-topk.md) and its [tester handoff](../results/archive/EXP-2026-09-09-053/TESTING.md).

Risks: insufficient populated context; a threshold measured on another GPU; equal cutoff scores; graph/scratch lifetime; no guarantee of lower scratch for every shape versus the old chunked sort. Setting ctx=196608 is not a substitute for filling it.

## 2. Reduced-vocabulary MTP output head: backlog

Benefit: read and multiply fewer output-head rows for each proposed token while retaining the target's full vocabulary and verification. This changes draft-head cost, not MTP depth or thread count.

Practical evidence: MiaAI-Lab implemented this exact-model optimization in vLLM on one DGX Spark. The optimization report shows single-stream prose 36.9 -> 46.3 tok/s and attributes most of the gain to a 65,536-token draft subset. A later shipped subset contains 47,149 token IDs. Its BF16-head savings are not a forecast for our quantized sidecar.

Sources:

- [Project and measurements](https://github.com/MiaAI-Lab/Qwen3.8-Flash-Next-Single-DGX-Spark)
- [Head slicing and token-ID mapping implementation](https://github.com/MiaAI-Lab/Qwen3.8-Flash-Next-Single-DGX-Spark/blob/main/files/patch_mtp_draft_vocab.py)

Portability: plausible algorithmic port, not a drop-in llama.cpp/GGUF patch. Audit quantized row alignment, token-ID mapping, greedy/probabilistic speculation semantics and target/draft weight ownership. The published patch keeps the full head and adds a reduced copy: reduced traffic does not automatically mean reduced VRAM.

Risk: English/code-biased vocabularies may reduce Russian acceptance. Test Igor's languages. Do not infer sampled-distribution correctness merely from the existence of target verification; inspect the receiving runtime's acceptance/correction algorithm.

Possible first step after EXP-053, only if Igor selects it: source-level feasibility and a cost budget for our quantized draft head. No automatic model conversion or vocabulary truncation.

## 3. Fused hyper-connection GPU operations: backlog

Benefit: combine matrix-vector output and scale/activation/multiply epilogues, or REPEAT+MUL+ADD, to reduce intermediate traffic and dispatches. This changes actual kernels, unlike EXP-052's L2 graph-instance cache.

Practical evidence: apepojken's Flash-Next Vulkan implementation reports 25.5 -> 26.9 tok/s shallow and 23.2 -> 23.9 at 8k for mat-vec epilog fusion. A subsequent broadcast-scatter fusion reports 26.9 -> 27.2 shallow and 23.9 -> 24.2 at 8k. These are Strix Halo/RADV results, not CUDA measurements. The first commit describes floating-point variation in its output checks, not universal bitwise parity.

Sources:

- [Mat-vec epilog implementation and measurements](https://github.com/apepojken/llama.cpp/commit/3c82e945a2eda6408a12176b247e6e60ae7edaf6)
- [REPEAT+MUL+ADD implementation and measurements](https://github.com/apepojken/llama.cpp/commit/a4c5df63314d756275a33fc02407b9cbe76194dd)

Portability: same model operations, but requires CUDA code. First inspect existing CUDA fusions and exact shapes; do not duplicate an already-fused path. Preserve dependencies, strides, aliases and in-place residual reads. Do not remove CPU/GPU synchronization to manufacture a gain.

Possible first step: select one actually unfused decode chain, one kernel change and an OFF path. Do not import the whole fork.

## Decision policy

Only candidate 1 is active. Public results motivate experiments but do not change our baseline. Keep the accepted model/profile unless Igor explicitly approves a change. Record null results as well as gains. Promotion requires correctness, useful paired model results and Igor's decision.
