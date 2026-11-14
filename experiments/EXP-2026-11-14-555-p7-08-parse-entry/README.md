# EXP555: the parse entry point and the content-only fallback (P7-08 slice 6, part 1)

## Question

The remaining P7-08 item "model output parsers" needs an oracle. Two things had
to be established before any code:

1. can `common_chat_parse` run without model weights?
2. what does it do when a template produces no parser definition?

## Oracle without weights

`common_chat_parse(input, is_partial, params)` forwards to
`common_chat_peg_parse(params.parser, ...)`. The parser arena comes from
`common_chat_templates_apply`, which builds it from the template text and the
tool list only (`autoparser::peg_generator::generate_parser`). The input is
`common_chat_input(std::string)`, which fills the token vector with
`LLAMA_TOKEN_NULL`, so no vocabulary is needed either. `common_chat_session(tmpls,
nullptr, inputs)` therefore parses plain text correctly without weights.

Verified with a probe over the fixture templates: every one reports
`format=peg-native` and `parser=present`, so real templates never take the
fallback, and the PEG machinery (chat-peg-parser.cpp, 1239 lines with the builder
and the AST mappers) is the actual size of the remaining item, not a small slice.

## What this slice ports

The empty-arena path of `common_chat_peg_parse`: when no parser definition is
detected the native code uses a static `content(rest()) + end()` grammar, so the
parsed message is `role=assistant` and `content = generation_prompt ++ text`,
with no tools and no reasoning. The port adds `chat_parse_content_only(gp, text,
out)` returning that content, and the fixed role is stated in the fixture.

`common_chat_input` prepends the generation prompt to the input **before**
parsing, so the prompt appears in the parsed content unless a grammar consumes
it. That is observable and gated.

## Gate

```
FG555_RUNTIME_PARSE cases=628 values=1108 failures=0 skipped=0
mutations 6/6 caught (range 0:6)
mutations 6/6 caught (range 6:12)
baseline after restore: rc=0 cases=628 values=1108 failures=0 skipped=0
```

Ten cases: plain text, a prepended generation prompt, empty text with and without
a prompt, partial parsing with a newline, kept spaces, UTF-8 bytes, JSON-looking
text, a 400-byte text, and template markers treated as content. The fixture
compares the full OAI JSON of the native message against the JSON built from the
ported content and the fixed role, so escaping is compared too.

Twelve mutations: the prompt is dropped, reversed, appended, or loses a byte; the
text is dropped, reversed or starts late; the output starts at the second byte;
the text overwrites the prompt; the returned length omits the prompt, is the
prompt only, or is the text only. Two of them are detected by the runtime bounds
check (they index out of range on the empty-text case) rather than by a value
mismatch; the harness reports that distinction instead of hiding it.

## What remains for output parsing

- `common_chat_peg_builder` and the chat grammar construction (tags, tool calls,
  reasoning blocks, permutations).
- The AST mappers (`common_chat_peg_mapper`, gemma4 and minimax variants).
- `is_partial` streaming behaviour, which only exists once a real grammar can
  fail.
- The typed-content direction, grammars/autoparser, model parsers,
  `use_jinja=false`, and the chat API's missing time injection.
