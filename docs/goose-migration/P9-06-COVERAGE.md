# P9-06 coverage — chat/thinking controls and the standalone CLI

Updated: 2026-11-14. Native sources: `runtime/core/src/engine/chat_parse.cpp`
(23 lines), `runtime/core/src/engine/thinking_control.cpp` (121) and
`runtime/cli/main.cpp` (1148) — 1292 lines total.

The requirement is split into four independent slices. S2 is accepted; the rest
is open.

| slice | native scope | state |
|---|---|---|
| S1. Chat parse and rendering | `chat_parse.cpp`, the chat-path helpers around it | **done (EXP575)** — `build_parse_params` and `warn_parse_failed_once` |
| S2. CLI line protocol | `main.cpp` stdin reader, the six stdout emitters, the delta rule, recoverability | **done (EXP572)** |
| S3. Thinking control | `ThinkControl` Template/Prefill/None, `add_no_think_prefill`, `probe_think_control` | open (port written, not accepted) |
| S4. Serialized parser load | `build_parse_params`, `common_peg_arena::load` from JSON | **done (EXP575)** |

## S2 — accepted

Source: `experiments/EXP-2026-11-14-572-p9-06-cli-protocol/`; canonical files
`goose/cli/cli-protocol.goose` and `goose/cli/jsonparse.goose` (byte-identical
to `goose/json/json-parser.goose`, EXP493, kept as the module dependency).

The slice reads one stdin line into a flat command (kind, id, n_predict, think,
clear_kv, prompt bytes), emits `BMOE_READY`, `BMOE_BEGIN`, `BMOE_LOAD`,
`BMOE_PROGRESS`, `BMOE_DONE` and `BMOE_ERROR`, and implements the per-token
delta rule: a suffix when both tracked fields extend the snapshot, otherwise the
full text with `"reset":1`. Native defaults are preserved (`clear_kv` true,
`think`/`n_predict` from the process flags, `id` 0), a line with no usable `cmd`
is skipped, and only "empty prompt" and "exceeds the session n_ctx" are
recoverable.

Gate: 17 cases / 26 values, 0 failures. The oracle format strings are the
carrier's own, printed with `snprintf`, so the comparison is libc printf against
the Goose emitter. Mutations: 15/15 in two halves (7 + 8).

## Open boundaries

- The 63 long CLI flags, engine configuration and memory/streaming options
  belong to P9-01…P9-05/P9-07, not to this requirement.
- S3 and S4 ports exist but have no accepted gate; they need a native oracle
  and mutations before joining the canon.
- `chat_parse.cpp` (S1) is still native.
- The session protocol is gated without a model: tokenization, sampling and the
  engine loop are out of scope.
