# EXP-2026-11-14-578 — session loop budget and cancellation (P9-06 S5)

Native counterpart: `runtime/core/src/engine/session.cpp` — the abort callback
installed at 947-957 and the budget, cancellation and context decisions of
`Session::generate()` at 1286-1288, 1309-1314, 1400-1402, 1553-1565,
1658-1665, 1677-1707, 1752-1762, 1848, 1894-1911, 1925-1926 and 2011-2016.
Base commit: `dee2b8f7`. Experiment directory for the isolation rule: this run
was performed outside the repository, in `/tmp/opencode/sub-608/`.

## What this slice covers

`source/session-loop.goose` (240 lines, prefix `sl_`) is the loop's policy with
no model behind it. Every function names the native lines it reproduces.

| export | native | decision |
|---|---|---|
| `sl_abort` | 947-957 | the one predicate `llama.cpp` polls at a node boundary: `cancel_requested \|\| source.fatal() \|\| hook->fatal()` |
| `sl_loop_step` | 1658-1665 | continue, stop on the budget, stop on the captured replay length, stop on end-of-generation — in the native order |
| `sl_ctx_ok` | 1400-1402 | `n_prompt + n_predict + 8 > n_ctx` rejects the request; equality passes |
| `sl_replay_ok` | 1309-1314 | a replay needs `n_predict >= token_count`; a longer budget is accepted |
| `sl_draft_room` | 1677 | `n_predict - n_gen - 1`, one slot is reserved for the token already sampled |
| `sl_draft_width` | 1678-1707 | drafting needs speculation on and `room > 0`; the width is `min(draft_max, room)` |
| `sl_emit_count` | 1848 | one verify decode confirms a group, but the caller's budget still bounds what is emitted |
| `sl_step_verdict` | 1553-1565, 1752-1762 | a failed decode classifies as cancel (prefill or decode), workload, hook, overlap source, generic — in that order |
| `sl_result` | 2011-2016, 2048 | cancelled is `ok=true, cancelled=true`, except with a deterministic workload, where the run fails closed |
| `sl_cancel_effective` | 1286-1288 | the latch is refilled per generation, so a stale `cancel()` is dropped |
| `sl_n_prompt_reported` | 1925 | `n_prompt - n_common`: only the un-cached suffix counts as prompted |
| `sl_n_past` | 1926 | the engine-held token list in chat mode, `n_prompt + n_gen` otherwise |
| `sl_trim_end` | 1894-1911 | the KV is trimmed to the emitted prefix, and only under speculation |

The order inside `sl_loop_step` and `sl_step_verdict` is part of the port: the
budget arm is tested before the replay stop (so `n_gen == n_predict ==
token_count` reports a budget stop), and cancellation is tested before every
error (a cancelled decode returns non-zero through the abort callback, and
reporting that as a graph failure would lose the fact that a person asked).

## Owned decisions

- Numbers, not texts: the native error strings stay in the carrier. The port
  returns classifications (`SL_FAIL_*`, `SL_STOP_*`, `SL_RES_*`).
- The null-hook arm of the abort predicate is not carried. `open()` always
  builds the hook (803), so `p->hook &&` is dead in this engine.
- `sl_cancel_effective` takes the entry value and discards it, then follows
  the native store at 1288. Naming the parameter keeps the decision visible: a
  request made outside a generation cannot stop the next one.
- `sl_trim_end` returns `-1` when speculation is off. The native trim block is
  guarded by `if (spec_on)`, so "no trim point" is a real answer, not an
  omission.
- `sl_emit_count` clamps a negative remainder to zero. The native `for`
  condition already cannot run with `n_gen >= n_predict`; the clamp states the
  same thing for a caller that asks directly.

## Gate

`tests/build.sh` builds the single module with `goose --standalone`, compiles at
`-O1`, links `tests/session_loop_fixture.cpp` and runs it. The fixture holds a
transcription of each native decision, one function per decision with the
native lines quoted above it, plus a table of 31 scenarios. Every scenario
asserts all thirteen decisions, so a wrong order or a wrong boundary fails in
many places. A second table pins 43 boundary values by hand, independent of the
transcription, so a transcription that quietly disagrees with the source cannot
hide behind a port that agrees with it.

```
FG577_S5 cases=74 values=446 failures=0
```

74 cases = 31 scenarios + 43 pinned values; 446 values = 31 x 13 + 43 checks.
The same fixture also passes against a `-O0` build of the module.

## Mutations

`tests/mutate.py 1|2` holds 6 + 6 mutations; each rewrites one decision,
rebuilds at `-O0` and reruns the same fixture. Builds are serial on purpose.

```
python3 tests/mutate.py 1   # 6/6 caught
python3 tests/mutate.py 2   # 6/6 caught
```

Half 1 (bounds and loop order): exclusive `n_ctx` bound; the 8-token margin
dropped; a replay one token short accepted; the budget arm missing
`n_gen == n_predict`; the replay stop one token late; end-of-generation tested
before the replay stop. Half 2 (predicates and results): the abort predicate
requiring every cause at once; a workload mismatch reported before the cancel;
the prefill and decode cancels swapped; the drafted slot not carved out of the
budget; the draft width taking the wider bound; a cancelled workload accepted as
a finished run.

`sl_emit_count`, `sl_n_prompt_reported`, `sl_n_past` and `sl_cancel_effective`
are covered by scenarios and pinned values but have no dedicated mutation.

## Recorded limits

- The graph, the tokenizer, the sampler, the decode and the KV are native and
  untested here. The gate checks the decisions around them, never a generation.
- No model is loaded and no tok/s number exists for this slice by design.
- Not ported: the KV prefix-reuse search (`n_common`), the chat rollback
  (`rollback_turn`), the prompt/tokenizer checks before 1400, the per-token
  metrics and the latency accounting (1588-1932), and the capture/replay file
  format. Those belong to other P9-06 slices or to the carrier.
- The abort callback is the only cancellation path in this engine: a decode
  that is not interrupted by `llama.cpp` cannot be stopped between tokens,
  because the loop has no cancel test of its own. That is the native behaviour,
  not a port choice.
- The replay floor compares against `token_count`, not against the route record
  count; a workload whose records disagree with its token count is caught by
  `finish()` at the end of the run, which stays native.
