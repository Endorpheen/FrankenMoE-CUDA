# EXP-2026-11-15-702 — one slot mechanism for both caches

Base: `781e6114`. No model run, no GPU.

## 1. Result

```
FG_STATE_SLOT checks=22 bad=0 PASS
overrange: Goose graph builder validation failed: code=103
```

Layout 0 (attention KV) and layout 1 (recurrent conv) now come from one export,
`state_slot`, and the 22 slots it produces are byte-for-byte the tensors the two
specialised exports produced: same `ne`, same `nb`, same `view_offs`, same `op`.

## 2. Why "same tensor" and not "same values"

An equivalent tensor would be enough for a test that computes through it, and not enough
for this change. The whole reason to unify is that a later fix - to a range check or to the
offset form - cannot land in one cache and miss the other. If the unified function drifted
from either specialised one, the drift would show up here immediately rather than as a
numerical difference three layers up.

## 3. Why layout 1 uses view2 and layout 0 uses view4

That is not an inconsistency, it is what the two specialised versions return:

| layout | specialised | shape returned |
|---|---|---|
| 0 attention KV | `attn_kv_slot` | `view4` `[head_dim, n_tokens, n_kv_heads, 1]` |
| 1 recurrent conv | `gdn_conv_slot` | `view2` `[(kernel-1)*channels, n_seqs]` |

Reproducing them exactly was the requirement, so the layout 1 branch keeps `view2`. The
first draft returned `view4 [(kernel-1), channels, 1, 1]`, which is a different tensor with
the same memory - exactly the kind of "equivalent" that would have broken a caller relying
on the flat slot stride.

## 4. Refusal cannot be probed in-process

`fail()` terminates: `state_slot` with `pos + n_tokens > n_ctx` aborts through
`fg158_fail` with code 103, so there is no return value to assert on. The gate runs that
case under `FG_STATE_SLOT_OVERRANGE` and checks that the process reports code 103 - the
refusal is observed as a refusal, not as a value.

Recording this because the first version of the gate tried to check the refusal by reading
a return value and simply crashed.

## 5. State ledger after this

| # | unit | status |
|---|---|---|
| 1 | attention KV slot | DONE (EXP-701) |
| 2 | attention KV write/keep/read | DONE (EXP-701) |
| 3 | recurrent conv state | DONE (EXP-692) |
| 4 | SSM state as an output | DONE (EXP-696) |
| 5 | one mechanism for both caches | **DONE (this card)** |
| 6 | buffer survives a graph rebuild | OPEN |
| 7 | reuse from n_past | OPEN |
| 8 | comparison vs full recompute | DONE for KV and conv |

Five of eight, and the two remaining are structure, not arithmetic.

## 6. Cost

One arch-unit build (1081 symbols) for the new export, one gate compile.
