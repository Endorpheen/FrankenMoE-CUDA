# P6-11: speculative decoding - the doors that exist and the four that are called

Recorded 2026-11-19 with `tools/door-callers.py`, after five ledger records had to be corrected for
counts taken from too narrow a search.

## The two modules

| module | exports | how it is reached |
|---|---|---|
| `goose/speculative/spec-decisions.goose` | **none** | its functions are private; `spec-doors.goose` forwards to them |
| `goose/speculative/spec-doors.goose` | 6 doors | called from the host, see below |

So the earlier phrasing "eleven decisions not yet wired" described something that cannot exist: there
are no exported decisions to wire. What can be wired, and partly is, are the doors.

## The six doors

| door | call site |
|---|---|
| `fg_sp_prefix` | `vendor/flint/common/sampling.cpp` |
| `fg_sp_n_max_call` | `vendor/flint/common/speculative.cpp` |
| `fg_sp_synth_accept` | `runtime/reconstructed/server-merge/server-context.cpp`, `vendor/flint/tools/server/server-context.cpp` |
| `fg_sp_reject_accept` | `vendor/flint/common/sampling.cpp` |
| `fg_sp_compatible` | **none** |
| `fg_sp_ngram_accept` | **none** |

## What is actually open

Not wiring, and not the decisions: an **MTP/draft-model run**. It needs a draft GGUF whose vocabulary
matches the target, then the shared/separate draft paths, MTP borrowing, accept/reject/rewind, and the
per-position `n_accepted_per_pos` statistics. No speculative decoding has been run on this project's
hardware at any point, and no measurement of it exists.

The requirement stays PARTIAL for that reason, and the two uncalled doors are named rather than
summarised as "the rest".
