# EXP-2026-11-17-926 — Maxwell-V1-Flash decision module

Status: ACCEPTED (first slice: decisions only)

## Hypothesis

The parts of Maxwell-V1-Flash that llama.cpp has no equivalent for are
decisions, not kernels: block-route geometry, per-group block-max remote
selection, the mHC Sinkhorn projection, hyper-connection gating, the sigmoid
router with expert bias, and the MTP index roll. Those can be ported to Goose
and proved against the HF reference before the weights finish downloading.

## Reference

`huggingface.co/maxwell-model/Maxwell-V1-Flash`, revision
`ca536c4380e0600d6aebdb808d2ce65ec724161b`:
`modeling_maxwell_moe.py` and `configuration_maxwell_moe.py`. The reference
implementation in `tests/fixture.cpp` was transcribed from those files
independently of the Goose source.

## What is owned

`source/runtime.goose`:

| entry | native behaviour |
|---|---|
| `mx_geom` | `num_prefix_blocks`/`num_local_blocks`, block-alignment refusals |
| `mx_selected_count` | `min(max(num_remote_blocks,1), num_blocks)` |
| `mx_router` | sigmoid scores, bias selects but never scales, top-k, score-normalised weights |
| `mx_sinkhorn` | row softmax + eps, column norm, `iters-1` × (row, column) |
| `mx_hc_xnorm` | `1/(‖x‖/sqrt(nC)+eps)` |
| `mx_hc_mix` | `h = r·proj·alpha + bias`; `h_pre=sigmoid+eps`, `h_post=2·sigmoid`, `h_res` logits |
| `mx_hc_head` | `sigmoid(mixes·scale + base)+eps` |
| `mx_route` | block eligibility, per-group block-max top-k, `-1` padding |
| `mx_route_final` | `cat(prefix, remote, local)` with the HF `-1` rules |
| `mx_mtp_roll` | `roll(ids,-1)`, last slot = pad id |

Matrix products, gathers and the block-max reduction stay on the host: they are
ggml-shaped work, not decisions. The module refuses (`<0`) on a rejected input;
the host must propagate the refusal and never substitute another decision.

## Gate

`bash tests/build.sh` → `checks=2180 failures=0`, `MAXWELL-DECISIONS OK`.
Integer outputs are compared exactly, floats within 1e-5 relative (same float32
formula; only summation order may differ). Sinkhorn is additionally checked to
end column-stochastic and to keep the `eps` floor where sigmoid underflows.

`python3 tests/mutate.py` → `mutations=14 survived=0`.

## Explicit non-claims

- This is not a Maxwell runtime. No weights, no graph, no loader, no kernels.
- Nothing here was measured for speed or memory; there is nothing to measure yet.
- The Q/K-norm, RoPE and the projection GEMMs are not in this slice; they map to
  existing ggml ops and will be wired, not re-ported.
- Ties in `topk` are resolved to the lower index and recorded here, because the
  HF oracle does not specify them either.

## Regression invariant

Maxwell is additive. Shared components (GGUF loader, KV cache, backends, server)
are changed only with the Apex reference and the second reference model
re-measured against their last accepted numbers at identical conditions; a
regression blocks acceptance (Igor's addendum, 2026-11-17).

## Next slice

Loader and tensor map for `maxwell_moe` (safetensors shards → our 4-bit
container), then the graph: QK-norm/RoPE, block-sparse attention route wiring,
mHC streams, router/shared expert, MTP head.

## Configuration validation (added)

`mx_cfg_check` owns the load-time rules and derived MBSA geometry: GQA
divisibility, indexer heads == KV heads, indexer head_dim == attention
head_dim, dense lead within the layer count, block alignment of the prefix and
local windows, positive remote/FF sizes, and `n_prefix`, `n_local`, remote and
mHC mix width on success. Gate: 2777 checks, 0 failures.
