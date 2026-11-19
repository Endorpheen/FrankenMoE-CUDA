# EXP-2026-11-19-1279: REJECTED hypothesis - the chain's ops are not the doors

**Hypothesis:** `fg_sampler_chain(op, ...)` reaches the same function as the corresponding
`fg_sampler_*_call` door, so the dispatch table can be validated by equivalence.

**Result:** rejected. Routing op 1 through the chain and comparing with the top-p door gives **898
mismatches out of 900**:

```
TOP-P MISMATCH tp=0.5 mk=0 goose=26 native=2
```

The chain's op 1 is `sm_apply_top_p(lv, p, n, f, k)`: it takes **no id array** and filters logits in place,
whereas the top-p door also permutes ids and takes `min_keep` in a different position. They implement the same
decision with different contracts, which is why the chain's own return value (26) reads as "nothing filtered"
against the native count (2).

## What this cost and what it taught

The first reconstruction routed the restored harness through the chain and produced the same 898 mismatches
inside an **accepted** measurement - which would have read as a regression in the port. The harness had in fact
measured the doors. Two lessons, both already in this session's record in other forms:

- an equivalence I assumed between two functions was never written down anywhere, and it was false;
- a red number in a restored harness first accuses the reconstruction, not the code.

## Where the check went instead

`EXP-2026-11-18-1188-shipping-doors/wrap_now.c` maps the dispatcher to the doors the harness measured, and the
accepted numbers reproduce exactly: typical-p 1800/1800, top-p 900/900, top-p large 45/45, XTC 720/720, DRY step 1
200/200. The chain's internal op table stays unverified by equivalence and needs the real sampler chain, which
needs a model.
