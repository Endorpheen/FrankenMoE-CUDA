# Slice 2 oracle cases: reasoning and TAG_WITH_JSON

Ten template/analysis pairs captured from the native `peg_generator::generate_parser` (no model weights). Same probe as slice 1, extended cases only.

Oracle build and run:

```sh
bash build_probe.sh          # links probe.cpp against build/llama-cuda/libllama-common.so
./probe . > probe.out       # 10 cases, analysis + common_peg_arena::dump
```

The analysis lines are the struct fields printed by `autoparser::analyze_template`; `dump=` is the verbatim `common_peg_arena::dump(arena.root())` result. Dumps are single lines. Templates: `t0.jinja` (plain), `t5.jinja` (reasoning tags), `t6.jinja` (reasoning plus wrapped content), `t7.jinja` (`<function=name>` with JSON arguments), `t8.jinja` (reasoning only on tool calls).

## Case 1: plain_roles_none

Template `./t0.jinja`:

```jinja
{% for message in messages %}{{ message['role'] }}: {{ message['content'] }}
{% endfor %}{% if add_generation_prompt %}assistant: {% endif %}
```

```
template=./t0.jinja
params: tools=0 tool_choice=NONE reasoning=0 json_schema=0 add_generation_prompt=1 parallel=1
analysis: user_start=[user:] assistant_start=[assistant:]
analysis: reasoning=NONE start=[] end=[]
analysis: content=PLAIN start=[] end=[]
analysis: tool_mode=NONE section_start=[] section_end=[] per_call_start=[] per_call_end=[]
generation_prompt=[assistant: ]
grammar_lazy=0 grammar_len=0 triggers=0
```

Expected dump (native oracle):

```text
Sequence(Literal(assistant: ), Epsilon, Tag(content, Until()), End)
```

## Case 2: reasoning_plain

Template `./t5.jinja`:

```jinja
{% for m in messages %}{% if m.role == 'user' %}<|user|>{{ m.content }}{% else %}<|assistant|>{% if m.reasoning_content %}<think>{{ m.reasoning_content }}</think>{% endif %}{{ m.content }}{% endif %}{% endfor %}{% if add_generation_prompt %}<|assistant|>{% endif %}
```

```
template=./t5.jinja
params: tools=0 tool_choice=NONE reasoning=1 json_schema=0 add_generation_prompt=1 parallel=1
analysis: user_start=[<|user|>] assistant_start=[<|assistant|>]
analysis: reasoning=TAG_BASED start=[<think>] end=[</think>]
analysis: content=PLAIN start=[] end=[]
analysis: tool_mode=NONE section_start=[] section_end=[] per_call_start=[] per_call_end=[]
generation_prompt=[<|assistant|>]
grammar_lazy=0 grammar_len=0 triggers=0
```

Expected dump (native oracle):

```text
Sequence(Literal(<|assistant|>), Space, Repetition(Sequence(Epsilon, Literal(<think>), Tag(reasoning, Until(</think>)), Epsilon, Literal(</think>)), 0, 1), Tag(content, Until()), End)
```

## Case 3: reasoning_wrapped

Template `./t6.jinja`:

```jinja
{% for m in messages %}{% if m.role == 'user' %}<|user|>{{ m.content }}{% else %}<|assistant|>{% if m.reasoning_content %}<think>{{ m.reasoning_content }}</think>{% endif %}{% if m.content %}<response>{{ m.content }}</response>{% endif %}{% endif %}{% endfor %}{% if add_generation_prompt %}<|assistant|>{% endif %}
```

```
template=./t6.jinja
params: tools=0 tool_choice=NONE reasoning=1 json_schema=0 add_generation_prompt=1 parallel=1
analysis: user_start=[<|user|>] assistant_start=[<|assistant|><response>]
analysis: reasoning=TAG_BASED start=[<think>] end=[</think>]
analysis: content=ALWAYS_WRAPPED start=[<response>] end=[</response>]
analysis: tool_mode=NONE section_start=[] section_end=[] per_call_start=[] per_call_end=[]
generation_prompt=[<|assistant|>]
grammar_lazy=0 grammar_len=0 triggers=0
```

Expected dump (native oracle):

