# Parser output cases, second batch

Eighteen pairs `family + raw completion -> expected JSON` (ten from iteration 1, eight from iteration 2). The expected side is
taken from the native code and the native PEG test corpus, not invented:

- the message shape and the JSON field order are
  `common_chat_msg::to_json_oaicompat` (`common/chat.cpp:156-226`);
- the raw strings and the parsed values are the inputs and `expect(...)`
  arguments of `tests/test-chat.cpp` at the cited lines;
- the grammar decisions are the cited lines of `common/parsers/*.cpp`.

The JSON below is compact, exactly what `dump()` of the native message emits
with an empty `id` (the id field is omitted when empty). A tool call is
`{"type":"function","function":{"name":...,"arguments":...}}` where
`arguments` is a JSON **string**, so quotes inside it are escaped twice.

Two notes on fidelity:

1. `tests/test-chat.cpp` compares messages after `normalize()`, which parses
   `arguments` and dumps it again. The native schema-driven argument builder
   emits compact JSON, so `{"arg1":1}` is the native byte form even where the
   expectation in the test is written `{"arg1": 1}`.
2. The raw string is the model continuation after the generation prompt, as
   `peg_tester::test` feeds it (`tests/test-chat.cpp:1144-1168`).

Argument types are not guessed. Formats that render one element per parameter
(Muse, MiniCPM5, K2 Horizon, Harmony XML) receive a type table built from the
same JSON schema the native parser uses (`may_be_string` / `value_types`):

| table value | native decision | draft behaviour |
|---|---|---|
| `D_STR` | the parameter may only be a string | always JSON-escaped, so `42` stays `"42"` |
| `D_JSON` | the parameter cannot be a string | JSON text copied verbatim |
| `D_MIXED` | string is one of several types | JSON first, text fallback (the native mixed branch) |
| absent | not in the schema | the parameter is dropped, never guessed |

Whole-object formats (Functionary, GigaChat, LLM-jp Harmony) need no table:
their arguments are one JSON object and nothing is retyped.

---

## 1. TranslateGemma — plain answer

Source: `common/parsers/translate-gemma.cpp:57-59`,
`tests/test-chat.cpp:7233`.

Raw:

```
Hello, world!
What's up?
```

Expected:

```json
{"role":"assistant","content":"Hello, world!\nWhat's up?"}
```

The parser is `literal(generation_prompt) << content(rest)`; nothing is
stripped or trimmed.

---

## 2. Functionary v3.2 — content recipient

Source: `common/parsers/functionary-v3-2.cpp:33-40`,
`tests/test-chat.cpp:6189-6190`.

Raw:

```
all
Hello, world!
What's up?
```

Expected:

```json
{"role":"assistant","content":"Hello, world!\nWhat's up?"}
```

`all\n` is the content recipient; the payload is everything after it.

---

## 3. Functionary v3.2 — tool recipient

Source: `common/parsers/functionary-v3-2.cpp:43-57`,
`tests/test-chat.cpp:6191-6196`.

Raw:

```
special_function
{"arg1": 1}
```

Expected:

```json
{"role":"assistant","content":"","tool_calls":[{"type":"function","function":{"name":"special_function","arguments":"{\"arg1\":1}"}}]}
```

The recipient before `\n` is the function name; the rest is the arguments
object, parsed by the tool schema and re-emitted compactly. The native test
expects `message_assist_call` (`tests/test-chat.cpp:1050`), whose arguments
are `{"arg1": 1}` before normalization.

---

## 4. GigaChat V3 — content then one call

Source: `common/parsers/gigachat-v3.cpp:28-61`,
`tests/test-chat.cpp:7181-7189`.

Raw:

```
Hello, world!
What's up?<|message_sep|>

function call<|role_sep|>
{"name": "special_function", "arguments": {"arg1": 1}}
```

Expected:

```json
{"role":"assistant","content":"Hello, world!\nWhat's up?","tool_calls":[{"type":"function","function":{"name":"special_function","arguments":"{\"arg1\":1}"}}]}
```

Content stops at `<|message_sep|>`; the call is the `function call` block and
at most one call is supported (`max_calls = 1`, line 49).

---

## 5. Muse Glimmer — plain final answer

Source: `common/parsers/muse-glimmer.cpp:74-76`,
`tests/test-chat.cpp:6604-6607`.

Raw:

```
 to=user<|message|>Hello, world!
What's up?<|eot|>
```

Expected:

```json
{"role":"assistant","content":"Hello, world!\nWhat's up?"}
```

