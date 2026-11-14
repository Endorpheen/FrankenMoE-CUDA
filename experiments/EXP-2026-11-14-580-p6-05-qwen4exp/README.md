# EXP-2026-11-14-580 — qwen4exp builder (P6-05 architecture 2)

Native counterpart: `src/models/qwen4exp.cpp` (Qwen3.8 family: gated delta net,
indexer/QSA, hyper-connections, PLE, MTP head). Base commit: `96e2fb91`.

## What this slice covers

`source/qwen4exp.goose` is the plan/shape half of the builder: the per-layer
kind decision (attention vs gated delta net on the hybrid interval, the
recurrent flag), the GDN geometry (`wqkv`, key/value widths from
`ssm_n_group`), the fused QKV and value widths, the indexer `k_proj`/`q_proj`
widths, hyper-connection head norm and per-layer down shapes, routed-expert
width with `n_expert_used`, the MTP `eh_proj` doubled hidden state, and the
per-layer embedding marker.

`source/base/model_graph.goose` is the accepted model-graph slice without its
`fn main()` (the canonical file is `goose/model/goose-model-graph.goose`; the
only difference is the 35-line entry point).

## Gate

`tests/build.sh` builds both modules with `--standalone` at `-O1` and runs ten
structure cases:

```
FG579_ARCH2 cases=10 values=26 failures=0
```

The gate checks the plan and shapes only. It does not link a native library:
the expectations are a transcription of `qwen4exp.cpp`, so it proves the
decisions and shapes, not native numeric execution.

## Mutations

`tests/mutate.py 0 6|6 12` holds 12 semantic mutations in two halves; each runs
in its own temporary tree at `-O0` while the gate stays at `-O1`.

```
python3 tests/mutate.py 0 6    # 6/6 caught
python3 tests/mutate.py 6 12   # 6/6 caught
```

Mutations cover the hybrid interval, the recurrent flag, fused QKV and wqkv
widths, indexer projections, transposed embeddings, HC multiplicity, per-layer
HC down, routed-expert width, MTP input width, and the recurrent key/value
widths.

## Recorded limits

- `q4_plan`, `q4_is_ple`, the PLE tensors, `indexer.k_proj`, shared experts and
  `nextn.*norm` are not covered by the ten cases.
- The `is_recr() == 1` branch has no positive case: the fixture passes zero
  flags, so only the negative direction is exercised.
- No native library is linked; the oracle is a transcription, and the gate is
  structural.