```text
Sequence(Literal(<|assistant|>), Space, Repetition(Sequence(Epsilon, Literal(<think>), Tag(reasoning, Until(</think>)), Epsilon, Literal(</think>)), 0, 1), Literal(<response>), Tag(content, Until(</response>)), Literal(</response>), End)
```

## Case 4: tag_json_auto_parallel

Template `./t7.jinja`:

```jinja
{% for m in messages %}{% if m.role == 'user' %}<user>{{ m.content }}</user>{% else %}<assistant>{{ m.content }}{% for tc in m.tool_calls %}<function={{ tc['function']['name'] }}>{{ tc['function']['arguments'] | tojson }}</function>{% endfor %}</assistant>{% endif %}{% endfor %}{% if add_generation_prompt %}<assistant>{% endif %}
```

```
template=./t7.jinja
params: tools=1 tool_choice=AUTO reasoning=0 json_schema=0 add_generation_prompt=1 parallel=1
analysis: user_start=[<user>] assistant_start=[<assistant>]
analysis: reasoning=NONE start=[] end=[]
analysis: content=PLAIN start=[] end=[]
analysis: tool_mode=TAG_WITH_JSON section_start=[] section_end=[] per_call_start=[] per_call_end=[]
generation_prompt=[<assistant>]
grammar_lazy=0 grammar_len=0 triggers=0
```

Expected dump (native oracle):

```text
Sequence(Literal(<assistant>), Space, Epsilon, Repetition(Tag(content, Epsilon), 0, 1), Repetition(Rule(tool-call, Sequence(Literal(), Choice(Rule(tool-get-weather, Sequence(Atomic(Tag(tool-open, Sequence(Literal(<function=), Atomic(Tag(tool-name, Literal(get_weather))), Literal(>)))), Epsilon, Space, Tag(tool-args, Schema(Rule(json-value, Choice(Rule(json-object, Sequence(Literal({), Space, Choice(Literal(}), Sequence(Rule(json-string, Sequence(Literal("), String("), Literal("))), [cycle], Literal(:), [cycle], [cycle], Repetition(Sequence([cycle], Literal(,), [cycle], [cycle], [cycle], [cycle], [cycle], [cycle]), 0, unbounded), [cycle], Literal(}))))), Rule(json-array, Sequence(Literal([), Space, Choice(Literal(]), Sequence([cycle], Repetition(Sequence([cycle], Literal(,), [cycle], [cycle]), 0, unbounded), [cycle], Literal(]))))), [cycle], Rule(json-number, Sequence(Repetition(Literal(-), 0, 1), Choice(Literal(0), Sequence(CharRepeat([1-9], 1, 1), CharRepeat([0-9], 0, unbounded))), Repetition(Sequence(Literal(.), CharRepeat([0-9], 1, unbounded)), 0, 1), Repetition(Sequence(Choice(Literal(e), Literal(E)), Repetition(CharRepeat([+-], 1, 1), 0, 1), [cycle]), 0, 1), Not(CharRepeat([0-9.eE+-], 1, 1)))), Rule(json-bool, Choice(Literal(true), Literal(false))), Rule(json-null, Literal(null)))), {"type":"object","properties":{"arg1":{"type":"string"},"arg2":{"type":"number"}},"required":["arg1"]})), Atomic(Tag(tool-close, Space)))), Rule(tool-get-time, Sequence(Atomic(Tag(tool-open, Sequence(Literal(<function=), Atomic(Tag(tool-name, Literal(get_time))), Literal(>)))), Epsilon, Space, Tag(tool-args, Schema(Rule(json-value, Choice(Rule(json-object, Sequence(Literal({), Space, Choice(Literal(}), Sequence(Rule(json-string, Sequence(Literal("), String("), Literal("))), [cycle], Literal(:), [cycle], [cycle], Repetition(Sequence([cycle], Literal(,), [cycle], [cycle], [cycle], [cycle], [cycle], [cycle]), 0, unbounded), [cycle], Literal(}))))), Rule(json-array, Sequence(Literal([), Space, Choice(Literal(]), Sequence([cycle], Repetition(Sequence([cycle], Literal(,), [cycle], [cycle]), 0, unbounded), [cycle], Literal(]))))), [cycle], Rule(json-number, Sequence(Repetition(Literal(-), 0, 1), Choice(Literal(0), Sequence(CharRepeat([1-9], 1, 1), CharRepeat([0-9], 0, unbounded))), Repetition(Sequence(Literal(.), CharRepeat([0-9], 1, unbounded)), 0, 1), Repetition(Sequence(Choice(Literal(e), Literal(E)), Repetition(CharRepeat([+-], 1, 1), 0, 1), [cycle]), 0, 1), Not(CharRepeat([0-9.eE+-], 1, 1)))), Rule(json-bool, Choice(Literal(true), Literal(false))), Rule(json-null, Literal(null)))), {"type":"object","properties":{}})), Atomic(Tag(tool-close, Space))))), Repetition(Sequence(Literal(, ), [cycle]), 0, unbounded), Literal())), 0, 1), End)
```

