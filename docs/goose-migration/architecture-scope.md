# Architecture scope: what we keep and what we drop

Recorded 2026-11-08 by owner directive.

> Full compatibility with historical llama.cpp models is not wanted. Priority is modern
> architectures plus universal inference infrastructure.

This document is the single place that decides which of the 166 model builders in
`src/models/*.cpp` carry a migration requirement. The plan's task ledger counts only the kept set;
anything not listed as kept is out of scope and carries no requirement.

## Rule

An architecture is **kept** if it is either:

1. the Apex target itself (`qwen35moe`), or
2. actively used for current work in this project (Qwen lineage), or
3. a modern architecture that shares the common graph/attention/MoE machinery and is plausibly
   served by this runtime, or
4. the infrastructure families that modern models build on (dense transformer, MoE, hybrid
   linear-attention, MLA, sliding-window, vision-language through the shared graph).

An architecture is **dropped** if it is superseded, unmaintained in practice, or brings a whole
operator family (convolution, image, audio, training) that nothing else in the kept set needs.

## Kept

Modern text and MoE: `qwen35moe` (Apex), `qwen4exp`, `qwen3`, `qwen3moe`, `qwen3next`, `qwen2`,
`qwen2moe`, `deepseek32`, `deepseek4`, `kimi-k3`, `kimi-linear`, `glm5-next`, `glm4-moe`, `glm4`,
`glm-dsa`, `gemma3`, `gemma3n`, `gemma4`, `gemma4-assistant`, `mistral3`, `mistral4`, `minimax-m3`, `minimax-m2`,
`minimax-01`, `ernie4-5`, `ernie4-5-moe`, `grok`, `seed-oss`, `step35`, `hunyuan-moe`, `hunyuan-dense`,
`exaone4`, `exaone-moe`, `granite-moe`, `granite-hybrid`, `granite-swa`, `lfm2`, `lfm2moe`,
`jamba`, `nemotron-h`, `nemotron-h-moe`, `plamo3`, `olmo2`, `olmoe`, `arctic`, `dbrx`, `dots1`,
`qwen35`,
`command-r`, `cohere2`, `cohere2moe`, `bitnet`, `llama4`, `llama`, `smollm3`.

Hybrid / linear-attention / SSM (needed by the kept hybrids): `qwen3next`, `kimi-linear`,
`mamba2`, `mamba`, `rwkv7`, `rwkv6`, `granite-hybrid`, `jamba`, `lfm2`, `nemotron-h`,
`delta-net-base`, `plamo2`, `arwkv7`, `rwkv7-base`, `rwkv6-base`, `rwkv6qwen2`, `mamba-base`.

Vision-language through the same graph (text path is the requirement; the projector is not):
`qwen3vl`, `qwen3vlmoe`, `qwen2vl`, `gemma3`, `internlm2`, `minicpm3`, `minicpm`, `glm4-moe`.

## Dropped

Legacy dense and encoder architectures whose serving story is over, or that exist only to satisfy
an old checkpoint format:

`bloom`, `gpt2`, `gptneox`, `falcon`, `falcon-h1`, `mpt`, `starcoder`, `starcoder2`, `baichuan`,
`chatglm`, `codeshell`, `refact`, `stablelm`, `openelm`, `t5`, `t5encoder`,
`bert`, `nomic-bert`, `nomic-bert-moe`, `jina-bert-v2`, `jina-bert-v3`, `neo-bert`, `modern-bert`,
`eurobert`, `jais`, `jais2`, `xverse`, `orion`, `deci`, `arcee`, `hrm-text`, `maion-coder`,
`maincoder`, `smallthinker`, `nanbeige`, `pangu-embed`, `pangu-embed`-family, `plm`, `mellum`,
`maple`, `laguna`, `spark2-5`, `muse-glimmer`, `muse-spark`, `kolibri1`, `k2-horizon`,
`grovemoe`, `afmoe`, `apertus`, `bailingmoe`, `bailingmoe2`, `bailingmoe3`,
`chameleon`, `clef`, `cogvlm`, `dflash`, `dream`, `eagle3`, `llada`, `llada-moe`, `mimo2`,
`openai-moe`, `phi2`, `phi3`, `phimoe`, `plamo`, `dots3note`, `deepseek2ocr`, `paddleocr`,
`gemma-embedding`, `gemma-embedding2`, `llama-embed`, `flint`, `rnd1`, `talkie`, `vllm-decision2`,
`beam`, `clip`, `wavtokenizer-dec`, `qwen3tts`, `pockettts`.

Superseded generations where a newer builder in the kept set covers the same family: `qwen`
(v1 — named explicitly as legacy by the owner), `deepseek`, `deepseek2`, `gemma`, `gemma2`,
`exaone`, `granite`, `granite-switch`, `olmo`, `nemotron`, `lfm-base`, `hy-v3`, `hy-v4`,
`hunyuan-vl`.

Also dropped:

- **Training and optimizer operators** — `cross-entropy-loss`, `opt-step-adamw`, `opt-step-sgd`,
  `ggml-opt`. This is an inference runtime; training is out of scope.
- **Image and convolution operators** — `conv2d`, `conv3d`, `conv-transpose`, `conv2d-dw`,
  `im2col`, `col2im`, `pool1d`, `pool2d`, `upscale`, window partition. No kept architecture uses
  them once the legacy vision encoders are gone.
- **Multimodal encoders, projectors and audio** — `tools/mtmd/**`, `tools/tts/**` and the audio
  codecs. The text path of a vision model stays; the vision tower does not.
- **Alternative backends** — Metal, Vulkan, SYCL, CANN, HIP, MUSA, OpenCL, OpenVINO, WebGPU,
  ZDNN, ZenDNN, RPC, Hexagon, Graphcore/ET, BLAS wrappers. This project targets CUDA and CPU.
- **Legacy tokenizer families** not required by kept architectures — `WPM`, `UGM`, `RWKV`,
  `PLAMO` tokenizers stay only if a kept model needs them.

## What is deliberately *not* dropped

`fwht_cuda` was a candidate for removal as "image math", and the census says otherwise: it is
reached from `ggml_cuda_mul_mat` through `ggml_cuda_op_mul_mat_use_fwht`
(`GGML_HINT_SRC0_IS_HADAMARD`), at 0.3% + 0.3% of kernel time in a real Apex run. It is live
inference code, so task `P3-14` is narrowed to FWHT rather than deleted. Removing a task because
its name sounds legacy, without checking the census, is the exact mistake this note exists to
prevent.

## Counting

| | tasks |
|---|---:|
| Total before this directive | 160 |
| Removed outright | 8 |
| Narrowed in place | 3 |
| **Total after** | **152** |

The 8 removed tasks each covered a whole operator or architecture family: `P2-11` (CPU
conv/pool/image), `P2-12` (training/optimizers), `P3-15` (CUDA backward/optimizer), `C2-01`
(remaining tokenizer families), `C2-02` (remaining architecture adapters), `C2-03` (optional
compatibility matrix), `P10-03` (multimodal/audio/RPC tools), `P10-04` (alternative backends).

Task count is not effort and not code volume. By audited line count the dropped scope is large:
`src/models/**` is 454 files and 114,189 lines, alternative backends 164 files and 19,108 lines,
multimodal/audio 172 files and 66,292 lines, training 20 files and 3,616 lines — roughly 203,000
lines that no longer carry a requirement. That figure is context, not a completion percentage.
