# Typed content: 10 differential pairs with real native output

Every `out:` below is copied verbatim from `probe4` (`./probe4 > cases.out`). Nothing is hand-written.
Reproduce with:

```
cd /tmp/opencode/sub-565
B=/home/end0/AI/LlamaServer/llama.cpp-flint-cuda
g++ -std=c++17 -O1 -o probe4 probe4.cpp \
  $B/build-cuda/common/CMakeFiles/llama-common.dir/chat.cpp.o \
  $B/build-cuda/common/CMakeFiles/llama-common.dir/json.cpp.o \
  -I$B -I$B/common -I$B/include -I$B/vendor -I$B/ggml/include \
  -L$B/build-cuda/bin -lllama-common -lllama -lggml -lggml-base -lmtmd \
  -Wl,-rpath,$B/build-cuda/bin -lpthread -ldl
./probe4
```

Message shape used everywhere: `role = "user"`, `content = ""`, the listed `content_parts`.
`dump()` is compact; `dump(2)` is only used in probe.cpp, not here.

## Pairs

### 1. Two text parts, string direction
Input parts: `{text,"first"} {text,"second"}`, `concat_typed_text = true`
API: `common_chat_msgs_to_json_oaicompat({m}, true)`

```
[{"role":"user","content":"first\nsecond"}]
```

Rule: a newline separates text parts (`common/chat.cpp:172-174`).

### 2. Two text parts, typed direction
Same input, `concat_typed_text = false`

```
[{"role":"user","content":[{"type":"text","text":"first"},{"type":"text","text":"second"}]}]
```

Rule: the array branch fires only when the message has no media (`common/chat.cpp:190-198`).

### 3. Single text part, typed direction
Input parts: `{text,"only"}`, concat = false

```
[{"role":"user","content":[{"type":"text","text":"only"}]}]
```

Note: this is the minimal case where the typed shape is visible at all; with one part the rendered
prompt is byte-identical to the string case, which is why a render-only harness cannot see it.

### 4. Text then media marker
Input parts: `{text,"look:"} {media_marker,"<__media__>"}`, concat = true

```
[{"role":"user","content":"look:<__media__>"}]
```

Rule: no newline before a media marker (`common/chat.cpp:175-177`).

### 5. Media marker then text
Input parts: `{media_marker,"<__media__>"} {text,"describe"}`, concat = true

```
[{"role":"user","content":"<__media__>describe"}]
```

Rule: no newline after a media marker (`common/chat.cpp:173-174`).

### 6. Media marker only
Input parts: `{media_marker,"<__media__>"}`, concat = true

```
[{"role":"user","content":"<__media__>"}]
```

### 7. Unknown part type between two texts
Input parts: `{text,"a"} {image_url,"u"} {text,"b"}`, concat = true
(stderr prints `Ignoring content part type: image_url`)

```
[{"role":"user","content":"a\nb"}]
```

Rule: an unknown type is skipped and does not reset the media state (`common/chat.cpp:178-181`).

### 8. Text part ending in its own newline
Input parts: `{text,"a\n"} {text,"b"}`, concat = true

```
[{"role":"user","content":"a\n\nb"}]
```

Rule: the separator is added unconditionally for a non-first text, so an existing trailing newline
produces two.

### 9. Media present under typed caps still flattens
Input parts: `{text,"look:"} {media_marker,"<__media__>"}`, concat = false

```
[{"role":"user","content":"look:<__media__>"}]
```

Rule: `concat_typed_text || contains_media()` (`common/chat.cpp:166`). This is the case that makes
media handling independent of template caps.

### 10. only_typed: a flat string through a typed-only template
Template: `{% for m in messages %}{{ m.content[0] | tojson }}{% endfor %}`
Caps from `jinja::caps_get`: `string=0 typed=1`
API: `common_chat_templates_init(nullptr, src)` + `common_chat_templates_apply`
Input: `content = "hello"` (string, no parts)

```
{"type": "text", "text": "hello"}
```

This is the oracle EXP537 said did not exist. `content[0]` of a string would be the single character
`"h"`; getting the part object proves `messages_inp_normalizer` rewrote the string into
`[{type:"text", text:"hello"}]` (`common/chat.cpp:463-470`).

## Appendix: extra native observations

### A. Same template, typed input (control for pair 10)
Input parts: `{text,"hello"} {text,"world"}`

```
{"type": "text", "text": "hello"}
```

### B. Caps table

```
Qwen3.5-4B: string=1 typed=1
Reka-Edge : string=1 typed=1
synthetic : string=0 typed=1
```

### C. Render difference on a real template (Qwen3.5-4B, `add_generation_prompt=false`)

Typed parts `{text,"first line"} {text,"second line"}`:

```
<|im_start|>user
first linesecond line<|im_end|>
```

Flat string `"first line\nsecond line"`:

```
<|im_start|>user
first line
second line<|im_end|>
```

The two are not equivalent: the typed path loses the separator because the template concatenates
`item.text` with no delimiter (`models/templates/Qwen3.5-4B.jinja:30-31`).

### D. Parser round trip and rejection

Text-only array survives a parse->serialize round trip:

```
in : [{"role":"user","content":[{"type":"text","text":"a"},{"type":"text","text":"b"}]}]
out: content='' parts=2 json=[{"role":"user","content":[{"type":"text","text":"a"},{"type":"text","text":"b"}]}]
```

An `image_url` part is rejected outright:

```
REJECTED: Failed to parse messages: Unsupported content part type: "image_url"
```

### E. Caps scan over the shipped templates

```
templates=74 typed=25 both=25 typed_only=0 string_only=48
```
