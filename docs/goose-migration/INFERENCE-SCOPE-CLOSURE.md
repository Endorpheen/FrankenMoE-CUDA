# Inference scope closure: P4-04 and P6-06

Both requirements were written for the full upstream surface. The product is
the FrankenMoE inference runtime; training and non-Apex architectures are
explicit compatibility work. This note records the scope decision so the two
items are not held open by code the product does not run.

## P4-04 -- graph construction, hashing, traversal and differentiation

Criterion: "All graph expansion/copy/export/gradient/optimizer entrypoints
mapped; cycles, shared views, empty graphs and malformed dimensions tested."

Inference evidence (EXP159/EXP160):

| clause | evidence |
|---|---|
| graph expansion/copy | EXP159: iterative forward traversal, order/select, copy/dup/borrowed views, clear/lookups |
| hashing/bitset policy | EXP159: core hash/bitset policy, checked graph layout |
| cycles, shared views, empty graphs | EXP159: 93 393 candidate / 92 907 rollback checks, 12 000-depth case |
| malformed dimensions / cleanup | EXP159: 9 error-cleanup cases, 256 threaded independent graph lifecycles |
| fusion/params/hash | EXP160: 714 native predicate comparisons, 1 655 checks, 15 malformed inputs |
| Apex | EXP159/EXP160: exact text/all 128 IDs/full probabilities, 127 graph reuses, zero graph heaps |

The requirement's training half -- gradients, autodiff, optimizers and training
export -- is not part of the inference runtime. It is retired from this
requirement by owner scope; if training is ever added, it needs its own
requirements and gates. This is recorded, not silently dropped.

## P6-06 -- remaining recurrent and hybrid model builders

Criterion: "Every remaining recurrent/hybrid text builder in the kept set
mapped with a tiny fixture. Multimodal encoders, projectors and audio builders
are out of scope by owner directive."

Kept set for the migration: the Apex-35B hybrid architecture actually run by
the product -- attention layers plus the recurrent/SSM hybrid path. Evidence:

- builders enumerated from the loader's own tensor list;
- tiny hybrid fixture green: 5 checks / 0 failures, per-layer
  `cache_r_l*`/`conv` attribution;
- two synthetic mutants were rejected by both native and Goose at allocation,
  which is a generator limitation, not a port divergence (EXP-342);
- the real Apex hybrid model loads, serves and matches the accepted output.

RWKV/Mamba-style builders, other model families and multimodal/audio
encoders/projectors are compatibility work for `C2-*`/`P10-10`, not the kept
set. They remain visible in the ledger and are not counted as tested.

## Verdict

P4-04 and P6-06 close on the inference/Apex scope. The retired halves are
named, with their future home, rather than removed from history.
