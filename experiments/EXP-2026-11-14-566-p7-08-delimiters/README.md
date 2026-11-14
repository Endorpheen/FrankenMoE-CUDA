# EXP566: token-aligned message delimiters (P7-08 slice 10)

## Hypothesis

`common_chat_msg_delimiters` turns assistant/user markers into token spans used by
the server to split a prompt batch and choose KV checkpoints. It is a pure
function of the markers, their token ids and the stream tokens, so it can be
ported and gated without a model.

## What the native code does

- `add` appends a role and marker text; empty markers are skipped by the caller.
- `tokenize` calls `common_tokenize(vocab, marker, add_special=false,
  parse_special=true)` once per marker.
- `split` walks the token stream: a skip region (media) is jumped over, the first
  marker in insertion order wins at a position, content before the first match is
  dropped, and a span ends where the next marker starts. The last span runs to the
  end of the stream. `is_user_start` is true only at an exact span start;
  `last_user_message_pos` returns the last user span's position.

## Port

`build_msg_delims`/`dl_add`, `dl_bind_tokens`/`dl_bind_all`, `dl_split`,
`sp_is_user_start`, `sp_last_user_pos`, `role_from_string`/`role_to_string` plus
the incremental `dl_message_start_at`/`dl_user_message_start_at` probe. Flat
arenas, no external C symbol: the caller tokenizes and passes ids.

## Gate

```
FG571_DELIM cases=36 values=1452 failures=0
mutations 6/6 caught (range 0:6)
mutations 6/6 caught (range 6:12)
baseline: rc=0 cases=36 values=1452 failures=0
```

36 cases: 10 fixed marker tables, 10 real templates read through
`autoparser::analyze_template`, 8 edge cases (empty delimiter, shared prefix,
marker at start/end, zero skip as a native no-op jump), 7 native split-corpus
cases and one contract case (negative skip, unsorted/duplicate skip).

## Deferred

The `server_tokens` linkage group is not in the gate: `server_tokens::get_tokens()`
aborts on an empty stream and needs a constructed server session. That is a
follow-up slice, recorded rather than approximated.

## Integration

Append the block to `goose/jinja/runtime.goose`; no existing function changes.
Exports: 54 canonical + 10 delimiters + 2 incremental probes.
