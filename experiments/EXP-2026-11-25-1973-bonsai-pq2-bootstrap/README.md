# EXP-2026-11-25-1973: Bootstrap Bonsai dense Qwen35 and PQ2_0

**Status: component validation passed. The end-to-end runtime is not
connected.** There is no model performance baseline or release artifact.

The local file is `Ternary-Bonsai-2.5-27B-PQ2_0-mtp.gguf` (7492338944
bytes). No model files were downloaded or modified. The file uses
GGUF v3 and the dense `qwen35` architecture: 64 main layers with 48
recurrent/GDN branches and 16 full-attention branches, plus one
separate MTP layer. Hidden width is 5120 and FFN width is 17408.
There are 402 type-142 PQ2_0 tensors, eight Q4_0 tensors in the MTP
tail and 456 F32 tensors. The existing Apex loader is scoped to
`qwen35moe`, so it cannot treat this file as Apex.

## Validated modules

| Component | Implementation and check |
| --- | --- |
| GGUF spans and profile layout | Reuse the existing Goose reader with PQ2_0 layout registration. Validate all 866 tensor spans and bind all tensor names, dimensions and types to the dense trunk or MTP layout. A one-byte short file extent is rejected. |
| PQ2_0 and Hadamard | Reuse the existing Q2_0 scalar decoder with the PQ2 block geometry (128 weights per 34 bytes). CUDA unpacking and restored embeddings match Goose exactly. Signed 1024-channel FWHT matches between CPU and CUDA at widths 5120, 6144 and 17408. The 6144 case includes the GDN head permutation. |
| Tokenizer | Reuse the existing FrankenMoE tokenizer with the metadata-only copy of the local GGUF. Every token ID matches the local llama.cpp tokenizer on five fixtures, including Cyrillic, Japanese, code, whitespace and role markers. |
| CUDA projection and FFN | Execute the real output head and the first layer's gate/up/SwiGLU/down chain on resident weights. Sample against the Goose decoder and FP64 products. Max absolute sample errors are 2.97e-9 and 3.66e-11, respectively. This check does not compare final generation or guarantee bitwise equality with the Prism runtime. |

Hadamard forward applies signs before the normalized transform.
Token embeddings apply the inverse transform before signs. Grouped
GDN values need their permutation before signs and rotation. See
the [Prism loader](https://github.com/PrismML-Eng/llama.cpp/blob/prism/src/llama-model.cpp)
and [GDN geometry](https://github.com/PrismML-Eng/llama.cpp/blob/prism/src/llama-model.h).
The [Prism model card](https://huggingface.co/prism-ml/Ternary-Bonsai-2.5-27B-GGUF)
specifies the sampling profiles. Preserve the embedded chat template
and distinguish its thinking mode from the no-thinking evaluation.

## RTX 4070 capacity probe

Load the complete 7481217024-byte tensor payload, including the draft
weights, in one device allocation. Keep it resident while touching
679477248 bytes of Q4_0 KV capacity for 36864 positions and 16
attention layers, 156893184 bytes of planned GDN state, and a
1 GiB placeholder for graph scratch. The device reports 749142016
bytes free after those allocations. Actual serving allocations, draft
KV and any extra graph intermediates have yet to be measured. The
capacity probe does not establish a full-inference peak or speed.

## Remaining integration

- Keep the accepted Apex and Inkling binaries frozen. Connect the
  Bonsai layout to a dense model backend, with per-weight forward
  rotation and token-lookup inverse rotation. Reuse the Qwen35
  Attention, KV, RoPE, GDN and residual machinery where the tensor
  geometry and accumulation match. Validate against the Prism graph
  before taking speed measurements.
- Connect the separate Q4_0 draft layer after validating ordinary
  generation, including recurrent state and draft rollback. Validate
  the shared HTTP tokenization, completion and chat paths, streaming
  and usage on the resulting model.
- Measure actual VRAM placement at 32K/512. Once the entire path has
  passed correctness, record prefill, decode, request timing, peak
  VRAM and RAM, and map exposed CUDA/host/transfer intervals. No
  speed targets or measured loss ranking exist for Bonsai yet.
- Rebuild and run the existing Apex and Inkling correctness and
  performance gates before accepting a shared runtime. These probes
  link no changed production runtime and do not replace those gates.

## Reproduction

Build from the repository with `bash source/build.sh /absolute/new/scratch`.
Run `python3 source/inventory.py <local-gguf> <scratch>/inventory.json`,
then `python3 source/probe.py <scratch> <scratch>/inventory.json --vocabulary`.
The independent tokenizer comparison is
`python3 source/tokenizer-check.py <scratch>/frankenmoe-tokenize <llama-tokenize> <scratch>/vocabulary.gguf`.
Use `--layout-only` before the GPU test if desired. Local reference
CLI: `/home/end0/AI/LlamaServer/llama.cpp/build/bin/llama-tokenize`.
The rebuilt library, cubin and tokenizer match the original artifacts.
See `results.json`, `tokenizer-results.json` and `inputs.sha256`.
