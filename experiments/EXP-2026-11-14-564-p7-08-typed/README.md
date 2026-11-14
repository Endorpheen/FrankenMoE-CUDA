# EXP564: typed content direction (P7-08 slice 9)

## Hypothesis

The ported message normalizer only rewrites `messages[].content` between a
string and a part array under two template capabilities: `supports_string_content`
(array → string) and `supports_typed_content` (string → one text part). EXP537
recorded `only_typed` as untestable; a probe shows it is reachable through
`common_chat_templates_apply`.

## Oracle

Three independent surfaces, all without weights:

- `common_chat_msgs_to_json_oaicompat(msgs, true/false)` — the exact JSON in both
  forms;
- `common_chat_templates_init(nullptr, src)` + `common_chat_templates_apply` — a
  custom template with `content[0]` forces the typed branch;
- `jinja::caps_get` — the `supports_string_content` / `supports_typed_content`
  flags for a template source.

## Port

`chat_content_parts_caps_json` and `chat_content_string_caps_json` rewrite content
between the two shapes with the capability flags: arrays become a string when
`only_string`, strings become one text part when `only_typed`, media always forces
the string form, unknown part types are kept in the array form, and an empty
content yields `""`. Concatenation and the media scan are reused from the
canonical runtime rather than duplicated, as native does.

## Gate

```
FG565_TYPED pairs=24 values=34 failures=0
mutations 11/11 caught (range 0:11)
mutations 10/10 caught (range 11:21)
```

24 pairs: nine JSON pairs, an unknown part type, both capability combinations
(including the deliberate "same string renders as `h` under string caps" control),
`n == 0` in both directions, media under typed-only caps, Cyrillic, emoji,
control characters, three invalid UTF-8 forms (overlong, truncated, surrogate),
and the separator contract.

## Findings

- Native writes raw UTF-8, escapes control bytes as lowercase `\u00XX`, keeps
  `0x7f` raw, and throws `type_error.316` on invalid UTF-8. The draft's old
  `\xNN` escaper was wrong and was replaced by the canonical escaper plus a strict
  UTF-8 check.
- Compact separators (`","`, `":"`) are what the oracle emits; the module's
  spaced writer produces the same value but different bytes, so the byte contract
  uses the compact form.
- The canonical runtime keeps a second escaper (`esc` with `\xNN`) used by
  `chat_content_json`, `chat_message_full_json` and `chat_tools_json`; unifying
  them is a separate slice.

## Still open

Tool calls in the typed path, legacy `llama_chat_apply_template` content merging,
autoparser/grammars, model parsers, partial streaming and the chat API's missing
time injection.