` to=user` is the final recipient and `<|eot|>` ends the turn.

---

## 6. Muse Glimmer — tool call with ATEM markup

Source: `common/parsers/muse-glimmer.cpp:78-117`,
`tests/test-chat.cpp:6611-6616`.

Raw:

```
 to=special_function<|message|><atem:function_calls>
<atem:invoke name="special_function">
<atem:parameter name="arg1">1</atem:parameter>
</atem:invoke>
</atem:function_calls>
```

Expected:

```json
{"role":"assistant","content":"","tool_calls":[{"type":"function","function":{"name":"special_function","arguments":"{\"arg1\":1}"}}]}
```

`arg1` is declared as an integer, so the table marks it `D_JSON` and the value
stays a JSON number. A mixed string/number/object schema is `D_MIXED`: JSON
first, text fallback, exactly the native mixed branch.

---

## 7. Muse Glimmer — reasoning then final answer

Source: `common/parsers/muse-glimmer.cpp:60-76`,
`tests/test-chat.cpp:6637-6642`.

Raw:

```
 to=self<|message|>Thinking.<|eom|><|start|>assistant to=user<|message|>Hello!<|eot|>
```

Expected:

```json
{"role":"assistant","content":"Hello!","reasoning_content":"Thinking."}
```

`to=self` is chain-of-thought and ends on `<|eom|>`; the next assistant
message starts with `<|start|>assistant`.

---

## 8. MiniCPM5 — plain answer

Source: `common/parsers/minicpm5.cpp:108-113`,
`tests/test-chat.cpp:7241-7244` (`enable_thinking=false`).

Raw:

```
Hello, world!
What's up?
```

Expected:

```json
{"role":"assistant","content":"Hello, world!\nWhat's up?"}
```

With no `<think>` block and no `<function` the parser captures the rest.

---

## 9. MiniCPM5 — XML tool call

Source: `common/parsers/minicpm5.cpp:62-110`,
`tests/test-chat.cpp:7247-7252`.

Raw:

```
<function name="python"><param name="code">print('Hello, World!')</param></function>
```

Expected:

```json
{"role":"assistant","content":"","tool_calls":[{"type":"function","function":{"name":"python","arguments":"{\"code\":\"print('Hello, World!')\"}"}}]}
```

`code` may be a string, so the string rule (until `</param>`) applies and the
value is JSON-escaped rather than parsed as JSON.

---

## 10. MiniCPM5 — CDATA parameter

Source: `common/parsers/minicpm5.cpp:65-68`,
`tests/test-chat.cpp:7260-7265`.

Raw:

```
<function name="html"><param name="markup"><![CDATA[<a href="/x">hi</a> </param>]]></param></function>
```

Expected:

```json
{"role":"assistant","content":"","tool_calls":[{"type":"function","function":{"name":"html","arguments":"{\"markup\":\"<a href=\\\"/x\\\">hi</a> </param>\"}"}}]}
```

The CDATA branch captures the inner text only: the first `</param>` is part
of the value, the closing `]]>` and the real `</param>` are markers.

---

## Native expectations not used as pairs

- K2 Horizon and LLM-jp Harmony have native tests
  (`tests/test-chat.cpp:4894-5010`, `6892-6985`) and their formats are
  tabulated in `REPORT.md`, but they are not in this draft (they need three
  think tags, argument permutation and space-tolerant token scanning).
- Harmony XML has no test in `tests/test-chat.cpp`; its only oracle is the
  parser source `common/parsers/harmony-xml.cpp`. It is not covered by a pair.
- The MiniCPM5 empty-argument case (`<function name="empty_args"></function>`
  -> `"{}"`, `tests/test-chat.cpp:7254-7257`) and the `</think>` reasoning
  case (`7268-7276`) are implemented in the draft but not listed as numbered
  pairs because the limit is ten.

---

## 11. Harmony XML — final content

Source: `common/parsers/harmony-xml.cpp:74-77`, `:186-189` (no native PEG test
exists for this template).

Raw:

```
<|channel|>final<|message|>Hello<|end|>
```

Expected:

```json
{"role":"assistant","content":"Hello"}
```

The final rule is `<|channel|>final<|message|>` + content up to `<|end|>` or an
EOG token, then an optional `<|end|>`/EOG. Content is not trimmed.

---

## 12. Harmony XML — `rfl:` parameter call

Source: `common/parsers/harmony-xml.cpp:49-50` (parameter markers), `:90-172`
(tool grammar), `:99-101` (optional closing tag).