## Case 5: tag_json_auto_serial

Template `./t7.jinja`:

```jinja
{% for m in messages %}{% if m.role == 'user' %}<user>{{ m.content }}</user>{% else %}<assistant>{{ m.content }}{% for tc in m.tool_calls %}<function={{ tc['function']['name'] }}>{{ tc['function']['arguments'] | tojson }}</function>{% endfor %}</assistant>{% endif %}{% endfor %}{% if add_generation_prompt %}<assistant>{% endif %}
```

```
template=./t7.jinja
params: tools=1 tool_choice=AUTO reasoning=0 json_schema=0 add_generation_prompt=1 parallel=0
analysis: user_start=[<user>] assistant_start=[<assistant>]
analysis: reasoning=NONE start=[] end=[]
analysis: content=PLAIN start=[] end=[]
analysis: tool_mode=TAG_WITH_JSON section_start=[] section_end=[] per_call_start=[] per_call_end=[]
generation_prompt=[<assistant>]
grammar_lazy=0 grammar_len=0 triggers=0
```

Expected dump (native oracle):

```text
Sequence(Literal(<assistant>), Space, Epsilon, Repetition(Tag(content, Epsilon), 0, 1), Repetition(Rule(tool-call, Sequence(Literal(), Choice(Rule(tool-get-weather, Sequence(Atomic(Tag(tool-open, Sequence(Literal(<function=), Atomic(Tag(tool-name, Literal(get_weather))), Literal(>)))), Epsilon, Space, Tag(tool-args, Schema(Rule(json-value, Choice(Rule(json-object, Sequence(Literal({), Space, Choice(Literal(}), Sequence(Rule(json-string, Sequence(Literal("), String("), Literal("))), [cycle], Literal(:), [cycle], [cycle], Repetition(Sequence([cycle], Literal(,), [cycle], [cycle], [cycle], [cycle], [cycle], [cycle]), 0, unbounded), [cycle], Literal(}))))), Rule(json-array, Sequence(Literal([), Space, Choice(Literal(]), Sequence([cycle], Repetition(Sequence([cycle], Literal(,), [cycle], [cycle]), 0, unbounded), [cycle], Literal(]))))), [cycle], Rule(json-number, Sequence(Repetition(Literal(-), 0, 1), Choice(Literal(0), Sequence(CharRepeat([1-9], 1, 1), CharRepeat([0-9], 0, unbounded))), Repetition(Sequence(Literal(.), CharRepeat([0-9], 1, unbounded)), 0, 1), Repetition(Sequence(Choice(Literal(e), Literal(E)), Repetition(CharRepeat([+-], 1, 1), 0, 1), [cycle]), 0, 1), Not(CharRepeat([0-9.eE+-], 1, 1)))), Rule(json-bool, Choice(Literal(true), Literal(false))), Rule(json-null, Literal(null)))), {"type":"object","properties":{"arg1":{"type":"string"},"arg2":{"type":"number"}},"required":["arg1"]})), Atomic(Tag(tool-close, Space)))), Rule(tool-get-time, Sequence(Atomic(Tag(tool-open, Sequence(Literal(<function=), Atomic(Tag(tool-name, Literal(get_time))), Literal(>)))), Epsilon, Space, Tag(tool-args, Schema(Rule(json-value, Choice(Rule(json-object, Sequence(Literal({), Space, Choice(Literal(}), Sequence(Rule(json-string, Sequence(Literal("), String("), Literal("))), [cycle], Literal(:), [cycle], [cycle], Repetition(Sequence([cycle], Literal(,), [cycle], [cycle], [cycle], [cycle], [cycle], [cycle]), 0, unbounded), [cycle], Literal(}))))), Rule(json-array, Sequence(Literal([), Space, Choice(Literal(]), Sequence([cycle], Repetition(Sequence([cycle], Literal(,), [cycle], [cycle]), 0, unbounded), [cycle], Literal(]))))), [cycle], Rule(json-number, Sequence(Repetition(Literal(-), 0, 1), Choice(Literal(0), Sequence(CharRepeat([1-9], 1, 1), CharRepeat([0-9], 0, unbounded))), Repetition(Sequence(Literal(.), CharRepeat([0-9], 1, unbounded)), 0, 1), Repetition(Sequence(Choice(Literal(e), Literal(E)), Repetition(CharRepeat([+-], 1, 1), 0, 1), [cycle]), 0, 1), Not(CharRepeat([0-9.eE+-], 1, 1)))), Rule(json-bool, Choice(Literal(true), Literal(false))), Rule(json-null, Literal(null)))), {"type":"object","properties":{}})), Atomic(Tag(tool-close, Space))))), Literal())), 0, 1), End)
```

