# External LLM optimization ideas

Status: **deferred research notebook**. This file does not add implementation tasks, change the migration GOAL, or authorize optimization experiments. Nothing below has been validated on FrankenMoE. Extend this notebook when relevant external research is reviewed; do not start a monitoring or benchmark job merely to populate it.

Evidence labels: **R** = authors report an experiment; **S** = source mechanism, model, or simulation; **H** = our untested application hypothesis. R is attribution, not independent reproduction. A handbook's citation of another paper is not a new handbook experiment.

## HF-MOE-HANDBOOK: first source

Reviewed: 2026-11-07. [The Ultimate MoE Training Handbook][HF], especially Systems and From training to serving. Source revision: `ef70726e6f8b99d74298a16a4894340f288a58c2`; [pinned training chapter][HF3], [pinned serving chapter][HF4]. The embedded site was unavailable to the reader tool; the official Space's source chapters were read directly. The original papers below were also inspected.

| Idea and source | Principle and external evidence | Potential FrankenMoE benefit — H, not measured | Limitations and distinctions |
|---|---|---|---|
| Reduce served Top-K — [Chen/Yao][K], [HF, serving][HF4] | **R:** execute fewer experts; authors report reduced routed compute with limited task degradation in specific models. | Less CPU expert work and fewer weight reads could help decode. | Changes model outputs. Resident weights remain; fewer active parameters do not imply equivalent total-runtime acceleration. |
| Correct routing mixture coefficients — [Chen/Yao][K] | **R/S:** execute top `k1`, normalize by top-`k2` probability mass; denominator affects quality. | Could preserve more quality than simply lowering K. | These are expert-output coefficients, not edits to the trained router matrix. Quality tasks and perplexity can disagree; no universal `k2`. |
| Layer-specific active experts, LExI — [paper, method and limitations][L] | **R:** synthetic-input sensitivity profiling plus evolutionary search assigns a **static K per layer**, without calibration data or retraining. | Spend an active-expert budget where layers are most sensitive. | Not token-adaptive routing. Authors tested H100/vLLM configurations; applicability to quantized CPU MoE is unknown. It retains expert weights. |
| Expert pruning — [LExI evaluation][L] | **R:** removing experts saves storage, but authors found inconsistent inference gains and possible load imbalance. | An optional future memory-capacity trade-off when the full model cannot fit. | Pruning changes the model; unlike reducing active K, it removes resident weights. Quality and speed need separate evidence. |
| Fuse token permutation/copy — [HF, Systems][HF3] | **R:** handbook training measurements show lower permutation/copy overhead after fusion. | Avoid temporary buffers and repeated memory passes in dispatch/combine. | Training step results are not decode results. Preserve routing, inverse maps, empty experts, duplicate contributions and accumulation semantics. |
| Grouped quantized GEMM with aligned batches — [HF, Systems][HF3] | **S:** group expert matrix operations; quantized grouped GEMMs require aligned token counts. | Jointly choose packing, tile shapes and batch handling for Goose CPU/CUDA kernels. | Padding can waste work. GGUF formats and supported instructions differ from Transformer Engine recipes; small decode batches may lose. |
| Quantize experts selectively — [HF, Low-bit experts][HF4] | **S:** expert-heavy storage makes mixed precision attractive; retain sensitive components more accurately. | Candidate input to future GOQ precision/layout research, after migration. | Lower weight size is not proof of faster GEMM. Weight-only and activation quantization have different quality risks; hardware support matters. |
| Overlap transfer and expert compute — [HF, Systems][HF3] | **S:** pipeline dispatch, computation and combine. | A future CPU/GPU staging pipeline might hide transfers. | Multi-GPU training evidence does not establish PCIe gains. Buffer lifetime, cancellation, synchronization and memory pressure require explicit contracts. |
| Separate attention/KV and expert resources — [HF, serving][HF4] | **S:** attention carries request state; experts carry most weights. | Consider residency and scheduling according to the actual memory bottleneck. | Disaggregated GPU-pool results do not prove benefits on our hybrid machine. Transfer and batching overhead can dominate. |
| Protect routing numerics; distinguish balancing from serving — [HF, router scoring and balancing][HF3] | **S:** router precision can affect expert selection; balancing biases are training mechanisms. | A future quality analysis could isolate routing changes from expert arithmetic error. | Do not add load-balancing bias or change router precision casually at inference. This may change selected experts, rather than merely schedule identical work. |

The handbook illustrates some mechanisms with simulations. Those illustrations are not hardware measurements. Top-K quality claims above belong to Chen/Yao; LExI's results belong to its authors. The permutation result is a handbook training observation. No external speed multiplier is transferred to Apex, Qwen, FrankenMoE, or Fried Goose here.

## How to extend this file

Append a dated source entry with a stable ID, primary link, version/commit, mechanism, author-reported evidence, hardware/workload/baseline, possible benefit, and limitations. Label local extrapolation H. Keep contradictory and negative findings. Separate mathematical work reduction, weight size, peak RAM/VRAM, prefill, decode, latency, and aggregate throughput. Do not add unrelated measurements or implementation changes while collecting references.

Before a future idea becomes an optimization task, it needs an explicit decision after the current functional migration priority. Approximate routing/quantization proposals need quality criteria; exact memory/kernel proposals need semantic and ownership criteria. Evaluation design will be chosen then, not executed now.

[HF]: https://huggingface.co/spaces/huggingface/moe-blogpost
[HF3]: https://huggingface.co/spaces/huggingface/moe-blogpost/blob/ef70726e6f8b99d74298a16a4894340f288a58c2/app/src/content/chapters/3-anatomy-of-moe-training.mdx
[HF4]: https://huggingface.co/spaces/huggingface/moe-blogpost/blob/ef70726e6f8b99d74298a16a4894340f288a58c2/app/src/content/chapters/4-from-training-to-serving.mdx
[K]: https://arxiv.org/abs/2609.04575v1
[L]: https://arxiv.org/html/2509.02753v1#S4
