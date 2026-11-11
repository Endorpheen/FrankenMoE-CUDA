# EXP391: fused-op resolution decisions are live (APEX.CORE)

## Verdict

**LIVE on the reconstructed tree** (context creation).

## Transferred

| op | rule | native site |
|---|---|---|
| 0 | a fused op is probed only when it is enabled and auto-resolution was requested | `llama_context::resolve_fused_ops` |
| 1 | the probe graph is reserved for `n_tokens_per_seq * n_seqs` | same |
| 2 | a device mismatch disables the op, otherwise it is confirmed | same |

## Gate

| check | result |
|---|---|
| 43 draws over enabled × auto, widths 0..5 × 0..5, both verdicts | 43 checks / 0 failures |
| mutations | **5/5 caught** |

## Run

| check | result |
|---|---|
| completion | `e68df05b…`, equal |
| restore | succeeds |
| `FG391S_FUSED_OPS_RESOLVE` | **live** (first-call line at context creation) |
| `FG389S` / `FG386S` / `FG345W` / `FG241U` | live |

## Process note

The first integration attempt dropped the verdict branch because my script's own escaping did not match
the file, and the staging rebuild aborted half-way — the next run then executed without the lost-bridge
disable and died with `FG205G validation code 21`. That is the correct failure mode: a half-applied
staging is visible immediately, not silently accepted. The verdict branch was then wired with a single
line replacement and the rebuild is clean.