## Case 6: tag_json_req_parallel

Template `./t7.jinja`:

```jinja
{% for m in messages %}{% if m.role == 'user' %}<user>{{ m.content }}</user>{% else %}<assistant>{{ m.content }}{% for tc in m.tool_calls %}<function={{ tc['function']['name'] }}>{{ tc['function']['arguments'] | tojson }}</function>{% endfor %}</assistant>{% endif %}{% endfor %}{% if add_generation_prompt %}<assistant>{% endif %}
```

```
template=./t7.jinja
params: tools=1 tool_choice=REQUIRED reasoning=0 json_schema=0 add_generation_prompt=1 parallel=1
analysis: user_start=[<user>] assistant_start=[<assistant>]
analysis: reasoning=NONE start=[] end=[]
analysis: content=PLAIN start=[] end=[]
analysis: tool_mode=TAG_WITH_JSON section_start=[] section_end=[] per_call_start=[] per_call_end=[]
generation_prompt=[<assistant>]
grammar_lazy=0 grammar_len=1451 triggers=0
```

Expected dump (native oracle):

```text
Sequence(Literal(<assistant>), Space, Epsilon, Repetition(Tag(content, Epsilon), 0, 1), Rule(tool-call, Sequence(Literal(), Choice(Rule(tool-get-weather, Sequence(Atomic(Tag(tool-open, Sequence(Literal(<function=), Atomic(Tag(tool-name, Literal(get_weather))), Literal(>)))), Epsilon, Space, Tag(tool-args, Schema(Rule(json-value, Choice(Rule(json-object, Sequence(Literal({), Space, Choice(Literal(}), Sequence(Rule(json-string, Sequence(Literal("), String("), Literal("))), [cycle], Literal(:), [cycle], [cycle], Repetition(Sequence([cycle], Literal(,), [cycle], [cycle], [cycle], [cycle], [cycle], [cycle]), 0, unbounded), [cycle], Literal(}))))), Rule(json-array, Sequence(Literal([), Space, Choice(Literal(]), Sequence([cycle], Repetition(Sequence([cycle], Literal(,), [cycle], [cycle]), 0, unbounded), [cycle], Literal(]))))), [cycle], Rule(json-number, Sequence(Repetition(Literal(-), 0, 1), Choice(Literal(0), Sequence(CharRepeat([1-9], 1, 1), CharRepeat([0-9], 0, unbounded))), Repetition(Sequence(Literal(.), CharRepeat([0-9], 1, unbounded)), 0, 1), Repetition(Sequence(Choice(Literal(e), Literal(E)), Repetition(CharRepeat([+-], 1, 1), 0, 1), [cycle]), 0, 1), Not(CharRepeat([0-9.eE+-], 1, 1)))), Rule(json-bool, Choice(Literal(true), Literal(false))), Rule(json-null, Literal(null)))), {"type":"object","properties":{"arg1":{"type":"string"},"arg2":{"type":"number"}},"required":["arg1"]})), Atomic(Tag(tool-close, Space)))), Rule(tool-get-time, Sequence(Atomic(Tag(tool-open, Sequence(Literal(<function=), Atomic(Tag(tool-name, Literal(get_time))), Literal(>)))), Epsilon, Space, Tag(tool-args, Schema(Rule(json-value, Choice(Rule(json-object, Sequence(Literal({), Space, Choice(Literal(}), Sequence(Rule(json-string, Sequence(Literal("), String("), Literal("))), [cycle], Literal(:), [cycle], [cycle], Repetition(Sequence([cycle], Literal(,), [cycle], [cycle], [cycle], [cycle], [cycle], [cycle]), 0, unbounded), [cycle], Literal(}))))), Rule(json-array, Sequence(Literal([), Space, Choice(Literal(]), Sequence([cycle], Repetition(Sequence([cycle], Literal(,), [cycle], [cycle]), 0, unbounded), [cycle], Literal(]))))), [cycle], Rule(json-number, Sequence(Repetition(Literal(-), 0, 1), Choice(Literal(0), Sequence(CharRepeat([1-9], 1, 1), CharRepeat([0-9], 0, unbounded))), Repetition(Sequence(Literal(.), CharRepeat([0-9], 1, unbounded)), 0, 1), Repetition(Sequence(Choice(Literal(e), Literal(E)), Repetition(CharRepeat([+-], 1, 1), 0, 1), [cycle]), 0, 1), Not(CharRepeat([0-9.eE+-], 1, 1)))), Rule(json-bool, Choice(Literal(true), Literal(false))), Rule(json-null, Literal(null)))), {"type":"object","properties":{}})), Atomic(Tag(tool-close, Space))))), Repetition(Sequence(Literal(, ), [cycle]), 0, unbounded), Literal())), End)
```

