# LINK — delimiters + streaming linkage (P7-08 / P6-09 boundary)

Date: 2026-11-14
Base: commit 6c217e30. `goose/jinja/runtime.goose` at that commit is identical
to 3c88b136 (blob SHA-256
`3be19f7fe73250d3ee79f3caca444d7f75659c00d9f6067c402d855f47e736e3`,
12026 lines), so the delimiters block already in `source/runtime.goose` is
still on the current canon. No extra rebase was needed for this round.
Mode: no build; `goose check` only.

## 1. Contract: how the server uses the span queries

The server does not call these queries during SSE generation. It calls them
while filling the prompt batch, to place KV-cache checkpoints:

- `server_task::apply_chat_session` computes the spans once per task:
  `params.message_spans = tokens.find_message_spans(session.message_delimiters())`
  (`tools/server/server-task.h:292`).
- `server_tokens::find_message_spans` builds the skip map from
  `map_idx_to_media` (placeholder start -> chunk token count) and calls
  `common_chat_msg_delimiters::split(tokens, skips)`
  (`tools/server/server-common.cpp:866-872`).
- During batch fill, `spans.last_user_message_pos()` is read once
  (`server-context.cpp:4326`) and `spans.is_user_start(pos)` is probed:
  - at `slot.prompt.n_tokens()` to break the batch at the last user message or
    at a user message far enough past the last checkpoint
    (`server-context.cpp:4370-4374`);
  - at `n_tokens_start` to decide whether the batch starts a user message and
    whether mid-prompt checkpointing may continue
    (`server-context.cpp:4408-4409,4427,4447`).

Semantics used by the server:

- positions are indices in the prompt token array, not bytes;
- `is_user_start(p)` is true only when a USER span starts exactly at `p`
  (`common/chat.h:182-189`);
- `last_user_message_pos()` is the last USER span start, or -1
  (`common/chat.h:191-198`);
- spans cover `[delimiter start, next delimiter start)`, so the marker tokens
  themselves belong to the span that starts with them (`chat.cpp:144-150`);
- skips make a media chunk opaque: a delimiter sequence inside it never
  matches, and the chunk stays inside the preceding span
  (`chat.cpp:127-140`, `tests/test-chat.cpp:1750-1791`).

For a future incremental path the caller has the same store, the same token
ids, the same skip table, and a cursor: the first token not yet consumed. It
must probe at every token position, because a delimiter is positional. A
marker whose start was skipped is lost; a marker that starts before the cursor
and overlaps it is not a new message.

## 2. Draft functions

Added at the end of the delimiters block in `source/runtime.goose`
(3 functions: 2 exported, 1 private):

```
fn dl_message_start_at_impl(D, T, tokens, sk_start, sk_len, n_skips, p) -> i64
export fn dl_message_start_at(... same ...) -> i64
export fn dl_user_message_start_at(... same ...) -> i64
```

`D`/`T` are the delimiter store from `dl_bind_*` (role, byte range, token
range), `tokens` is the prompt id array, the skip arrays are the media chunks.

`dl_message_start_at`:

- returns the role of the first delimiter in insertion order matching exactly
  at `p`, or -1 when nothing starts there;
- returns -1 when `p` is outside `[0, tokens.len)`;
- returns -1 when `p` is at the start of, or inside, a positive-length skip;
  a zero-length skip is the native no-op jump, so a match at `p` is allowed
  (`dl_split` does `i += 0; ++skip;` and tests `p` again);
