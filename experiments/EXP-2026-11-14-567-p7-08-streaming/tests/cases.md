# Streaming cases: 10 chunk sequences

All messages below are actual output from the native library, not guesses.

- library: `/home/end0/AI/LlamaServer/llama.cpp-flint-cuda/build-cuda/bin/libllama-common.so.0.7.0`
  (sha256 starts `d05eba2bdb0281ec499ca2fb`; the only tree with `common_chat_session`)
- harness: `probe.cpp`, calls `common_chat_parse(common_chat_input(text), is_partial, params)`
  with a PEG arena built by `build_chat_peg_parser`, no weights, no vocab, no template
- grammar (`make_stream_parser`):
  `start + optional("<think>" reasoning(until("</think>")) "</think>") + content(until("<tool_call>")) + zero_or_more(tool) + end`,
  where `tool = "<tool_call>" name(chars[a-zA-Z_]{1,64}) args(until("</tool_call>")) "</tool_call>"`
- `is_partial=1` is `feed()`, `is_partial=0` is `finish()`.
  `feed` additionally keeps the previous result when the candidate message is empty
  (`chat.cpp:1755-1757`); that merged value is shown in the `feed result` column.
- `T` is the raw parse result: `need` = NEED_MORE_INPUT, `ok` = SUCCESS, `fail` = FAIL.
  `end` and `roots` are `common_peg_parse_result::end` and `nodes.size()`.

Notation: `c=` content, `r=` reasoning, `n=` tool calls, then `name` / `args`.

## 1. Plain content, empty chunk, final finish

| # | chunk | accumulated | T / end / roots | candidate message | feed result | monotone |
|---|---|---|---|---|---|---|
| 1 | `""` | `""` | need 0 1 | empty (role only) | empty | — |
| 2 | `Hel` | `Hel` | need 3 1 | c=`Hel` | c=`Hel` | yes (0→3) |
| 3 | `Hello` | `Hello` | need 5 1 | c=`Hello` | c=`Hello` | yes, delta `lo` |
| 4 | `Hello world` | `Hello world` | need 11 1 | c=`Hello world` | c=`Hello world` | yes, delta ` world` |
| 5 | finish `""` | `Hello world` | need 11 1 | c=`Hello world` | c=`Hello world` | yes, delta 0 |

`need` at step 5 is normal: `content(until(...))` consumes to EOF, so the root never
reaches `end`; `common_chat_parse` does not require SUCCESS, and the message is complete.
Step 1 shows the empty-candidate rule: an all-empty parse never clears the result.

## 2. Split in the middle of the opening reasoning tag

| # | chunk | accumulated | T / end / roots | candidate message | feed result | monotone |
|---|---|---|---|---|---|---|
| 1 | `<thi` | `<thi` | need 4 0 | empty | empty | yes (no leak into content) |
| 2 | `nk>` | `<think>` | need 7 1 | empty | empty | yes |
| 3 | `pla` | `<think>pla` | need 10 1 | r=`pla` | r=`pla` | yes (0→3) |
| 4 | `n` | `<think>plan` | need 11 1 | r=`plan` | r=`plan` | yes |
| 5 | `</think>` | `<think>plan</think>` | need 19 2 | r=`plan`, c=`` | r=`plan` | yes |
| 6 | `Hi` | `<think>plan</think>Hi` | need 21 2 | r=`plan`, c=`Hi` | same | yes (c 0→2) |

Step 1 has zero roots: the optional literal returns NEED_MORE at EOF, so the partial
tag is not published as content. This is the property that keeps step 2 from shrinking
content from `<thi` to ``.

## 3. Split inside a multi-byte UTF-8 character

| # | chunk | accumulated bytes | T / end / roots | candidate message | feed result | monotone |
|---|---|---|---|---|---|---|
| 1 | `<think>Приве` + `0xD1` | 18 | need 17 1 | r=`Приве` | r=`Приве` | yes |
| 2 | `т` (0xD1 0x82) | 19 | need 19 1 | r=`Привет` | r=`Привет` | yes, `Приве` is a prefix |
| 3 | `</think>` | 27 | need 27 2 | r=`Привет`, c=`` | same | yes |

The half character is not emitted and not replaced by U+FFFD: `until` returns
NEED_MORE with `end = last_valid_pos` (`peg-parser.cpp:696-700`), so the previous
reasoning string stays a strict byte prefix of the next one.

## 4. Split inside the closing reasoning tag

| # | chunk | accumulated | T / end / roots | candidate message | feed result | monotone |
|---|---|---|---|---|---|---|
| 1 | `<think>plan</thi` | 16 | need 16 1 | r=`plan` | r=`plan` | yes |
| 2 | `nk` | `<think>plan</think` | 18 | need 18 1 | r=`plan` | yes |
| 3 | `>` | `<think>plan</think>` | 19 | need 19 2 | r=`plan`, c=`` | yes |
| 4 | `Hi` | `<think>plan</think>Hi` | 21 | need 21 2 | r=`plan`, c=`Hi` | yes |

The partial `</thi` is held back by `until` PARTIAL_MATCH; reasoning does not include
the marker prefix.

## 5. Split inside the tool-call marker