## Case 7: tag_json_req_serial

Template `./t7.jinja`:

```jinja
{% for m in messages %}{% if m.role == 'user' %}<user>{{ m.content }}</user>{% else %}<assistant>{{ m.content }}{% for tc in m.tool_calls %}<function={{ tc['function']['name'] }}>{{ tc['function']['arguments'] | tojson }}</function>{% endfor %}</assistant>{% endif %}{% endfor %}{% if add_generation_prompt %}<assistant>{% endif %}
```

```
template=./t7.jinja
params: tools=1 tool_choice=REQUIRED reasoning=0 json_schema=0 add_generation_prompt=1 parallel=0
analysis: user_start=[<user>] assistant_start=[<assistant>]
analysis: reasoning=NONE start=[] end=[]
analysis: content=PLAIN start=[] end=[]
analysis: tool_mode=TAG_WITH_JSON section_start=[] section_end=[] per_call_start=[] per_call_end=[]
generation_prompt=[<assistant>]
grammar_lazy=0 grammar_len=1408 triggers=0
```

Expected dump (native oracle):

```text
Sequence(Literal(<assistant>), Space, Epsilon, Repetition(Tag(content, Epsilon), 0, 1), Rule(tool-call, Sequence(Literal(), Choice(Rule(tool-get-weather, Sequence(Atomic(Tag(tool-open, Sequence(Literal(<function=), Atomic(Tag(tool-name, Literal(get_weather))), Literal(>)))), Epsilon, Space, Tag(tool-args, Schema(Rule(json-value, Choice(Rule(json-object, Sequence(Literal({), Space, Choice(Literal(}), Sequence(Rule(json-string, Sequence(Literal("), String("), Literal("))), [cycle], Literal(:), [cycle], [cycle], Repetition(Sequence([cycle], Literal(,), [cycle], [cycle], [cycle], [cycle], [cycle], [cycle]), 0, unbounded), [cycle], Literal(}))))), Rule(json-array, Sequence(Literal([), Space, Choice(Literal(]), Sequence([cycle], Repetition(Sequence([cycle], Literal(,), [cycle], [cycle]), 0, unbounded), [cycle], Literal(]))))), [cycle], Rule(json-number, Sequence(Repetition(Literal(-), 0, 1), Choice(Literal(0), Sequence(CharRepeat([1-9], 1, 1), CharRepeat([0-9], 0, unbounded))), Repetition(Sequence(Literal(.), CharRepeat([0-9], 1, unbounded)), 0, 1), Repetition(Sequence(Choice(Literal(e), Literal(E)), Repetition(CharRepeat([+-], 1, 1), 0, 1), [cycle]), 0, 1), Not(CharRepeat([0-9.eE+-], 1, 1)))), Rule(json-bool, Choice(Literal(true), Literal(false))), Rule(json-null, Literal(null)))), {"type":"object","properties":{"arg1":{"type":"string"},"arg2":{"type":"number"}},"required":["arg1"]})), Atomic(Tag(tool-close, Space)))), Rule(tool-get-time, Sequence(Atomic(Tag(tool-open, Sequence(Literal(<function=), Atomic(Tag(tool-name, Literal(get_time))), Literal(>)))), Epsilon, Space, Tag(tool-args, Schema(Rule(json-value, Choice(Rule(json-object, Sequence(Literal({), Space, Choice(Literal(}), Sequence(Rule(json-string, Sequence(Literal("), String("), Literal("))), [cycle], Literal(:), [cycle], [cycle], Repetition(Sequence([cycle], Literal(,), [cycle], [cycle], [cycle], [cycle], [cycle], [cycle]), 0, unbounded), [cycle], Literal(}))))), Rule(json-array, Sequence(Literal([), Space, Choice(Literal(]), Sequence([cycle], Repetition(Sequence([cycle], Literal(,), [cycle], [cycle]), 0, unbounded), [cycle], Literal(]))))), [cycle], Rule(json-number, Sequence(Repetition(Literal(-), 0, 1), Choice(Literal(0), Sequence(CharRepeat([1-9], 1, 1), CharRepeat([0-9], 0, unbounded))), Repetition(Sequence(Literal(.), CharRepeat([0-9], 1, unbounded)), 0, 1), Repetition(Sequence(Choice(Literal(e), Literal(E)), Repetition(CharRepeat([+-], 1, 1), 0, 1), [cycle]), 0, 1), Not(CharRepeat([0-9.eE+-], 1, 1)))), Rule(json-bool, Choice(Literal(true), Literal(false))), Rule(json-null, Literal(null)))), {"type":"object","properties":{}})), Atomic(Tag(tool-close, Space))))), Literal())), End)
```

