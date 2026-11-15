# EXP-2026-11-15-701 — the attention KV cache: write, keep, read back

Base: `7561018c`. No model run, no GPU.

## 1. Result

```
token 0: differs=0
token 1: differs=0
token 2: differs=0
FG_KV_CACHE tokens=3 head_dim=8 n_ctx=8 bad=0 PASS
```

Three tokens, each writing its k/v into a persistent cache through the module's slot and
attending over the positions written so far. The output is bit-identical to the same graph
over a fresh buffer holding the same positions - i.e. persistence changes nothing that
arithmetic would not already give.

## 2. The slot is a Goose decision

`goose/model/gdn-ops.goose::attn_kv_slot` computes

```
offset = layer * (n_kv_heads * n_ctx * head_dim * elem) + pos * (head_dim * elem)
```

and returns `[head_dim, n_tokens, n_kv_heads, 1]`. The gate does not compute offsets; it
asks for a slot and gets a view. Range checks are in the module (`pos + n_tokens > n_ctx`
fails), so a buffer overrun is refused rather than silently wrapping.

## 3. Why the reference had to be rebuilt

The first reference used a narrower view over the source k/v (`t+1` columns). That differs
from the cached path by rounding alone, because a 3-wide softmax and an 8-wide masked
softmax are different computations - and the gate reported `differs=8` at tokens 1 and 2
for that reason, not because the slot was wrong.

The reference now rebuilds a full-width buffer with the same masked shape, filling
positions `0..t` through the same slot. That isolates the question being asked: does
storing and re-reading give what recomputing gives. It does.

This is the fourth time in this session a failing check was the check's fault. Recorded in
`docs/goose-migration/STATE.md`.

## 4. Two wiring facts this established

1. `ggml_mul_mat(a, b)` reduces over `a->ne[0]`, so the value matrix must be presented in
   `[n_kv, head_dim]` order - transposed relative to its natural storage. The first attempt
   aborted on `ggml_can_mul_mat`.
2. A `[head_dim, n_ctx]` view over a 4-D cache steps by `nb[1] = elem*head_dim`, not by
   `n_ctx*head_dim*elem`. Using the latter skipped a cache row per position.

Neither is a decision; both are substrate conventions that the stack must respect.

## 5. Cost

One arch-unit build, shared with the module export. Gate compiles are seconds.
