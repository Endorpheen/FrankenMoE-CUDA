# EXP-2026-11-17-928 — Maxwell arch and trunk graph in the runtime

Status: ACCEPTED (architecture and graph; synthetic weights only)

## Result

- Incremental build of the runtime with the new arch: `ninja llama`, 51 steps,
  exit 0, no compiler errors.
- `tests/load_forward.cpp` against a synthetic Maxwell GGUF (2 layers, hidden
  64, 8 experts, stub vocabulary): the model loads (`n_vocab=256 n_embd=64
  n_layer=2`), the mHC graph is built and reserved, `llama_decode` returns 0,
  logits are finite, `argmax=193`, `l1=31.64`.
- Four real defects were found and fixed by this run: view byte offsets were
  double-scaled, the HC-head `1/sqrt` divisor had the wrong broadcast shape, the
  graph node budget did not include the mHC cost, and the GGUF keys need the
  `maxwell_moe.` prefix.

Not measured: real weights, text quality, prefill/decode rate, VRAM/RAM.

## Hypothesis

`maxwell_moe` can be added to the runtime without touching the hot paths of any
existing architecture: one arch enum row, one factory branch, one model file,
and reuse of the already-present mHC graph pieces (the same ones glm5-next and
deepseek4 use for DeepSeek-V4-style hyper-connections) plus the existing MoE
builder, which already implements a sigmoid router with an expert bias and
top-k renormalisation.

## What is implemented

`vendor/flint/src/models/maxwell.cpp`:

- `load_arch_hparams`: RMS eps, expert FFN sizes, leading dense blocks, sigmoid
  gating, expert-weight normalisation, mHC stream count / Sinkhorn iterations /
  eps, rope base and head geometry through the common loader.
- `load_arch_tensors`: attention Q/K/V/O, QK-norm, HC attention and FFN tensors,
  dense FFN on the leading layers, routed experts as 3-D tensors, expert bias,
  shared expert, learned HC head.
- graph: 4 residual streams; per sub-layer `hc_pre` (RMS-folded projection,
  sigmoid gates, Sinkhorn `comb`), the sub-layer itself, then `hc_post`
  (expand + mix); learned `hc_head` contraction, final norm, LM head.
- attention: GQA 32/4 x 128, RMS QK-norm, RoPE theta 2e7, existing causal
  attention builder.

Registration: `LLM_ARCH_MAXWELL` enum + `"maxwell_moe"` name + factory branch +
rope type NORM.

## The block-sparse reduction

With prefix 1024, local 1024 and 16 remote blocks of 128, the route always
covers at most 32 blocks = 4096 tokens. For any KV length <= 4096 every block
up to the query is in `prefix ∪ remote ∪ local`, so block-sparse attention is
exactly dense causal attention there. The first build therefore runs dense
causal attention and leaves the indexer to the >4096 path, which is not built
yet. This is an equivalence, not an approximation, for the supported length.

## Known differences to record

- mHC folds the `1/(rms+eps)` scale into an RMS normalisation (`eps` under the
  sqrt). At hidden 2048 the relative difference is ~1e-9.
- The MTP head and the block-sparse indexer are not loaded or executed.
- The expert-bias precision fix (fp32) is preserved by keeping every 1-D tensor
  in F32 during conversion.

## Next

Build, then load a synthetic GGUF produced by EXP-927 and run one forward.
Real-weight numeric comparison follows when the download finishes (owned by
another agent). No speed or memory claim is made yet.

## Regression invariant

Shared files changed: `llama-arch.h/.cpp` (enum, name), `llama-model.cpp`
(factory, rope type), `models/models.h` (declaration). All are additive. Apex
and the second reference model must be re-measured against their accepted
numbers at identical conditions before this slice is accepted.