## Case 8: tools_only_parallel

Template `./t8.jinja`:

```jinja
{% for m in messages %}{% if m.role == 'user' %}<user>{{ m.content }}</user>{% else %}<assistant>{% if m.tool_calls %}<think>{{ m.reasoning_content }}</think>{% for tc in m.tool_calls %}<function={{ tc['function']['name'] }}>{{ tc['function']['arguments'] | tojson }}</function>{% endfor %}{% else %}{{ m.content }}{% endif %}</assistant>{% endif %}{% endfor %}{% if add_generation_prompt %}<assistant>{% endif %}
```

```
template=./t8.jinja
params: tools=1 tool_choice=AUTO reasoning=1 json_schema=0 add_generation_prompt=1 parallel=1
analysis: user_start=[<user>] assistant_start=[<assistant>]
analysis: reasoning=TOOLS_ONLY start=[<think>] end=[</think>]
analysis: content=PLAIN start=[] end=[]
analysis: tool_mode=TAG_WITH_JSON section_start=[] section_end=[] per_call_start=[] per_call_end=[]
generation_prompt=[<assistant>]
grammar_lazy=0 grammar_len=0 triggers=0
```

Expected dump (native oracle):

```text
Sequence(Literal(<assistant>), Space, Repetition(Sequence(Epsilon, Literal(<think>), Tag(reasoning, Until(</think>)), Epsilon, Literal(</think>)), 0, 1), Repetition(Tag(content, Epsilon), 0, 1), Repetition(Rule(tool-call, Sequence(Literal(), Choice(Rule(tool-get-weather, Sequence(Atomic(Tag(tool-open, Sequence(Literal(<function=), Atomic(Tag(tool-name, Literal(get_weather))), Literal(>)))), Epsilon, Space, Tag(tool-args, Schema(Rule(json-value, Choice(Rule(json-object, Sequence(Literal({), Space, Choice(Literal(}), Sequence(Rule(json-string, Sequence(Literal("), String("), Literal("))), [cycle], Literal(:), [cycle], [cycle], Repetition(Sequence([cycle], Literal(,), [cycle], [cycle], [cycle], [cycle], [cycle], [cycle]), 0, unbounded), [cycle], Literal(}))))), Rule(json-array, Sequence(Literal([), Space, Choice(Literal(]), Sequence([cycle], Repetition(Sequence([cycle], Literal(,), [cycle], [cycle]), 0, unbounded), [cycle], Literal(]))))), [cycle], Rule(json-number, Sequence(Repetition(Literal(-), 0, 1), Choice(Literal(0), Sequence(CharRepeat([1-9], 1, 1), CharRepeat([0-9], 0, unbounded))), Repetition(Sequence(Literal(.), CharRepeat([0-9], 1, unbounded)), 0, 1), Repetition(Sequence(Choice(Literal(e), Literal(E)), Repetition(CharRepeat([+-], 1, 1), 0, 1), [cycle]), 0, 1), Not(CharRepeat([0-9.eE+-], 1, 1)))), Rule(json-bool, Choice(Literal(true), Literal(false))), Rule(json-null, Literal(null)))), {"type":"object","properties":{"arg1":{"type":"string"},"arg2":{"type":"number"}},"required":["arg1"]})), Atomic(Tag(tool-close, Space)))), Rule(tool-get-time, Sequence(Atomic(Tag(tool-open, Sequence(Literal(<function=), Atomic(Tag(tool-name, Literal(get_time))), Literal(>)))), Epsilon, Space, Tag(tool-args, Schema(Rule(json-value, Choice(Rule(json-object, Sequence(Literal({), Space, Choice(Literal(}), Sequence(Rule(json-string, Sequence(Literal("), String("), Literal("))), [cycle], Literal(:), [cycle], [cycle], Repetition(Sequence([cycle], Literal(,), [cycle], [cycle], [cycle], [cycle], [cycle], [cycle]), 0, unbounded), [cycle], Literal(}))))), Rule(json-array, Sequence(Literal([), Space, Choice(Literal(]), Sequence([cycle], Repetition(Sequence([cycle], Literal(,), [cycle], [cycle]), 0, unbounded), [cycle], Literal(]))))), [cycle], Rule(json-number, Sequence(Repetition(Literal(-), 0, 1), Choice(Literal(0), Sequence(CharRepeat([1-9], 1, 1), CharRepeat([0-9], 0, unbounded))), Repetition(Sequence(Literal(.), CharRepeat([0-9], 1, unbounded)), 0, 1), Repetition(Sequence(Choice(Literal(e), Literal(E)), Repetition(CharRepeat([+-], 1, 1), 0, 1), [cycle]), 0, 1), Not(CharRepeat([0-9.eE+-], 1, 1)))), Rule(json-bool, Choice(Literal(true), Literal(false))), Rule(json-null, Literal(null)))), {"type":"object","properties":{}})), Atomic(Tag(tool-close, Space))))), Repetition(Sequence(Literal(, ), [cycle]), 0, unbounded), Literal())), 0, 1), End)
```

