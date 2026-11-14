# EXP567: streaming session, diffs and monotonic publication (P7-08 slice 11)

## Hypothesis

`common_chat_session` re-parses the whole accumulated text on every chunk, keeps
the last good message, and publishes only when the parsed candidate is a monotone
extension of the previous one. The session layer is a pure function of the
accumulated facts, so it can be ported and gated without a model.

## What the native session does

- `feed(chunk)` appends the chunk to the accumulated input, parses the whole text
  leniently and maps it; on a parse failure with `result.end > 0` the candidate is
  empty and the previous message survives; on `result.end == 0` the state is
  marked as failed and repeated calls keep failing without finishing.
- `finish(chunk)` appends, parses non-leniently and marks the session finished
  before parsing; a bad tail is a protocol error.
- `compute_diffs` accepts a candidate only when reasoning and content grow as
  suffixes, the tool-call count does not shrink, the last previous call's name is
  a prefix and new calls are appended; otherwise it throws.

## Port

`chat_stream_open`, `chat_stream_step`, `chat_msg_diff`, `chat_msg_monotone`,
`chat_stream_error_name`. The step validates the previous message table against
its buffers before reading anything (-5), publishes only monotone candidates
(-6 with the diff code in the state), and keeps last-error state separate from
the per-step status. Caller example in `examples/stream_consumer.c`.

## Gate

```
FG568_STREAM   cases=10 steps=47 values=271 failures=0
FG568_DIFF     cases=10 values=106 failures=0
FG568_MONO     cases=10 steps=21 values=273 failures=0
FG568_CONTRACT cases=13 steps=15 values=61 failures=0
FG568_CALLER   cases=12 values=12 failures=0
```

Covered: cuts inside a tag, inside a quote, between `\` and `'` (a real
non-monotone pair), growth of JSON arguments, shrinking call count, name change,
empty candidate, garbage tail, state corruption, every status code.

## Recorded limitations

- The parser stays native: the gate feeds the step the native parse facts. The
  session/publication layer is what is gated.
- The mapper is called through the export ABI (`extern "peg_map_msg"`), because
  Goose does not allow calling an exported function with const slices internally.
- Freezing on a non-monotone tail is native behaviour, not a defect.
- The caller must keep the generation prompt in the buffer and must not treat a
  publish after -6 as a successful finish; `docs/stream-contract.md` records the
  contract.
