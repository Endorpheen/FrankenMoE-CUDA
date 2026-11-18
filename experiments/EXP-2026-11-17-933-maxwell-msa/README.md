# EXP-2026-11-17-933: Maxwell block-sparse attention (MSA) in the runtime

## Hypothesis

Maxwell-MoE refuses contexts above its dense support window because the
block-sparse indexer (MBSA) is not implemented. Porting the sparse branch from
the reference graph (`minimax-m3`) makes contexts above the window executable
without changing the dense path.

## What was built

1. **MSA KV cache brought back** together with the matching attention input.
   The earlier segfault came from pairing the MSA cache with the plain
   `build_attn_inp_kv`.
2. **Maxwell MSA input** (`mw_input_msa`): position mask, position -> cell map,
   decode gather map, cell -> position-block map, and the route bias. The bias
   forces the prefix blocks up to the query and the local window, the same
   decision as the Goose `mx_route_bias`.
3. **Sparse branch**: indexer Q/K with RoPE, indexer-K written into the indexer
   cache, block-max scores over the indexer keys, top-k blocks with the bias,
   per-stream gather of K/V through the cell map, and one grouped flash
   attention with the gathered mask. Batch/prefill uses the per-stream loop.

Both paths are chosen by `ctx > block_size * support_blocks`, and the parameter
values come from `hparams` (the same values the cache selection uses).

## Correctness, synthetic model (2 layers, hidden 64)

| context | path | result |
|---|---|---|
| 256 | dense | `l1=31.379171`, argmax 225, identical to before the change |
| 1024 (> window 512) | sparse | `l1=31.388003`, argmax 225, all logits finite |
| 256, no indexer tensors | dense | `l1=31.734641`, argmax 193, unchanged |

The dense numbers are bit-identical to the pre-change build, so the sparse work
does not disturb the short-context path.

## Real model, contexts above the dense window

Maxwell-MoE Q4_0, `block_size=128`, `total_support_blocks=32` (dense window
4096), prefix/local window 1024, 16 remote blocks.

| run | prompt_n | prefill tok/s | decode tok/s | text |
|---|---:|---:|---:|---|
| ctx 32768 | 4801 | 401.5 | 15.7 | coherent continuation |
| ctx 32768, warm | 4365 | 403.6 | 38.3 | coherent continuation |
| ctx 32768, warm | 4365 | 399.1 | 38.5 | coherent continuation |
| ctx 32768, warm | 4365 | 401.0 | 35.5 | coherent continuation |

Requests above 4096 tokens are served instead of refused, and the output is
coherent, which is what the sparse branch is for.

## Same-load benchmark (the Apex procedure) on the candidate

Prompt 2048, generation 128, 1 warmup excluded + 3 measured, ctx 4096.

| candidate | prefill tok/s | decode tok/s | RSS MiB | VRAM MiB |
|---|---:|---:|---:|---:|
| v9, own CUDA build, own server | 1257.46 | 40.46 | 21177 | 10608 |

## Measurement defect found while comparing builds

`maxwell-candidate-v3` reported 1846.86 / 63.18 on this test. It is not a
comparable candidate: its `llama-server` has

    RUNPATH /home/end0/AI/LlamaServer/franken-recovery-20261116/support/bin

so the loader resolves `libllama`/`libggml` from the accepted support tree and
ignores the copies next to the binary. Its `libllama.so.0` also has a different
hash from the one the loader actually used. Any candidate measured through such
a server describes the accepted libraries, not the candidate.

A/B in one build tree, same test, same machine:

| library set | prefill tok/s | decode tok/s |
|---|---:|---:|
| sources before this change | 965.88 | 33.17 |
| sources with the sparse branch | 1236.48 | 39.76 |

The sparse change is 28% faster on the short-context path than the tree it
replaced, so it does not carry a hidden dense-path cost.

## Gates

- `tools/maxwell-convert.py` test: 89 checks, 0 failures.
- Synthetic dense path bit-identical to the pre-change build.
- Contexts above the window: load, decode, coherent text.
- Raw logs in `raw/`.

## Verdict

ACCEPTED for the sparse path: contexts above the dense window are executable and
coherent, the dense path is unchanged bit-for-bit, and the change is not slower
than the tree it replaced. Route-level numeric equivalence against an oracle is
the next gate (block scores, top-k set, gathered mask) and is not claimed here.
