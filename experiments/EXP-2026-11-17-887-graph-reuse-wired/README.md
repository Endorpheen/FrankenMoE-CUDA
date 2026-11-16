# EXP-2026-11-17-887 — the first graph-reuse call site is wired, and both paths give the reference answer

EXP-886 proved `goose/cpu/graph-reuse.goose` equal to the native conjunction over 4000 randomised cases
without touching a call site. This wires the first one and checks the two paths against each other.

## What moved

`llm_graph_input_embd::can_reuse` - five conditions, the smallest member of the family - now asks
`fg_gr_check` with three rows per field:

| field | must-match | wanted | actual |
|---|---|---|---|
| tokens | batch supplies tokens | `n_tokens` | `tokens->ne[0]` |
| embd | batch supplies embd | `n_tokens` | `embd->ne[1]` |
| mixed_tokens | tensor exists | `llm_graph_n_tok_rows()` | `mixed_tokens->ne[0]` |
| mixed_embd | tensor exists | `n_tokens` | `mixed_embd->ne[1]` |
| scale_rows | tensor exists | `n_tokens` | `scale_rows->ne[1]` |

The one non-obvious translation is the native `tokens && tokens->ne[0] == n_tokens` guard: a required row whose
tensor is missing would compare `n_tokens` against nothing. It carries `INT64_MIN` as its extent instead, which
cannot equal a real extent, so the row fails exactly as the native guard did. Without that, a null tensor would
have compared equal in the degenerate case where the wanted extent happened to be negative.

The native body is kept behind `GGML_GOOSE_GRAPH_REUSE=0`.

## Verified, both directions

```
512 tokens, module ON :  sha c2ff42cc1219   decode 39.59 (37.60-39.71)
128 tokens, module OFF:  sha 9d90a010775f   (= reference for 128)
```

Both match the reference hashes. The flag-off run is the rollback path and it produces the recorded answer, so
the switch is a real rollback and not a second code path that merely looks similar.

`can_reuse` sits on the request path, so the hash agreeing is the meaningful signal: a wrong reuse decision
would either rebuild a graph it should have kept (slow, same answer) or keep one it should have rebuilt (wrong
answer). The answer did not move.

## Scope

One call site of 21. The other 20 have different field lists and were not touched. This is the pattern to
repeat for them: rows as data, native body behind the switch, hash checked on both sides.

The compiled objects are not committed - `fg-gr.o` is generated from the Goose file by
`runtime/reconstructed/build.py`, and the bridge source is in `integration/m1/common/fg-graph-reuse-bridge.c`.