- requires skips sorted, unique, non-overlapping and non-negative; malformed
  arrays return -1 (the server's media chunks are disjoint by construction).

`dl_user_message_start_at` returns 1 for a USER start, 0 for another role or
no start, -1 for the contract errors. This is the one-position equivalent of
`sp_is_user_start`.

Unlike `dl_split`, these functions allocate nothing, do not scan the rest of
the prompt, and do not modify the caller's buffers.

## 3. Cases (12)

All evidence lines are the flint tree. Role numbers: 0 unknown, 2 assistant,
3 user. `NULL` is `LLAMA_TOKEN_NULL` (-1).

| # | Store | Tokens | Skips | p | Expected | Evidence |
| --- | --- | --- | --- | --- | --- | --- |
| 1 | USER=[10,11] | 10,11,7 | - | 0 | USER (3) | match at the first token, `chat.cpp:132-140` |
| 2 | USER=[10,11] | 7,8,10,11,9 | - | 2 | USER (3) | marker in the middle, same lines |
| 3 | USER=[10,11] | 7,8,10,11 | - | 2 | USER (3) | marker at the end, guard `p+cnt <= len` (`chat.cpp:133-135`) |
| 4 | USER=[10,11] | 10,11,7 | - | 1 | -1 | inside a marker, no new start; overlap is not a start |
| 5 | USER=[10,11] | 10,11,7 | - | 3 | -1 | `p == len` is out of range |
| 6 | USER=[10], ASSISTANT=[10,11] | 10,11,20 | - | 0 | USER (3) | first delimiter wins, not the longest (`chat.cpp:132-141`) |
| 7 | ASSISTANT=[10,11], USER=[10] | 10,11,20 | - | 0 | ASSISTANT (2) | same position, insertion order decides |
| 8 | USER=[10,11], ASSISTANT=[10,12] | 10,11,100,10,12,200,10,11 | - | 0,3,6 | USER, ASSISTANT, USER | several markers in one stream |
| 9 | ASSISTANT=[], USER=[10,11] | 5,6 | - | 0,1 | ASSISTANT (2) | an empty token list matches every position (`std::equal` over an empty range) |
| 10 | USER=[10,11], ASSISTANT=[10,12] | 10,11,NULL,NULL,10,12 | {2:2} | 2,3,4 | -1,-1,ASSISTANT | skip start and inside are opaque; p=4 matches again (`chat.cpp:127-140`) |
| 11 | USER=[10,11] | 5,10,11 | {1:0} | 1 | USER (3) | zero-length skip is a no-op jump, matching is allowed |
| 12 | USER=[10,11] | 10,11,7 | {5:1,2:1} or {1:-1} or {1:3,2:1} | any | -1 | unsorted, negative, or overlapping skips violate the contract |

Case 4 and case 8 together define the cursor rule: probing only at marker
starts finds all markers; probing after a start misses that marker.

## 3b. Fixture coverage: real find_message_spans (12 sequences)

`tests/fixture.cpp` now has a linkage group that calls the real
`server_tokens::find_message_spans` from `libllama-server-impl.so`, not a local
copy of its body. Each case builds the delimiter store twice (native
`common_chat_msg_delimiters`, port `dl_add` + `dl_bind_tokens`), then compares
`dl_split` with the native spans and the two server queries at every position,
exactly like the earlier delimiter cases.

Modes:

- synthetic token ids (cases 1-8): edge shapes, shared prefixes, empty stream;
- vocab-only GGUF (cases 9-10): markers tokenized with
  `common_tokenize(vocab, marker, false, true)` from
  `models/ggml-vocab-qwen35.gguf`, assembled into a ChatML conversation;
- media (cases 11-12): a real `server_tokens` built from
  `mtmd_test_create_input_chunks()` (a text chunk and an image chunk), so
  `find_message_spans` computes the skip map itself from the image chunk. Text
  tokens are appended after the chunk to model a message after the media.

| # | Sequence | Mode | Checks |
| --- | --- | --- | --- |
| 1 | `l01_empty_stream` | synthetic | zero tokens, one delimiter, zero spans, last user -1 |
| 2 | `l02_single_marker` | synthetic | one marker and nothing else |
| 3 | `l03_marker_at_start` | synthetic | marker at token 0 |
| 4 | `l04_marker_in_middle` | synthetic | marker between content tokens |
| 5 | `l05_marker_at_end` | synthetic | marker ends at the stream end |
| 6 | `l06_two_markers` | synthetic | user + assistant with a shared first token |
| 7 | `l07_three_markers` | synthetic | user/assistant/user span table |
| 8 | `l08_shared_prefix_order` | synthetic | insertion order, not longest match |
| 9 | `l09_vocab_single` | vocab | real ChatML user marker in the middle |
| 10 | `l10_vocab_three` | vocab | real ChatML user/assistant/user stream |
| 11 | `l11_media_between_messages` | media | text marker, image chunk, marker appended after it |
| 12 | `l12_media_hidden` | media | a delimiter starting at the skip start, a NULL-only delimiter inside the chunk, a delimiter crossing from text into the chunk, and a marker after the chunk |

Expected: 12 new cases, `FG571_DELIM cases=48 values=<N> failures=0` in total
(36 previous + 12). The media cases fail with a clear message if the mtmd test
chunks are unavailable.

Build changes: the fixture includes `tools/server/server-common.h` and links
`-lllama-server-impl -lmtmd` with `-I$L/tools/server -I$L/tools/mtmd`. The
Goose source is unchanged from the probe round.

## 4. What remains

- Wire the probe into a streaming session state: cursor, pending tokens,
  rewind on sampling retraction, and skip updates when a media chunk is
  tokenized. The current server path still precomputes `dl_split` spans.
- The `dl_split` side of the gate is now covered by the 12 cases above. Still
  open: compare the incremental probe itself with `sp_is_user_start` at every
  start position (expected `dl_message_start_at(p) == span role` and the USER
  wrapper == `sp_is_user_start(p)`), and wire it into a streaming session.
- Rebase on the current canon and re-run the delimiters gate before merge, per
  the base-SHA rule.
- The fixture and build script are extended for the link group; the Goose
  source is unchanged from the probe round.

## 4b. Integration package

Base SHA: 6c217e30. The canon `goose/jinja/runtime.goose` at that SHA is
byte-identical to 3c88b136, so the existing delimiter block is the accepted
structure; this round only appends the two exported probe functions and the
private impl. Nothing outside `source/runtime.goose`, `tests/fixture.cpp`,
`tests/build.sh` and this document is touched; the native server files and the
canon body are unchanged.

Interfaces the module relies on (all already in the base commit):

- the flat delimiter store `D`/`T` produced by `dl_add` + `dl_bind_tokens`
  (`role`, byte range, token range) and the private `dl_match_at`/`dl_dent`;
- `llama_tokens` and the observation that a delimiter is a token-id sequence;
- nothing else: the probe has no externs, no allocation, no llama/ggml call.

Interfaces the fixture relies on:

- `common_chat_msg_delimiters::{add, tokenize, split}` and
  `common_chat_msg_spans::{is_user_start, last_user_message_pos}`
  (libllama-common);
- `server_tokens::find_message_spans` and the public `server_tokens`
  constructors / `get_tokens` / `push_back` (libllama-server-impl);
- `mtmd_test_create_input_chunks` and the chunk token queries (libmtmd), plus
  `common_tokenize` with a vocab-only GGUF;
- `common_chat_template` / `autoparser::analyze_template` for the existing
  template group (libllama-common).

Exact integration step for the probe:

1. keep the appended block at the end of the delimiters section;
2. after a token is appended to the streaming cursor, call
   `dl_message_start_at(D,T,tokens,sk_start,sk_len,n_skips,p)` at `p` equal to
   the first unconsumed token; a non-negative role is a message start;
3. call `dl_user_message_start_at` where the server currently calls
   `is_user_start`; the return 0/1 is the boolean, -1 is a contract error;
4. do not replace the existing `dl_split` path until the fixture gate passes;
   `sp_is_user_start` / `sp_last_user_pos` remain the stable interface.

Acceptance criteria for the module: `goose check` clean, all 54 base exports
preserved, the 12 additions (10 delimiters + 2 probes) present, no removed
names, the 12 link cases and the existing 36 cases pass with `failures=0`, and
the probe agrees with `sp_is_user_start` at every span start in the fixture.

Assumptions and possible conflicts:

- the server's current `message_spans` path is unchanged; the probe is additive;
- the probe duplicates the match condition of `dl_split`; both must change
  together (the cases lock the observable result);
- the fixture's link group depends on `server-common.h`'s transitive includes
  and on the flint `build-cuda` library layout; this is the first thing to fail
  on a different tree;
- the media cases use the fixed mtmd test-chunk stream; arbitrary media
  placement needs a real mtmd context and is out of scope;
- per the base-SHA rule, rebase the whole block on the current canon and re-run
  both gates before integration.

## 5. Risks

1. Positional miss: if the caller probes after consuming a marker's first
   token, the message start is lost. The function deliberately does not look
   back.
2. Cursor rewind: sampling retractions that move the cursor backwards require
   re-probing the restored positions.
3. Skip staleness: a media chunk not yet in the skip table is matchable; the
   caller must add the chunk before probing its tokens.
4. Overlapping skips are refused (-1), while `dl_split` does not validate
   them. Real media chunks are disjoint, but a synthetic input can diverge.
5. Role 0 (UNKNOWN) is a valid match and is different from -1 (none).
6. The probe duplicates the match loop from `dl_split`; a change in one must
   be reflected in the other. The 12 cases lock the observable behaviour.
7. Server positions are narrowed to int32 in the native queries; the port uses
   i64. All real prompts are far below 2^31.
8. Tokenization boundary: delimiter ids must match the model vocabulary; a
   marker split across tokenizer pieces is missed, exactly like native.
9. The server currently probes at batch boundaries, not every token. The probe
   is for a per-token path and is not a drop-in replacement for the existing
   checkpoint logic.
10. No build or run: the cases are traced by hand against the native lines, not
    executed. The link group adds a build dependency on `server-common.h`,
    `libllama-server-impl` and `libmtmd`; if that header's transitive includes
    change, only the fixture build is affected.
11. The mtmd test chunk stream is fixed (`1,2,3,4,5` + image placeholders).
    Appended text covers "after media", but a media chunk between two arbitrary
    token runs still needs a real mtmd input. The current helper cannot build
    one without a model/projector.
12. `server_tokens::find_message_spans` is called on a local object for
    non-media cases and on the media-carrying object for cases 11-12; the port
    receives the same skip map that the native object built.

## 6. Hashes

```
ed2c3acabc8504abfa50bb34b83d25d713c1b848832ed3455a3ee36883fb8fe4  source/runtime.goose
2853e886b6e407d9830a0225aa1f514bc2255c3e5959febdb1b6cfd7128e1400  exports-after.txt
a5a483bb0b79aec354561097c1e3556d5300abfe42d902b9919a7b4a9958d6b8  tests/fixture.cpp
19550edb481ab5ce944135abf14c5c20f3dcca981f9e286a33a9e01d25f5bce2  tests/build.sh
```

Exports: 54 in the base canon, 66 now (10 delimiters + 2 linkage), 0 removed.
`goose check`: passed, no warnings in the added block (the file keeps the 149
pre-existing canon warnings at lines <= 11834).