| # | chunk | accumulated | T / end / roots | candidate message | feed result | monotone |
|---|---|---|---|---|---|---|
| 1 | `Hi<tool_c` | 9 | need 9 2 | c=`Hi`, n=0 | c=`Hi`, n=0 | yes |
| 2 | `all` | `Hi<tool_call` | 12 | need 12 2 | c=`Hi`, n=0 | yes, c unchanged |
| 3 | `>` | `Hi<tool_call>` | 13 | need 13 2 | c=`Hi`, n=0 | yes |
| 4 | `get_wea` | `Hi<tool_call>get_wea` | 20 | need 20 2 | c=`Hi`, n=1 name=`get_wea` args=`{` | n 0→1 |

The partial marker `<tool_c` never appears in content: `until("<tool_call>")` returns
SUCCESS before a prefix that reaches EOF. Content stays `Hi` for four steps; a tool
call appears only when the name has at least one character.

## 6. Tool name grows, empty chunk, arguments begin

| # | chunk | accumulated | T / end / roots | candidate message | feed result | monotone |
|---|---|---|---|---|---|---|
| 1 | `Hi<tool_call>get_weather` | 24 | need 24 2 | c=`Hi`, name=`get_weather`, args=`{` | same | n 0→1 |
| 2 | feed `""` | 24 | need 24 2 | identical | identical | yes, delta 0 |
| 3 | `{"city":"Par` | 36 | need 36 2 | c=`Hi`, name=`get_weather`, args=`{"city":"Par` | same | args `{` is a prefix |

Name growth is the case `compute_diffs` explicitly allows: empty → value or prefix
extension (`chat.cpp:276-282`). Empty chunk feed still reparses the whole input and
produces an identical message, hence an empty delta.

## 7. Argument string, then split in the tool closing tag, then finish

| # | chunk | accumulated | T / end / roots | candidate message | feed result | monotone |
|---|---|---|---|---|---|---|
| 1 | `{"city":"Paris"}` | 40 | need 40 2 | args=`{"city":"Paris"}` | same | yes |
| 2 | `</tool_ca` | 49 | need 49 2 | args unchanged | same | yes, delta 0 |
| 3 | finish `ll>` | 52 | ok 52 3 | c=`Hi`, name=`get_weather`, args=`{"city":"Paris"}` | same | yes |

Step 2: `until("</tool_call>")` stops before the partial closing tag, `tool_close`
returns NEED_MORE; the arguments are complete JSON and stay unchanged. Step 3 reaches
SUCCESS because the tool call ends exactly at EOF and `end` succeeds.

## 8. Finish after plain content

| # | chunk | accumulated | T / end / roots | candidate message | feed result | monotone |
|---|---|---|---|---|---|---|
| 1 | `Hello world` (feed) | 11 | need 11 1 | c=`Hello world` | c=`Hello world` | — |
| 2 | finish `""` | 11 | need 11 1 | c=`Hello world` | same, finished | yes, delta 0 |

`is_partial=0` changes nothing on a NEED_MORE path: the only `is_partial` use inside
`common_chat_peg_parse` is the FAIL branch (`chat.cpp:1647`). The session flag
`finished` is set by `finish` (`chat.cpp:1763-1771`).

## 9. Unparseable tail: partial FAIL and final throw

| # | chunk | accumulated | T / end / roots | candidate message | feed result | monotone |
|---|---|---|---|---|---|---|
| 1 | `Hi<tool_call>f{"a":1}</tool_call>` | 33 | ok 33 2 | c=`Hi`, name=`f`, args=`{"a":1}` | same | — |
| 2 | `oops` (feed) | 37 | fail 33 0 | empty | previous message unchanged | frozen, no delta |
| 3 | finish `""` | 37 | fail 33 0 | THROW `The model produced output that does not match the expected peg-simple format` | — | terminal error |

Step 2 is the important one. `is_partial && result.end > 0` enters the partial branch
(`chat.cpp:1647-1662`), but a FAIL result carries no AST roots: `sequence` returns
`FAIL(start, result.end)` without nodes (`peg-parser.cpp:323`), and the arena visit
walks only `result.nodes` (`peg-parser.cpp:199-203`). The mapper therefore sees an
empty root list, the message is empty, and `feed` keeps the previous result. The
branch avoids the throw but does not publish a partial AST. At finish the same input
throws (`chat.cpp:1670`).

## 10. Protocol errors: feed after finish, second finish

| # | call | expected native behaviour | draft.goose |
|---|---|---|---|
| 1 | `feed` after `finish` | `GGML_ASSERT(!finished && "feed() after finish()")` (`chat.cpp:1754`) | returns `-7`, state unchanged |
| 2 | `finish` twice | `GGML_ASSERT(!finished && "finish() called twice")` (`chat.cpp:1764`) | returns `-7` |
| 3 | `feed("")` before finish | legal, reparses identical input, empty delta | returns `1` if the candidate is empty, delta 0 |

## Monotonicity summary

Required by `server-task.cpp:169` → `common_chat_msg_diff::compute_diffs`:

| field | required relation | source |
|---|---|---|
| `content` | previous is a byte prefix of next; shrink to a prefix is allowed with empty delta | `chat.cpp:58-70`, `250-253` |
| `reasoning_content` | same | `chat.cpp:246-249` |
| `tool_calls.size()` | never decreases | `chat.cpp:255-266` |
| tool call `name` | empty → non-empty, or previous is a prefix of next | `chat.cpp:269-283` |
| tool call `arguments` | previous is a byte prefix of next; shrink to a prefix allowed with empty delta | `chat.cpp:284-292` |
| tool call `id` | may change; change is reported as a header delta | `chat.cpp:285-291` |

Steps marked `yes` above satisfy these relations. Sequence 9 satisfies them only by
freezing: the old message is returned unchanged, and the error appears at finish.