Raw:

```
 to=special_function<|channel|>analysis <|constrain|>xml<|message|><rfl:parameter name="arg1">
1
</rfl:parameter>
<|end|>
```

Expected:

```json
{"role":"assistant","content":"","tool_calls":[{"type":"function","function":{"name":"special_function","arguments":"{\"arg1\":1}"}}]}
```

`arg1` is an integer in the tool schema (`D_JSON`), so the value stays `1`.
The newline after `">"` is part of the native parameter opener, and the closing
`</rfl:parameter>` is optional on the last argument because EOG ends the call.

---

## 13. Muse Spark — commentary is content

Source: `common/parsers/muse-spark.cpp:67-76`,
`tests/test-chat.cpp:6682-6686`.

Raw:

```
 to=commentary<|message|>Hello, world!
What's up?<|eom|><|start|>assistant to=user<|message|>Done!<|eot|>
```

Expected:

```json
{"role":"assistant","content":"Hello, world!\nWhat's up?Done!"}
```

Commentary is user-facing content and concatenates with the final answer with
no separator, as the native parser does.

---

## 14. Muse Spark — commentary then tool call

Source: `common/parsers/muse-spark.cpp:78-121`,
`tests/test-chat.cpp:6689-6696`.

Raw:

```
 to=commentary<|message|>Let me look that up.<|eom|><|start|>assistant to=special_function<|message|><atem:function_calls>
<atem:invoke name="special_function">
<atem:parameter name="arg1">1</atem:parameter>
</atem:invoke>
</atem:function_calls>
```

Expected:

```json
{"role":"assistant","content":"Let me look that up.","tool_calls":[{"type":"function","function":{"name":"special_function","arguments":"{\"arg1\":1}"}}]}
```

---

## 15. K2 Horizon — reasoning then content

Source: `common/parsers/k2-horizon.cpp:80-101`,
`tests/test-chat.cpp:4898-4901`.

Raw:

```
I'm
thinking</ifm|think>Hello, world!
What's up?
```

Expected:

```json
{"role":"assistant","content":"Hello, world!\nWhat's up?","reasoning_content":"I'm\nthinking"}
```

The `<ifm|think>` opener belongs to the generation prompt; `reasoning_effort`
selects `think`, `think_fast` or `think_faster` and the matching close tag.

---

## 16. K2 Horizon — XML call

Source: `common/parsers/k2-horizon.cpp:107-158`,
`tests/test-chat.cpp:4914-4925`.

Raw:

```
I'm
thinking</ifm|think><ifm|tool_calls>
<ifm|tool_call>special_function
<ifm|arg_key>arg1</ifm|arg_key>
<ifm|arg_value>1</ifm|arg_value>
</ifm|tool_call>
</ifm|tool_calls>
```

Expected:

```json
{"role":"assistant","content":"","reasoning_content":"I'm\nthinking","tool_calls":[{"type":"function","function":{"name":"special_function","arguments":"{\"arg1\":1}"}}]}
```

`arg_type` is optional and only appears in the `xml_typed` call format
(`k2-horizon.cpp:110-111`); `arg1` is an integer (`D_JSON`).

---

## 17. LLM-jp Harmony — final content

Source: `common/parsers/llm-jp-harmony.cpp:72-94`,
`tests/test-chat.cpp:6895`.

Raw:

```
<|channel|> final<|message|> Hello, world!
What's up?
```

Expected:

```json
{"role":"assistant","content":"Hello, world!\nWhat's up?"}
```

Every special token may be followed by spaces (the tokenizer emits one); the
single optional space after `<|message|>` is consumed, so an intentional second
space would survive.

---

## 18. LLM-jp Harmony — tool recipient in the role header

Source: `common/parsers/llm-jp-harmony.cpp:106-131`,
`tests/test-chat.cpp:6917-6929`.

Raw:

```
<|channel|> analysis<|message|> I'm
thinking<|end|><|start|> assistant to=functions.special_function<|channel|> commentary <|constrain|>  json<|message|> {"arg1": 1}
```

Expected:

```json
{"role":"assistant","content":"","reasoning_content":"I'm\nthinking","tool_calls":[{"type":"function","function":{"name":"special_function","arguments":"{\"arg1\":1}"}}]}
```

The recipient may also sit in the channel header
(`llm-jp-harmony.cpp:121-123`); parallel calls are `<|end|>`-separated assistant
messages (`:128-131`).