## Case 9: tools_only_serial

Template `./t8.jinja`:

```jinja
{% for m in messages %}{% if m.role == 'user' %}<user>{{ m.content }}</user>{% else %}<assistant>{% if m.tool_calls %}<think>{{ m.reasoning_content }}</think>{% for tc in m.tool_calls %}<function={{ tc['function']['name'] }}>{{ tc['function']['arguments'] | tojson }}</function>{% endfor %}{% else %}{{ m.content }}{% endif %}</assistant>{% endif %}{% endfor %}{% if add_generation_prompt %}<assistant>{% endif %}
```

```
template=./t8.jinja
params: tools=1 tool_choice=AUTO reasoning=1 json_schema=0 add_generation_prompt=1 parallel=0
analysis: user_start=[<user>] assistant_start=[<assistant>]
analysis: reasoning=TOOLS_ONLY start=[<think>] end=[</think>]
analysis: content=PLAIN start=[] end=[]
analysis: tool_mode=TAG_WITH_JSON section_start=[] section_end=[] per_call_start=[] per_call_end=[]
generation_prompt=[<assistant>]
grammar_lazy=0 grammar_len=0 triggers=0
```

Expected dump (native oracle):

```text
Sequence(Literal(<assistant>), Space, Repetition(Sequence(Epsilon, Literal(<think>), Tag(reasoning, Until(</think>)), Epsilon, Literal(</think>)), 0, 1), Repetition(Tag(content, Epsilon), 0, 1), Repetition(Rule(tool-call, Sequence(Literal(), Choice(Rule(tool-get-weather, Sequence(Atomic(Tag(tool-open, Sequence(Literal(<function=), Atomic(Tag(tool-name, Literal(get_weather))), Literal(>)))), Epsilon, Space, Tag(tool-args, Schema(Rule(json-value, Choice(Rule(json-object, Sequence(Literal({), Space, Choice(Literal(}), Sequence(Rule(json-string, Sequence(Literal("), String("), Literal("))), [cycle], Literal(:), [cycle], [cycle], Repetition(Sequence([cycle], Literal(,), [cycle], [cycle], [cycle], [cycle], [cycle], [cycle]), 0, unbounded), [cycle], Literal(}))))), Rule(json-array, Sequence(Literal([), Space, Choice(Literal(]), Sequence([cycle], Repetition(Sequence([cycle], Literal(,), [cycle], [cycle]), 0, unbounded), [cycle], Literal(]))))), [cycle], Rule(json-number, Sequence(Repetition(Literal(-), 0, 1), Choice(Literal(0), Sequence(CharRepeat([1-9], 1, 1), CharRepeat([0-9], 0, unbounded))), Repetition(Sequence(Literal(.), CharRepeat([0-9], 1, unbounded)), 0, 1), Repetition(Sequence(Choice(Literal(e), Literal(E)), Repetition(CharRepeat([+-], 1, 1), 0, 1), [cycle]), 0, 1), Not(CharRepeat([0-9.eE+-], 1, 1)))), Rule(json-bool, Choice(Literal(true), Literal(false))), Rule(json-null, Literal(null)))), {"type":"object","properties":{"arg1":{"type":"string"},"arg2":{"type":"number"}},"required":["arg1"]})), Atomic(Tag(tool-close, Space)))), Rule(tool-get-time, Sequence(Atomic(Tag(tool-open, Sequence(Literal(<function=), Atomic(Tag(tool-name, Literal(get_time))), Literal(>)))), Epsilon, Space, Tag(tool-args, Schema(Rule(json-value, Choice(Rule(json-object, Sequence(Literal({), Space, Choice(Literal(}), Sequence(Rule(json-string, Sequence(Literal("), String("), Literal("))), [cycle], Literal(:), [cycle], [cycle], Repetition(Sequence([cycle], Literal(,), [cycle], [cycle], [cycle], [cycle], [cycle], [cycle]), 0, unbounded), [cycle], Literal(}))))), Rule(json-array, Sequence(Literal([), Space, Choice(Literal(]), Sequence([cycle], Repetition(Sequence([cycle], Literal(,), [cycle], [cycle]), 0, unbounded), [cycle], Literal(]))))), [cycle], Rule(json-number, Sequence(Repetition(Literal(-), 0, 1), Choice(Literal(0), Sequence(CharRepeat([1-9], 1, 1), CharRepeat([0-9], 0, unbounded))), Repetition(Sequence(Literal(.), CharRepeat([0-9], 1, unbounded)), 0, 1), Repetition(Sequence(Choice(Literal(e), Literal(E)), Repetition(CharRepeat([+-], 1, 1), 0, 1), [cycle]), 0, 1), Not(CharRepeat([0-9.eE+-], 1, 1)))), Rule(json-bool, Choice(Literal(true), Literal(false))), Rule(json-null, Literal(null)))), {"type":"object","properties":{}})), Atomic(Tag(tool-close, Space))))), Literal())), 0, 1), End)
```

## Case 10: tag_json_content_only

Template `./t7.jinja`:

```jinja
{% for m in messages %}{% if m.role == 'user' %}<user>{{ m.content }}</user>{% else %}<assistant>{{ m.content }}{% for tc in m.tool_calls %}<function={{ tc['function']['name'] }}>{{ tc['function']['arguments'] | tojson }}</function>{% endfor %}</assistant>{% endif %}{% endfor %}{% if add_generation_prompt %}<assistant>{% endif %}
```

```
template=./t7.jinja
params: tools=0 tool_choice=NONE reasoning=0 json_schema=0 add_generation_prompt=1 parallel=1
analysis: user_start=[<user>] assistant_start=[<assistant>]
analysis: reasoning=NONE start=[] end=[]
analysis: content=PLAIN start=[] end=[]
analysis: tool_mode=TAG_WITH_JSON section_start=[] section_end=[] per_call_start=[] per_call_end=[]
generation_prompt=[<assistant>]
grammar_lazy=0 grammar_len=0 triggers=0
```

Expected dump (native oracle):

```text
Sequence(Literal(<assistant>), Epsilon, Tag(content, Until()), End)
```

