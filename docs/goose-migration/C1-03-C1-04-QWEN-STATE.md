# C1-03 and C1-04: what Qwen-specific work exists, and what the native tree does not have

Checked 2026-11-19 on `main`, text level, no build.

## Present

| piece | state |
|---|---|
| `qwen4exp.goose` | plan and shape for 52 tensor ids, per layer kind (accepted P6-05) |
| `q4m-metadata.goose` | 19 exports: the 27 GGUF keys, their requirement and arity classes, PLE/HC/indexer/SSM guards, recurrent pattern, `n_embd_out`, model type |
| `q4t-tensor-map.goose` | 10 exports: id to name pattern, layer arity, suffix, bucket, loader flags, skip-if-virtual, per-kind presence |
| `q4e-expert-layout.goose` | 15 exports: routed/shared expert widths and shapes, fused-vs-split probes, attention QKV widths, head-width assert |

All four are in the composed unit and the unit compiles (CI run 44597680081, Goose job success).

## Absent

- **No Qwen4 loader module.** `goose/model/` has `qwen35-model-loader.goose` for the other family;
  the Qwen4 family has its metadata rules (`q4m`) and its tensor map (`q4t`), but nothing that joins
  them to a load path.
- **No Qwen-specific tokenizer wiring.** The family's vocabulary would go through the same
  `llama_goose_tokenize` path P7-06 describes; nothing Qwen-specific exists or is needed there, and
  nothing is claimed.
- **No cold-bank anywhere**, in Goose or in the native model: `vendor/flint/src/models/qwen4exp.cpp`
  contains no reference to a cold bank. C1-03 lists "реализовать отличия cold-bank" - in this tree's
  Apex-era qwen4exp there is no such feature to differ from, so a Goose implementation cannot
  correspond to it. That is a finding to record, not a gap to fill by invention.

## What C1-03 asks for, against what exists

| item | state |
|---|---|
| Qwen precision paths | no Qwen-specific quantiser in Goose; the shared `quant-*` modules are the ones in the unit |
| cold-bank differences | no cold-bank in the native model either - nothing to port |
| Qwen expert hooks | the expert layout is ported (`q4e`), the hooks are not wired to a caller |
| reuse of shared quant/math/controller modules | the `q4e` gate proves the expert widths come from `base.model_graph`, so the family baseline is shared |

## What closing them needs

1. a loader adapter for the family: join `q4m`'s validated keys and `q4t`'s map to the loader path,
   the way `q4t` already publishes loader flags;
2. a real Qwen GGUF, which C1-04 and C1-05 both need and neither can substitute for;
3. a recorded decision about each requirement item that the native model does not implement, so the
   ledger does not carry work that has nothing to port.

No Qwen model has been loaded, and no measurement of this family exists.
