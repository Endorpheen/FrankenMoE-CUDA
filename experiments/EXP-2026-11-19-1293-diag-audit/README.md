# EXP-2026-11-19-1293: every diagnostic added this session is behind a flag

The sampler file gained several prints while chasing the DRY and XTC defects. Each one is now checked, because a
diagnostic that costs a call on the shipped path is the same mistake as a counter that never prints its total.

| print | gate | default |
|---|---|---|
| `FG_CHAIN_ORDER` | `GGML_GOOSE_CHAIN_ORDER` set | off |
| `FG_DIST_IN` | `FG_SAMPLER_DIAG=1` | off |
| `FG_ADAPTIVE_FLAG`, `FG_ADAPTIVE_ENTRY`, `FG_ADAPTIVE_IN`, `FG_ADAPTIVE` | `FG_SAMPLER_DIAG=1` | off |
| `FG_ADAPTIVE_ACCEPT` | `FG_SAMPLER_DIAG=1` | off |

`fg_diag_enabled()` reads the environment **once** into a `static const bool`, so the shipped path pays one
predictable branch per sampler application and nothing else: no formatting, no array walks, no allocation. The
array hashes and dumps sit inside the `if`, not beside it.

Two measurements back this up: the acceptance run keeps producing `c2ff42cc1219` at 44.6-46.2 tok/s decode, the same
range as before the prints existed, and the diagnostics appear only when their flag is set - every log quoted in
this session's cards came from a run where it was.

## The audit itself was wrong first

My first pass flagged four prints as ungated. It used a three-line context window, which misses an `if` that wraps a
multi-line block. Reading the four sites showed all of them inside their gate. The check that would be worth keeping
is a proper syntax-aware one; a line-window grep is not it, and pretending otherwise is how a green audit ends up
meaning nothing.
