# Slice 3 oracle cases: JSON_NATIVE, TAG_WITH_TAGGED, call-id, wrappers

Twenty-one template/analysis pairs captured from the native `peg_generator::generate_parser` (no model weights).

Oracle build and run:

```sh
bash build_probe.sh          # links probe.cpp against build/llama-cuda/libllama-common.so
./probe . > probe.out       # 21 cases; dumps delimited by dump_begin=/dump_end=
```

`dump_begin=` / `dump_end=` delimit the verbatim `common_peg_arena::dump(arena.root())` result, which can contain newlines from literals. Templates: `t0/t5/t6/t7/t8.jinja` plus real files from `upstream/llama.cpp/models/templates`: Mistral-Nemo, Llama-3.1, Cohere2MoE, GLM-4.6, MiniMax-M2, Laguna-S, Mistral-Small.

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
analysis: wrapped=0 fkey=0 owrap=0 tolerate=0
analysis: fields name=[name] args=[arguments] id=[] gen=[] order=[]
analysis: callid pos=0 prefix=[] suffix=[]
analysis: func prefix=[] suffix=[] close=[] args_start=[] args_end=[] arg_name=[]|[] arg_value=[]|[]
analysis: tool_mode=NONE section_start=[] section_end=[] per_call_start=[] per_call_end=[]
generation_prompt=[assistant: ]
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
analysis: wrapped=0 fkey=0 owrap=0 tolerate=0
analysis: fields name=[name] args=[arguments] id=[] gen=[] order=[]
analysis: callid pos=0 prefix=[] suffix=[]
analysis: func prefix=[] suffix=[] close=[] args_start=[] args_end=[] arg_name=[]|[] arg_value=[]|[]
analysis: tool_mode=NONE section_start=[] section_end=[] per_call_start=[] per_call_end=[]
generation_prompt=[<|assistant|>]
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
analysis: wrapped=0 fkey=0 owrap=0 tolerate=0
analysis: fields name=[name] args=[arguments] id=[] gen=[] order=[]
analysis: callid pos=0 prefix=[] suffix=[]
analysis: func prefix=[] suffix=[] close=[] args_start=[] args_end=[] arg_name=[]|[] arg_value=[]|[]
analysis: tool_mode=NONE section_start=[] section_end=[] per_call_start=[] per_call_end=[]
generation_prompt=[<|assistant|>]
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
analysis: wrapped=0 fkey=0 owrap=0 tolerate=0
analysis: fields name=[name] args=[arguments] id=[] gen=[] order=[]
analysis: callid pos=0 prefix=[] suffix=[]
analysis: func prefix=[<function=] suffix=[>] close=[] args_start=[] args_end=[] arg_name=[]|[] arg_value=[]|[]
analysis: tool_mode=TAG_WITH_JSON section_start=[] section_end=[] per_call_start=[] per_call_end=[]
generation_prompt=[<assistant>]
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
analysis: wrapped=0 fkey=0 owrap=0 tolerate=0
analysis: fields name=[name] args=[arguments] id=[] gen=[] order=[]
analysis: callid pos=0 prefix=[] suffix=[]
analysis: func prefix=[<function=] suffix=[>] close=[] args_start=[] args_end=[] arg_name=[]|[] arg_value=[]|[]
analysis: tool_mode=TAG_WITH_JSON section_start=[] section_end=[] per_call_start=[] per_call_end=[]
generation_prompt=[<assistant>]
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
analysis: wrapped=0 fkey=0 owrap=0 tolerate=0
analysis: fields name=[name] args=[arguments] id=[] gen=[] order=[]
analysis: callid pos=0 prefix=[] suffix=[]
analysis: func prefix=[<function=] suffix=[>] close=[] args_start=[] args_end=[] arg_name=[]|[] arg_value=[]|[]
analysis: tool_mode=TAG_WITH_JSON section_start=[] section_end=[] per_call_start=[] per_call_end=[]
generation_prompt=[<assistant>]
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
analysis: wrapped=0 fkey=0 owrap=0 tolerate=0
analysis: fields name=[name] args=[arguments] id=[] gen=[] order=[]
analysis: callid pos=0 prefix=[] suffix=[]
analysis: func prefix=[<function=] suffix=[>] close=[] args_start=[] args_end=[] arg_name=[]|[] arg_value=[]|[]
analysis: tool_mode=TAG_WITH_JSON section_start=[] section_end=[] per_call_start=[] per_call_end=[]
generation_prompt=[<assistant>]
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
analysis: wrapped=0 fkey=0 owrap=0 tolerate=0
analysis: fields name=[name] args=[arguments] id=[] gen=[] order=[]
analysis: callid pos=0 prefix=[] suffix=[]
analysis: func prefix=[<function=] suffix=[>] close=[] args_start=[] args_end=[] arg_name=[]|[] arg_value=[]|[]
analysis: tool_mode=TAG_WITH_JSON section_start=[] section_end=[] per_call_start=[] per_call_end=[]
generation_prompt=[<assistant>]
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
analysis: wrapped=0 fkey=0 owrap=0 tolerate=0
analysis: fields name=[name] args=[arguments] id=[] gen=[] order=[]
analysis: callid pos=0 prefix=[] suffix=[]
analysis: func prefix=[<function=] suffix=[>] close=[] args_start=[] args_end=[] arg_name=[]|[] arg_value=[]|[]
analysis: tool_mode=TAG_WITH_JSON section_start=[] section_end=[] per_call_start=[] per_call_end=[]
generation_prompt=[<assistant>]
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
analysis: wrapped=0 fkey=0 owrap=0 tolerate=0
analysis: fields name=[name] args=[arguments] id=[] gen=[] order=[]
analysis: callid pos=0 prefix=[] suffix=[]
analysis: func prefix=[<function=] suffix=[>] close=[] args_start=[] args_end=[] arg_name=[]|[] arg_value=[]|[]
analysis: tool_mode=TAG_WITH_JSON section_start=[] section_end=[] per_call_start=[] per_call_end=[]
generation_prompt=[<assistant>]
```

Expected dump (native oracle):

```text
Sequence(Literal(<assistant>), Epsilon, Tag(content, Until()), End)
```

## Case 11: json_native_mistral_par

Template `./mistralai-Mistral-Nemo-Instruct-2407.jinja`:

```jinja
{%- if messages[0]["role"] == "system" %}
    {%- set system_message = messages[0]["content"] %}
    {%- set loop_messages = messages[1:] %}
{%- else %}
    {%- set loop_messages = messages %}
{%- endif %}
{%- if not tools is defined %}
    {%- set tools = none %}
{%- endif %}
{%- set user_messages = loop_messages | selectattr("role", "equalto", "user") | list %}

{#- This block checks for alternating user/assistant messages, skipping tool calling messages #}
{%- set ns = namespace() %}
{%- set ns.index = 0 %}
{%- for message in loop_messages %}
    {%- if not (message.role == "tool" or message.role == "tool_results" or (message.tool_calls is defined and message.tool_calls is not none)) %}
        {%- if (message["role"] == "user") != (ns.index % 2 == 0) %}
            {{- raise_exception("After the optional system message, conversation roles must alternate user/assistant/user/assistant/...") }}
        {%- endif %}
        {%- set ns.index = ns.index + 1 %}
    {%- endif %}
{%- endfor %}

{{- bos_token }}
{%- for message in loop_messages %}
    {%- if message["role"] == "user" %}
        {%- if tools is not none and (message == user_messages[-1]) %}
            {{- "[AVAILABLE_TOOLS][" }}
            {%- for tool in tools %}
                {%- set tool = tool.function %}
                {{- '{"type": "function", "function": {' }}
                {%- for key, val in tool.items() if key != "return" %}
                    {%- if val is string %}
                        {{- '"' + key + '": "' + val + '"' }}
                    {%- else %}
                        {{- '"' + key + '": ' + val|tojson }}
                    {%- endif %}
                    {%- if not loop.last %}
                        {{- ", " }}
                    {%- endif %}
                {%- endfor %}
                {{- "}}" }}
                {%- if not loop.last %}
                    {{- ", " }}
                {%- else %}
                    {{- "]" }}
                {%- endif %}
            {%- endfor %}
            {{- "[/AVAILABLE_TOOLS]" }}
            {%- endif %}
        {%- if loop.last and system_message is defined %}
            {{- "[INST]" + system_message + "\n\n" + message["content"] + "[/INST]" }}
        {%- else %}
            {{- "[INST]" + message["content"] + "[/INST]" }}
        {%- endif %}
    {%- elif (message.tool_calls is defined and message.tool_calls is not none) %}
        {{- "[TOOL_CALLS][" }}
        {%- for tool_call in message.tool_calls %}
            {%- set out = tool_call.function|tojson %}
            {{- out[:-1] }}
            {%- if not tool_call.id is defined or tool_call.id|length != 9 %}
                {{- raise_exception("Tool call IDs should be alphanumeric strings with length 9!") }}
            {%- endif %}
            {{- ', "id": "' + tool_call.id + '"}' }}
            {%- if not loop.last %}
                {{- ", " }}
            {%- else %}
                {{- "]" + eos_token }}
            {%- endif %}
        {%- endfor %}
    {%- elif message["role"] == "assistant" %}
        {{- message["content"] + eos_token}}
    {%- elif message["role"] == "tool_results" or message["role"] == "tool" %}
        {%- if message.content is defined and message.content.content is defined %}
            {%- set content = message.content.content %}
        {%- else %}
            {%- set content = message.content %}
        {%- endif %}
        {{- '[TOOL_RESULTS]{"content": ' + content|string + ", " }}
        {%- if not message.tool_call_id is defined or message.tool_call_id|length != 9 %}
            {{- raise_exception("Tool call IDs should be alphanumeric strings with length 9!") }}
        {%- endif %}
        {{- '"call_id": "' + message.tool_call_id + '"}[/TOOL_RESULTS]' }}
    {%- else %}
        {{- raise_exception("Only user and assistant roles are supported, with the exception of an initial optional system message!") }}
    {%- endif %}
{%- endfor %}
```

```
template=./mistralai-Mistral-Nemo-Instruct-2407.jinja
params: tools=1 tool_choice=AUTO reasoning=0 json_schema=0 add_generation_prompt=1 parallel=1
analysis: user_start=[[INST]] assistant_start=[]
analysis: reasoning=NONE start=[] end=[]
analysis: content=PLAIN start=[] end=[]
analysis: wrapped=1 fkey=0 owrap=0 tolerate=0
analysis: fields name=[name] args=[arguments] id=[id] gen=[] order=[name,arguments,id]
analysis: callid pos=0 prefix=[] suffix=[]
analysis: func prefix=[] suffix=[] close=[] args_start=[] args_end=[] arg_name=[]|[] arg_value=[]|[]
analysis: tool_mode=JSON_NATIVE section_start=[[TOOL_CALLS]] section_end=[] per_call_start=[] per_call_end=[]
generation_prompt=[]
```

Expected dump (native oracle):

```text
Sequence(Epsilon, Space, Epsilon, Repetition(Tag(content, Until([TOOL_CALLS])), 0, 1), Repetition(Rule(tool-call, Sequence(Literal([TOOL_CALLS]), Space, Literal([), Space, Choice(Rule(tool-get-weather, Tag(tool, Sequence(Atomic(Tag(tool-open, Literal({))), Space, Epsilon, Literal("name"), Space, Literal(:), Space, Atomic(Sequence(Literal("), Atomic(Tag(tool-name, Literal(get_weather))), Literal("))), Space, Literal(,), Space, Literal("arguments"), Space, Literal(:), Space, Tag(tool-args, Schema(Rule(json-value, Choice(Rule(json-object, Sequence(Literal({), Space, Choice(Literal(}), Sequence(Rule(json-string, Sequence(Literal("), String("), Literal("))), [cycle], Literal(:), [cycle], [cycle], Repetition(Sequence([cycle], Literal(,), [cycle], [cycle], [cycle], [cycle], [cycle], [cycle]), 0, unbounded), [cycle], Literal(}))))), Rule(json-array, Sequence(Literal([), Space, Choice(Literal(]), Sequence([cycle], Repetition(Sequence([cycle], Literal(,), [cycle], [cycle]), 0, unbounded), [cycle], Literal(]))))), [cycle], Rule(json-number, Sequence(Repetition(Literal(-), 0, 1), Choice(Literal(0), Sequence(CharRepeat([1-9], 1, 1), CharRepeat([0-9], 0, unbounded))), Repetition(Sequence(Literal(.), CharRepeat([0-9], 1, unbounded)), 0, 1), Repetition(Sequence(Choice(Literal(e), Literal(E)), Repetition(CharRepeat([+-], 1, 1), 0, 1), [cycle]), 0, 1), Not(CharRepeat([0-9.eE+-], 1, 1)))), Rule(json-bool, Choice(Literal(true), Literal(false))), Rule(json-null, Literal(null)))), {"type":"object","properties":{"arg1":{"type":"string"},"arg2":{"type":"number"}},"required":["arg1"]})), Space, Literal(,), Space, Repetition(Atomic(Sequence(Literal("id"), Space, Literal(:), Space, Choice(Sequence(Literal("), Atomic(Tag(tool-id, String("))), Literal(")), Atomic(Tag(tool-id, Rule(json-number, Sequence(Repetition(Literal(-), 0, 1), Choice(Literal(0), Sequence(CharRepeat([1-9], 1, 1), CharRepeat([0-9], 0, unbounded))), Repetition(Sequence(Literal(.), CharRepeat([0-9], 1, unbounded)), 0, 1), Repetition(Sequence(Choice(Literal(e), Literal(E)), Repetition(CharRepeat([+-], 1, 1), 0, 1), [cycle]), 0, 1), Not(CharRepeat([0-9.eE+-], 1, 1))))))))), 0, 1), Space, Atomic(Tag(tool-close, Literal(})))))), Rule(tool-get-time, Tag(tool, Sequence(Atomic(Tag(tool-open, Literal({))), Space, Epsilon, Literal("name"), Space, Literal(:), Space, Atomic(Sequence(Literal("), Atomic(Tag(tool-name, Literal(get_time))), Literal("))), Space, Literal(,), Space, Literal("arguments"), Space, Literal(:), Space, Tag(tool-args, Schema(Rule(json-value, Choice(Rule(json-object, Sequence(Literal({), Space, Choice(Literal(}), Sequence(Rule(json-string, Sequence(Literal("), String("), Literal("))), [cycle], Literal(:), [cycle], [cycle], Repetition(Sequence([cycle], Literal(,), [cycle], [cycle], [cycle], [cycle], [cycle], [cycle]), 0, unbounded), [cycle], Literal(}))))), Rule(json-array, Sequence(Literal([), Space, Choice(Literal(]), Sequence([cycle], Repetition(Sequence([cycle], Literal(,), [cycle], [cycle]), 0, unbounded), [cycle], Literal(]))))), [cycle], Rule(json-number, Sequence(Repetition(Literal(-), 0, 1), Choice(Literal(0), Sequence(CharRepeat([1-9], 1, 1), CharRepeat([0-9], 0, unbounded))), Repetition(Sequence(Literal(.), CharRepeat([0-9], 1, unbounded)), 0, 1), Repetition(Sequence(Choice(Literal(e), Literal(E)), Repetition(CharRepeat([+-], 1, 1), 0, 1), [cycle]), 0, 1), Not(CharRepeat([0-9.eE+-], 1, 1)))), Rule(json-bool, Choice(Literal(true), Literal(false))), Rule(json-null, Literal(null)))), {"type":"object","properties":{}})), Space, Literal(,), Space, Repetition(Atomic(Sequence(Literal("id"), Space, Literal(:), Space, Choice(Sequence(Literal("), Atomic(Tag(tool-id, String("))), Literal(")), Atomic(Tag(tool-id, Rule(json-number, Sequence(Repetition(Literal(-), 0, 1), Choice(Literal(0), Sequence(CharRepeat([1-9], 1, 1), CharRepeat([0-9], 0, unbounded))), Repetition(Sequence(Literal(.), CharRepeat([0-9], 1, unbounded)), 0, 1), Repetition(Sequence(Choice(Literal(e), Literal(E)), Repetition(CharRepeat([+-], 1, 1), 0, 1), [cycle]), 0, 1), Not(CharRepeat([0-9.eE+-], 1, 1))))))))), 0, 1), Space, Atomic(Tag(tool-close, Literal(}))))))), Repetition(Sequence(Space, Literal(,), Space, [cycle]), 0, unbounded), Space, Literal(]), Space, Literal())), 0, 1), End)
```

## Case 12: json_native_mistral_ser

Template `./mistralai-Mistral-Nemo-Instruct-2407.jinja`:

```jinja
{%- if messages[0]["role"] == "system" %}
    {%- set system_message = messages[0]["content"] %}
    {%- set loop_messages = messages[1:] %}
{%- else %}
    {%- set loop_messages = messages %}
{%- endif %}
{%- if not tools is defined %}
    {%- set tools = none %}
{%- endif %}
{%- set user_messages = loop_messages | selectattr("role", "equalto", "user") | list %}

{#- This block checks for alternating user/assistant messages, skipping tool calling messages #}
{%- set ns = namespace() %}
{%- set ns.index = 0 %}
{%- for message in loop_messages %}
    {%- if not (message.role == "tool" or message.role == "tool_results" or (message.tool_calls is defined and message.tool_calls is not none)) %}
        {%- if (message["role"] == "user") != (ns.index % 2 == 0) %}
            {{- raise_exception("After the optional system message, conversation roles must alternate user/assistant/user/assistant/...") }}
        {%- endif %}
        {%- set ns.index = ns.index + 1 %}
    {%- endif %}
{%- endfor %}

{{- bos_token }}
{%- for message in loop_messages %}
    {%- if message["role"] == "user" %}
        {%- if tools is not none and (message == user_messages[-1]) %}
            {{- "[AVAILABLE_TOOLS][" }}
            {%- for tool in tools %}
                {%- set tool = tool.function %}
                {{- '{"type": "function", "function": {' }}
                {%- for key, val in tool.items() if key != "return" %}
                    {%- if val is string %}
                        {{- '"' + key + '": "' + val + '"' }}
                    {%- else %}
                        {{- '"' + key + '": ' + val|tojson }}
                    {%- endif %}
                    {%- if not loop.last %}
                        {{- ", " }}
                    {%- endif %}
                {%- endfor %}
                {{- "}}" }}
                {%- if not loop.last %}
                    {{- ", " }}
                {%- else %}
                    {{- "]" }}
                {%- endif %}
            {%- endfor %}
            {{- "[/AVAILABLE_TOOLS]" }}
            {%- endif %}
        {%- if loop.last and system_message is defined %}
            {{- "[INST]" + system_message + "\n\n" + message["content"] + "[/INST]" }}
        {%- else %}
            {{- "[INST]" + message["content"] + "[/INST]" }}
        {%- endif %}
    {%- elif (message.tool_calls is defined and message.tool_calls is not none) %}
        {{- "[TOOL_CALLS][" }}
        {%- for tool_call in message.tool_calls %}
            {%- set out = tool_call.function|tojson %}
            {{- out[:-1] }}
            {%- if not tool_call.id is defined or tool_call.id|length != 9 %}
                {{- raise_exception("Tool call IDs should be alphanumeric strings with length 9!") }}
            {%- endif %}
            {{- ', "id": "' + tool_call.id + '"}' }}
            {%- if not loop.last %}
                {{- ", " }}
            {%- else %}
                {{- "]" + eos_token }}
            {%- endif %}
        {%- endfor %}
    {%- elif message["role"] == "assistant" %}
        {{- message["content"] + eos_token}}
    {%- elif message["role"] == "tool_results" or message["role"] == "tool" %}
        {%- if message.content is defined and message.content.content is defined %}
            {%- set content = message.content.content %}
        {%- else %}
            {%- set content = message.content %}
        {%- endif %}
        {{- '[TOOL_RESULTS]{"content": ' + content|string + ", " }}
        {%- if not message.tool_call_id is defined or message.tool_call_id|length != 9 %}
            {{- raise_exception("Tool call IDs should be alphanumeric strings with length 9!") }}
        {%- endif %}
        {{- '"call_id": "' + message.tool_call_id + '"}[/TOOL_RESULTS]' }}
    {%- else %}
        {{- raise_exception("Only user and assistant roles are supported, with the exception of an initial optional system message!") }}
    {%- endif %}
{%- endfor %}
```

```
template=./mistralai-Mistral-Nemo-Instruct-2407.jinja
params: tools=1 tool_choice=AUTO reasoning=0 json_schema=0 add_generation_prompt=1 parallel=0
analysis: user_start=[[INST]] assistant_start=[]
analysis: reasoning=NONE start=[] end=[]
analysis: content=PLAIN start=[] end=[]
analysis: wrapped=1 fkey=0 owrap=0 tolerate=0
analysis: fields name=[name] args=[arguments] id=[id] gen=[] order=[name,arguments,id]
analysis: callid pos=0 prefix=[] suffix=[]
analysis: func prefix=[] suffix=[] close=[] args_start=[] args_end=[] arg_name=[]|[] arg_value=[]|[]
analysis: tool_mode=JSON_NATIVE section_start=[[TOOL_CALLS]] section_end=[] per_call_start=[] per_call_end=[]
generation_prompt=[]
```

Expected dump (native oracle):

```text
Sequence(Epsilon, Space, Epsilon, Repetition(Tag(content, Until([TOOL_CALLS])), 0, 1), Repetition(Rule(tool-call, Sequence(Literal([TOOL_CALLS]), Space, Literal([), Space, Choice(Rule(tool-get-weather, Tag(tool, Sequence(Atomic(Tag(tool-open, Literal({))), Space, Epsilon, Literal("name"), Space, Literal(:), Space, Atomic(Sequence(Literal("), Atomic(Tag(tool-name, Literal(get_weather))), Literal("))), Space, Literal(,), Space, Literal("arguments"), Space, Literal(:), Space, Tag(tool-args, Schema(Rule(json-value, Choice(Rule(json-object, Sequence(Literal({), Space, Choice(Literal(}), Sequence(Rule(json-string, Sequence(Literal("), String("), Literal("))), [cycle], Literal(:), [cycle], [cycle], Repetition(Sequence([cycle], Literal(,), [cycle], [cycle], [cycle], [cycle], [cycle], [cycle]), 0, unbounded), [cycle], Literal(}))))), Rule(json-array, Sequence(Literal([), Space, Choice(Literal(]), Sequence([cycle], Repetition(Sequence([cycle], Literal(,), [cycle], [cycle]), 0, unbounded), [cycle], Literal(]))))), [cycle], Rule(json-number, Sequence(Repetition(Literal(-), 0, 1), Choice(Literal(0), Sequence(CharRepeat([1-9], 1, 1), CharRepeat([0-9], 0, unbounded))), Repetition(Sequence(Literal(.), CharRepeat([0-9], 1, unbounded)), 0, 1), Repetition(Sequence(Choice(Literal(e), Literal(E)), Repetition(CharRepeat([+-], 1, 1), 0, 1), [cycle]), 0, 1), Not(CharRepeat([0-9.eE+-], 1, 1)))), Rule(json-bool, Choice(Literal(true), Literal(false))), Rule(json-null, Literal(null)))), {"type":"object","properties":{"arg1":{"type":"string"},"arg2":{"type":"number"}},"required":["arg1"]})), Space, Literal(,), Space, Repetition(Atomic(Sequence(Literal("id"), Space, Literal(:), Space, Choice(Sequence(Literal("), Atomic(Tag(tool-id, String("))), Literal(")), Atomic(Tag(tool-id, Rule(json-number, Sequence(Repetition(Literal(-), 0, 1), Choice(Literal(0), Sequence(CharRepeat([1-9], 1, 1), CharRepeat([0-9], 0, unbounded))), Repetition(Sequence(Literal(.), CharRepeat([0-9], 1, unbounded)), 0, 1), Repetition(Sequence(Choice(Literal(e), Literal(E)), Repetition(CharRepeat([+-], 1, 1), 0, 1), [cycle]), 0, 1), Not(CharRepeat([0-9.eE+-], 1, 1))))))))), 0, 1), Space, Atomic(Tag(tool-close, Literal(})))))), Rule(tool-get-time, Tag(tool, Sequence(Atomic(Tag(tool-open, Literal({))), Space, Epsilon, Literal("name"), Space, Literal(:), Space, Atomic(Sequence(Literal("), Atomic(Tag(tool-name, Literal(get_time))), Literal("))), Space, Literal(,), Space, Literal("arguments"), Space, Literal(:), Space, Tag(tool-args, Schema(Rule(json-value, Choice(Rule(json-object, Sequence(Literal({), Space, Choice(Literal(}), Sequence(Rule(json-string, Sequence(Literal("), String("), Literal("))), [cycle], Literal(:), [cycle], [cycle], Repetition(Sequence([cycle], Literal(,), [cycle], [cycle], [cycle], [cycle], [cycle], [cycle]), 0, unbounded), [cycle], Literal(}))))), Rule(json-array, Sequence(Literal([), Space, Choice(Literal(]), Sequence([cycle], Repetition(Sequence([cycle], Literal(,), [cycle], [cycle]), 0, unbounded), [cycle], Literal(]))))), [cycle], Rule(json-number, Sequence(Repetition(Literal(-), 0, 1), Choice(Literal(0), Sequence(CharRepeat([1-9], 1, 1), CharRepeat([0-9], 0, unbounded))), Repetition(Sequence(Literal(.), CharRepeat([0-9], 1, unbounded)), 0, 1), Repetition(Sequence(Choice(Literal(e), Literal(E)), Repetition(CharRepeat([+-], 1, 1), 0, 1), [cycle]), 0, 1), Not(CharRepeat([0-9.eE+-], 1, 1)))), Rule(json-bool, Choice(Literal(true), Literal(false))), Rule(json-null, Literal(null)))), {"type":"object","properties":{}})), Space, Literal(,), Space, Repetition(Atomic(Sequence(Literal("id"), Space, Literal(:), Space, Choice(Sequence(Literal("), Atomic(Tag(tool-id, String("))), Literal(")), Atomic(Tag(tool-id, Rule(json-number, Sequence(Repetition(Literal(-), 0, 1), Choice(Literal(0), Sequence(CharRepeat([1-9], 1, 1), CharRepeat([0-9], 0, unbounded))), Repetition(Sequence(Literal(.), CharRepeat([0-9], 1, unbounded)), 0, 1), Repetition(Sequence(Choice(Literal(e), Literal(E)), Repetition(CharRepeat([+-], 1, 1), 0, 1), [cycle]), 0, 1), Not(CharRepeat([0-9.eE+-], 1, 1))))))))), 0, 1), Space, Atomic(Tag(tool-close, Literal(}))))))), Space, Literal(]), Space, Literal())), 0, 1), End)
```

## Case 13: json_native_llama_wrap

Template `./meta-llama-Llama-3.1-8B-Instruct.jinja`:

```jinja
{{- bos_token }}
{%- if custom_tools is defined %}
    {%- set tools = custom_tools %}
{%- endif %}
{%- if not tools_in_user_message is defined %}
    {%- set tools_in_user_message = true %}
{%- endif %}
{%- if not date_string is defined %}
    {%- set date_string = "26 Jul 2024" %}
{%- endif %}
{%- if not tools is defined %}
    {%- set tools = none %}
{%- endif %}

{#- This block extracts the system message, so we can slot it into the right place. #}
{%- if messages[0]['role'] == 'system' %}
    {%- set system_message = messages[0]['content']|trim %}
    {%- set messages = messages[1:] %}
{%- else %}
    {%- set system_message = "" %}
{%- endif %}

{#- System message + builtin tools #}
{{- "<|start_header_id|>system<|end_header_id|>\n\n" }}
{%- if builtin_tools is defined or tools is not none %}
    {{- "Environment: ipython\n" }}
{%- endif %}
{%- if builtin_tools is defined %}
    {{- "Tools: " + builtin_tools | reject('equalto', 'code_interpreter') | join(", ") + "\n\n"}}
{%- endif %}
{{- "Cutting Knowledge Date: December 2023\n" }}
{{- "Today Date: " + date_string + "\n\n" }}
{%- if tools is not none and not tools_in_user_message %}
    {{- "You have access to the following functions. To call a function, please respond with JSON for a function call." }}
    {{- 'Respond in the format {"name": function name, "parameters": dictionary of argument name and its value}.' }}
    {{- "Do not use variables.\n\n" }}
    {%- for t in tools %}
        {{- t | tojson(indent=4) }}
        {{- "\n\n" }}
    {%- endfor %}
{%- endif %}
{{- system_message }}
{{- "<|eot_id|>" }}

{#- Custom tools are passed in a user message with some extra guidance #}
{%- if tools_in_user_message and not tools is none %}
    {#- Extract the first user message so we can plug it in here #}
    {%- if messages | length != 0 %}
        {%- set first_user_message = messages[0]['content']|trim %}
        {%- set messages = messages[1:] %}
    {%- else %}
        {{- raise_exception("Cannot put tools in the first user message when there's no first user message!") }}
{%- endif %}
    {{- '<|start_header_id|>user<|end_header_id|>\n\n' -}}
    {{- "Given the following functions, please respond with a JSON for a function call " }}
    {{- "with its proper arguments that best answers the given prompt.\n\n" }}
    {{- 'Respond in the format {"name": function name, "parameters": dictionary of argument name and its value}.' }}
    {{- "Do not use variables.\n\n" }}
    {%- for t in tools %}
        {{- t | tojson(indent=4) }}
        {{- "\n\n" }}
    {%- endfor %}
    {{- first_user_message + "<|eot_id|>"}}
{%- endif %}

{%- for message in messages %}
    {%- if not (message.role == 'ipython' or message.role == 'tool' or 'tool_calls' in message) %}
        {{- '<|start_header_id|>' + message['role'] + '<|end_header_id|>\n\n'+ message['content'] | trim + '<|eot_id|>' }}
    {%- elif 'tool_calls' in message %}
        {%- if not message.tool_calls|length == 1 %}
            {{- raise_exception("This model only supports single tool-calls at once!") }}
        {%- endif %}
        {%- set tool_call = message.tool_calls[0].function %}
        {%- if builtin_tools is defined and tool_call.name in builtin_tools %}
            {{- '<|start_header_id|>assistant<|end_header_id|>\n\n' -}}
            {{- "<|python_tag|>" + tool_call.name + ".call(" }}
            {%- for arg_name, arg_val in tool_call.arguments | items %}
                {{- arg_name + '="' + arg_val + '"' }}
                {%- if not loop.last %}
                    {{- ", " }}
                {%- endif %}
                {%- endfor %}
            {{- ")" }}
        {%- else  %}
            {{- '<|start_header_id|>assistant<|end_header_id|>\n\n' -}}
            {{- '{"name": "' + tool_call.name + '", ' }}
            {{- '"parameters": ' }}
            {{- tool_call.arguments | tojson }}
            {{- "}" }}
        {%- endif %}
        {%- if builtin_tools is defined %}
            {#- This means we're in ipython mode #}
            {{- "<|eom_id|>" }}
        {%- else %}
            {{- "<|eot_id|>" }}
        {%- endif %}
    {%- elif message.role == "tool" or message.role == "ipython" %}
        {{- "<|start_header_id|>ipython<|end_header_id|>\n\n" }}
        {%- if message.content is mapping or message.content is iterable %}
            {{- message.content | tojson }}
        {%- else %}
            {{- message.content }}
        {%- endif %}
        {{- "<|eot_id|>" }}
    {%- endif %}
{%- endfor %}
{%- if add_generation_prompt %}
    {{- '<|start_header_id|>assistant<|end_header_id|>\n\n' }}
{%- endif %}
```

```
template=./meta-llama-Llama-3.1-8B-Instruct.jinja
params: tools=1 tool_choice=AUTO reasoning=0 json_schema=0 add_generation_prompt=1 parallel=0
analysis: user_start=[<|start_header_id|>user<|end_header_id|>] assistant_start=[<|start_header_id|>assistant<|end_header_id|>]
analysis: reasoning=NONE start=[] end=[]
analysis: content=PLAIN start=[] end=[]
analysis: wrapped=0 fkey=0 owrap=1 tolerate=0
analysis: fields name=[name] args=[parameters] id=[] gen=[] order=[name,parameters]
analysis: callid pos=0 prefix=[] suffix=[]
analysis: func prefix=[] suffix=[] close=[] args_start=[] args_end=[] arg_name=[]|[] arg_value=[]|[]
analysis: tool_mode=JSON_NATIVE section_start=[] section_end=[] per_call_start=[] per_call_end=[]
generation_prompt=[<|start_header_id|>assistant<|end_header_id|>\n\n]
```

Expected dump (native oracle):

```text
Sequence(Literal(<|start_header_id|>assistant<|end_header_id|>

), Space, Epsilon, Repetition(Tag(content, Until({)), 0, 1), Repetition(Rule(tool-call, Sequence(Literal(), Space, Choice(Rule(tool-get-weather, Tag(tool, Sequence(Atomic(Tag(tool-open, Literal({))), Space, Repetition(Sequence(Literal("type"), Space, Literal(:), Space, Literal("function"), Space, Literal(,), Space), 0, 1), Literal("name"), Space, Literal(:), Space, Atomic(Sequence(Literal("), Atomic(Tag(tool-name, Literal(get_weather))), Literal("))), Space, Literal(,), Space, Literal("parameters"), Space, Literal(:), Space, Tag(tool-args, Schema(Rule(json-value, Choice(Rule(json-object, Sequence(Literal({), Space, Choice(Literal(}), Sequence(Rule(json-string, Sequence(Literal("), String("), Literal("))), [cycle], Literal(:), [cycle], [cycle], Repetition(Sequence([cycle], Literal(,), [cycle], [cycle], [cycle], [cycle], [cycle], [cycle]), 0, unbounded), [cycle], Literal(}))))), Rule(json-array, Sequence(Literal([), Space, Choice(Literal(]), Sequence([cycle], Repetition(Sequence([cycle], Literal(,), [cycle], [cycle]), 0, unbounded), [cycle], Literal(]))))), [cycle], Rule(json-number, Sequence(Repetition(Literal(-), 0, 1), Choice(Literal(0), Sequence(CharRepeat([1-9], 1, 1), CharRepeat([0-9], 0, unbounded))), Repetition(Sequence(Literal(.), CharRepeat([0-9], 1, unbounded)), 0, 1), Repetition(Sequence(Choice(Literal(e), Literal(E)), Repetition(CharRepeat([+-], 1, 1), 0, 1), [cycle]), 0, 1), Not(CharRepeat([0-9.eE+-], 1, 1)))), Rule(json-bool, Choice(Literal(true), Literal(false))), Rule(json-null, Literal(null)))), {"type":"object","properties":{"arg1":{"type":"string"},"arg2":{"type":"number"}},"required":["arg1"]})), Space, Atomic(Tag(tool-close, Literal(})))))), Rule(tool-get-time, Tag(tool, Sequence(Atomic(Tag(tool-open, Literal({))), Space, Repetition(Sequence(Literal("type"), Space, Literal(:), Space, Literal("function"), Space, Literal(,), Space), 0, 1), Literal("name"), Space, Literal(:), Space, Atomic(Sequence(Literal("), Atomic(Tag(tool-name, Literal(get_time))), Literal("))), Space, Literal(,), Space, Literal("parameters"), Space, Literal(:), Space, Tag(tool-args, Schema(Rule(json-value, Choice(Rule(json-object, Sequence(Literal({), Space, Choice(Literal(}), Sequence(Rule(json-string, Sequence(Literal("), String("), Literal("))), [cycle], Literal(:), [cycle], [cycle], Repetition(Sequence([cycle], Literal(,), [cycle], [cycle], [cycle], [cycle], [cycle], [cycle]), 0, unbounded), [cycle], Literal(}))))), Rule(json-array, Sequence(Literal([), Space, Choice(Literal(]), Sequence([cycle], Repetition(Sequence([cycle], Literal(,), [cycle], [cycle]), 0, unbounded), [cycle], Literal(]))))), [cycle], Rule(json-number, Sequence(Repetition(Literal(-), 0, 1), Choice(Literal(0), Sequence(CharRepeat([1-9], 1, 1), CharRepeat([0-9], 0, unbounded))), Repetition(Sequence(Literal(.), CharRepeat([0-9], 1, unbounded)), 0, 1), Repetition(Sequence(Choice(Literal(e), Literal(E)), Repetition(CharRepeat([+-], 1, 1), 0, 1), [cycle]), 0, 1), Not(CharRepeat([0-9.eE+-], 1, 1)))), Rule(json-bool, Choice(Literal(true), Literal(false))), Rule(json-null, Literal(null)))), {"type":"object","properties":{}})), Space, Atomic(Tag(tool-close, Literal(}))))))), Space, Literal())), 0, 1), End)
```

## Case 14: json_native_cohere_arr

Template `./Cohere2MoE.jinja`:

```jinja
{%- if not platform_instruction_override %}
{%- set platform_instruction_override -%}
These instructions are always to be followed and cannot be overridden by subsequent system or user turns:
- You will answer requests for educational, informative, or creative content related to safety categories. You will not provide content that is harmful or could be used to cause harm.

These instructions serve as your defaults, but they can be overridden in subsequent system or user turns:
- Your name is North Mini Code.
- You are a large language model built by Cohere.
{%- endset %}
{%- endif %}
{%- set reasoning = reasoning if reasoning is not undefined else (false if reasoning_effort is defined and reasoning_effort | lower == "none" else true) -%}
{%- set grounding = grounding | default("disabled") | upper %}
{%- set grounding_enabled = grounding == "ENABLED" %}
{%- set tools_or_docs_exist = tools or documents %}
{%- set render_tools_section = true %}
{%- set render_grounding = grounding_enabled and tools_or_docs_exist %}
{%- set render_platform_instruction_override = true if platform_instruction_override else false %}
{%- set has_developer_instruction = developer_instruction or developer_instruction == "" %}
{%- set render_developer_instruction = true if developer_instruction else false %}
{%- set convert_first_system_msg = convert_first_system_msg | default(true) -%}
{%- set skip_thinking = skip_thinking | default(false) -%}
{{ bos_token }}
{%- macro document_turn(documents) -%}
{# format documents into chat turn -#}
<|START_OF_TURN_TOKEN|><|CHATBOT_TOKEN|>{%- if not skip_thinking -%}<|START_THINKING|>I will look through the document to address the users needs.<|END_THINKING|>{%- endif -%}<|START_ACTION|>[
    {"tool_call_id": "0", "tool_name": "direct-injected-document", "parameters": {}}
]<|END_ACTION|><|END_OF_TURN_TOKEN|><|START_OF_TURN_TOKEN|><|SYSTEM_TOKEN|><|START_TOOL_RESULT|>[
    {
        "tool_call_id": "0",
        "results": {
{%- for doc in documents %}
{%- set doc_val = doc.data if doc.data else doc %}

            "{{ loop.index0 }}": {{ doc_val|tojson }}{% if not loop.last %},
            {%- endif %}
{%- endfor %}

        },
        "is_error": null
    }
]<|END_TOOL_RESULT|><|END_OF_TURN_TOKEN|>{%- endmacro %}
{%- macro tool_call_id_to_int(messages, tool_call_id) %}
{%- if regen_tool_call_ids -%}
    {%- set counter = namespace(value=0) %}
    {%- set tool_call_id_seen = namespace(value=false) %}
    {%- for msg in messages %}
        {%- if msg.tool_calls %}
            {%- for tool_call in msg.tool_calls %}
                {%- if tool_call.id == tool_call_id and not tool_call_id_seen.value -%}
                    {{ counter.value }}
                    {%- set tool_call_id_seen.value = true %}
                {%- endif %}
                {%- set counter.value = counter.value + 1 %}
            {%- endfor %}
        {%- endif %}
    {%- endfor %}
{%- else -%}
    {{ tool_call_id }}
{%- endif -%}
{%- endmacro %}
{%- macro format_tool_message(messages, tool_msg) -%}
{#- format tool message #}{
        "tool_call_id": "{{ tool_call_id_to_int(messages, tool_msg.tool_call_id) }}",
        "results": {
        {%- if tool_msg.content is mapping or tool_msg.content is string %}

            {% if tool_msg.content is string -%}
                {%- set text_wrapper = {"content": tool_msg.content} -%}
            {%- else -%}
                {%- set text_wrapper = tool_msg.content -%}
            {%- endif %}
            "0": {{ text_wrapper|tojson }}
        {%- else %}
            {%- for content in tool_msg.content %}

            "{{ loop.index0 }}": {{ print_tool_content(content) }}{% if not loop.last %},{% endif %}
            {%- endfor %}
        {%- endif %}

        },
        "is_error": null
    }
{%- endmacro -%}
{%- macro print_tool_content(item) %}
{%- if item.type|lower == "text" -%}
{%- set text_wrapper = {"content": item.text} -%}
{{ text_wrapper|tojson }}
{%- elif item.type|lower == "document" and item.document and "data" in item.document -%}
{{ item.document.data|tojson }}
{%- else -%}
{{ item|tojson }}
{%- endif -%}
{%- endmacro %}
{%- macro print_msg(msg) %}
    {%- if msg is string -%}
<|START_TEXT|>{{ msg }}<|END_TEXT|>
    {%- elif msg.content is string -%}
<|START_TEXT|>{{ msg.content }}<|END_TEXT|>
    {%- else %}
        {%- set last_was_text = namespace(value=false) %}
        {%- for content in msg.content %}
            {%- if content.type|lower == "text" -%}
                {%- if not last_was_text.value -%}
                    <|START_TEXT|>
                {%- endif -%}
    {{ content.text }}
                {%- if loop.last -%}
                  <|END_TEXT|>
                {%- endif %}
                {%- set last_was_text.value = true -%}
            {%- else -%}
                {%- if last_was_text.value -%}
                    <|END_TEXT|>
                {%- endif -%}
                {%- set last_was_text.value = false -%}
            {%- endif -%}
            {%- if content.type|lower == "image" -%}
                {%- if content.data -%}
{{ content.data }}
                {%- else -%}
<|IMG_PATCH|>
                {%- endif -%}
            {%- endif -%}
        {%- endfor %}
    {%- endif %}
{%- endmacro %}
{%- macro print_thinking(msg) %}
    {%- if msg.reasoning -%}
{{ msg.reasoning }}
    {%- elif msg.reasoning_content -%}
{{ msg.reasoning_content }}
    {%- elif msg.thinking -%}
{{ msg.thinking }}
    {%- elif msg.content and msg.content[0].thinking -%}
{{ msg.content[0].thinking }}
    {%- endif %}
{%- endmacro %}
{%- if messages and messages[0]['role']|lower == 'system' and not has_developer_instruction and convert_first_system_msg %}{%- set developer_instruction = messages[0] %}{%- set render_developer_instruction = true %}{%- set initial_instruction_message = true %}{% endif %}
{%- set json_object = true if response_format and response_format.type == "json_object" else false %}
{%- set json_schema = (response_format.json_schema or response_format.schema) if response_format %}
{%- set json_mode = json_object or json_schema %}
{%- set tool_idx = namespace(value=0) %}
{%- set tool_ids_seen = namespace(value=[]) %}
{%- set regen_tool_call_ids = regen_tool_call_ids | default(true) -%}
{%- set sent_documents = namespace(value=false) -%}

{%- if render_tools_section or render_platform_instruction_override or render_grounding or json_mode -%}
<|START_OF_TURN_TOKEN|><|SYSTEM_TOKEN|><|START_TEXT|>
{%- elif not render_developer_instruction -%}
<|START_OF_TURN_TOKEN|><|SYSTEM_TOKEN|>
{%- endif %}

{%- set rendered_platform_turn_chunk = false %}

{%- if render_platform_instruction_override -%}
{{ platform_instruction_override }}
{% set rendered_platform_turn_chunk = true %}
{%- else %}
{%- endif %}

{%- if render_grounding -%}
{%- if rendered_platform_turn_chunk %}

{% endif -%}
Note that both your responses and reflections can be grounded. Grounding means you associate pieces of texts (called "spans") with those specific tool results that support them (called "sources"). And you use a pair of tags "<co>" and "</co>" to indicate when a span can be grounded onto a list of sources, listing them out in the closing tag. Sources from the same tool call are grouped together and listed as "{tool_call_id}:[{list of result indices}]", before they are joined together by ",". E.g., "<co>span</co: 0:[1,2],1:[0]>" means that "span" is supported by result 1 and 2 from "tool_call_id=0" as well as result 0 from "tool_call_id=1".
{% set rendered_platform_turn_chunk = true %}
{%- endif %}

{%- if render_tools_section %}
{%- if rendered_platform_turn_chunk %}

{% endif %}
# Available Tools
```json
[
{% if tools_or_docs_exist %}
{%- if documents %}
    {"name": "direct-injected-document", "description": "This is a special tool to directly inject user-uploaded documents into the chat as additional context. DO NOT use this tool by yourself!", "parameters": {"type": "object", "properties": {}, "required": []}, "responses": {"200": {"description": "Successfully returned a list of chunked text snippets from the directly uploaded documents.", "content": {"application/json": {"schema": {"type": "array", "items": {"type": "object", "required": ["url", "snippet"], "properties": {"url": {"type": "string", "description": "The url of the uploaded document."}, "snippet": {"type": "string", "description": "The text snippet for the returned document chunk."}}}}}}}}}
    {%- if tools %},
    {% else %}

    {% endif %}
{%- endif %}
{%- for tool in tools %}
    {"name": "{{ tool['function']['name'] }}", "description": "{{ tool['function']['description'] }}", "parameters": {{ tool['function']['parameters']|tojson }}, "responses": null}
    {%- if not loop.last %},{% endif %}

{% endfor %}
{%- else %}

{% endif %}
]
```
{%- set rendered_platform_turn_chunk = true %}
{%- endif -%}

{%- if json_mode -%}
{%- if rendered_platform_turn_chunk %}


{% endif -%}
When generating JSON objects, do not generate block markers. Generate an object directly without prefixing with ```json. Return only the JSON and nothing else.
    {%- if json_schema %}

Your output should adhere to the following json schema:
{{ json_schema }}
    {%- endif -%}
{%- set rendered_platform_turn_chunk = true %}
{%- endif %}
{%- if rendered_platform_turn_chunk -%}
<|END_TEXT|><|END_OF_TURN_TOKEN|>
{%- elif not render_developer_instruction -%}
<|END_OF_TURN_TOKEN|>
{%- endif %}
{%- if render_developer_instruction -%}
<|START_OF_TURN_TOKEN|><|SYSTEM_TOKEN|>{{ print_msg(developer_instruction) }}<|END_OF_TURN_TOKEN|>
{%- endif %}
{%- for message in messages %}
    {%- set msg_role_downcased = message.role | lower %}
    {%- if msg_role_downcased == 'system' and (not (loop.first and initial_instruction_message)) -%}
<|START_OF_TURN_TOKEN|><|SYSTEM_TOKEN|>{{ print_msg(message) }}<|END_OF_TURN_TOKEN|>
    {%- elif msg_role_downcased == 'user' -%}
<|START_OF_TURN_TOKEN|><|USER_TOKEN|>{{ print_msg(message) }}<|END_OF_TURN_TOKEN|>
        {%- if documents and not sent_documents.value %}{%- set sent_documents.value = true %}{% set tool_idx.value = tool_idx.value + 1 %}{{ document_turn(documents) }}{% endif %}
    {%- elif msg_role_downcased == 'assistant' or msg_role_downcased == 'chatbot' -%}
<|START_OF_TURN_TOKEN|><|CHATBOT_TOKEN|>
        {%- if message.tool_calls %}
            {% if not skip_thinking %}
                {% if message.tool_plan -%}
                    <|START_THINKING|>{{ message.tool_plan }}<|END_THINKING|>
                {%- elif message.reasoning or message.reasoning_content or message.thinking or (message.content and message.content[0].type == "thinking") -%}
                    <|START_THINKING|>{{ print_thinking(message) }}<|END_THINKING|>
                {%- endif %}
            {%- endif %}<|START_ACTION|>[
            {%- for tc in message.tool_calls %}

    {"tool_call_id": "{%- if regen_tool_call_ids -%}{{ tool_idx.value }}{%- else -%}{{ tc.id }}{%- endif -%}", "tool_name": "{{ tc['function']['name'] }}", "parameters": {{ tc['function']['arguments']|tojson }}}{% if not loop.last %},{% endif %}
                {%- set tool_idx.value = tool_idx.value + 1 %}
            {%- endfor %}

]<|END_ACTION|><|END_OF_TURN_TOKEN|>
        {%- else -%}
            {% if (message.reasoning or message.reasoning_content or message.thinking or (message.content and message.content[0].type == "thinking")) and not skip_thinking -%}
                <|START_THINKING|>{{ print_thinking(message) }}<|END_THINKING|>
            {%- endif -%}
            {{ print_msg(message) }}<|END_OF_TURN_TOKEN|>
        {%- endif %}
    {%- elif msg_role_downcased == 'tool' and message.tool_call_id not in tool_ids_seen.value -%}
<|START_OF_TURN_TOKEN|><|SYSTEM_TOKEN|><|START_TOOL_RESULT|>[
    {{ format_tool_message(messages, message) }}
        {%- for msg in messages[loop.index0 + 1:] %}

            {%- if msg.role | lower == 'tool' %},
    {{ format_tool_message(messages, msg) }}
                {%- set tool_ids_seen.value = tool_ids_seen.value + [msg.tool_call_id] %}
            {%- else %}
                {%- break %}
            {%- endif %}
        {%- endfor %}

]<|END_TOOL_RESULT|><|END_OF_TURN_TOKEN|>
    {%- endif %}
{%- endfor %}{%- if add_generation_prompt -%}<|START_OF_TURN_TOKEN|><|CHATBOT_TOKEN|>{% if reasoning %}<|START_THINKING|>{% else %}<|START_THINKING|><|END_THINKING|>{% endif %}{%- endif %}
```

```
template=./Cohere2MoE.jinja
params: tools=1 tool_choice=AUTO reasoning=0 json_schema=0 add_generation_prompt=1 parallel=1
analysis: user_start=[<|START_OF_TURN_TOKEN|><|USER_TOKEN|><|START_TEXT|>] assistant_start=[<|START_OF_TURN_TOKEN|><|CHATBOT_TOKEN|><|START_TEXT|>]
analysis: reasoning=TAG_BASED start=[<|START_THINKING|>] end=[<|END_THINKING|>]
analysis: content=ALWAYS_WRAPPED start=[<|START_TEXT|>] end=[<|END_TEXT|>]
analysis: wrapped=1 fkey=0 owrap=0 tolerate=0
analysis: fields name=[tool_name] args=[parameters] id=[] gen=[tool_call_id] order=[tool_call_id,tool_name,parameters]
analysis: callid pos=0 prefix=[] suffix=[]
analysis: func prefix=[] suffix=[] close=[] args_start=[] args_end=[] arg_name=[]|[] arg_value=[]|[]
analysis: tool_mode=JSON_NATIVE section_start=[<|START_ACTION|>] section_end=[<|END_ACTION|>] per_call_start=[] per_call_end=[]
generation_prompt=[<|START_OF_TURN_TOKEN|><|CHATBOT_TOKEN|><|START_THINKING|>]
```

Expected dump (native oracle):

```text
Sequence(Literal(<|START_OF_TURN_TOKEN|><|CHATBOT_TOKEN|>), Space, Epsilon, Repetition(Sequence(Literal(<|START_TEXT|>), Tag(content, Until(<|END_TEXT|>)), Literal(<|END_TEXT|>)), 0, 1), Repetition(Rule(tool-call, Sequence(Literal(<|START_ACTION|>), Space, Literal([), Space, Choice(Rule(tool-get-weather, Tag(tool, Sequence(Atomic(Tag(tool-open, Literal({))), Space, Epsilon, Repetition(Atomic(Sequence(Literal("tool_call_id"), Space, Literal(:), Space, Choice(Sequence(Literal("), Atomic(Tag(tool-id, String("))), Literal(")), Atomic(Tag(tool-id, Rule(json-number, Sequence(Repetition(Literal(-), 0, 1), Choice(Literal(0), Sequence(CharRepeat([1-9], 1, 1), CharRepeat([0-9], 0, unbounded))), Repetition(Sequence(Literal(.), CharRepeat([0-9], 1, unbounded)), 0, 1), Repetition(Sequence(Choice(Literal(e), Literal(E)), Repetition(CharRepeat([+-], 1, 1), 0, 1), [cycle]), 0, 1), Not(CharRepeat([0-9.eE+-], 1, 1))))))))), 0, 1), Space, Literal(,), Space, Literal("tool_name"), Space, Literal(:), Space, Atomic(Sequence(Literal("), Atomic(Tag(tool-name, Literal(get_weather))), Literal("))), Space, Literal(,), Space, Literal("parameters"), Space, Literal(:), Space, Tag(tool-args, Schema(Rule(json-value, Choice(Rule(json-object, Sequence(Literal({), Space, Choice(Literal(}), Sequence(Rule(json-string, Sequence(Literal("), String("), Literal("))), [cycle], Literal(:), [cycle], [cycle], Repetition(Sequence([cycle], Literal(,), [cycle], [cycle], [cycle], [cycle], [cycle], [cycle]), 0, unbounded), [cycle], Literal(}))))), Rule(json-array, Sequence(Literal([), Space, Choice(Literal(]), Sequence([cycle], Repetition(Sequence([cycle], Literal(,), [cycle], [cycle]), 0, unbounded), [cycle], Literal(]))))), [cycle], Rule(json-number, Sequence(Repetition(Literal(-), 0, 1), Choice(Literal(0), Sequence(CharRepeat([1-9], 1, 1), CharRepeat([0-9], 0, unbounded))), Repetition(Sequence(Literal(.), CharRepeat([0-9], 1, unbounded)), 0, 1), Repetition(Sequence(Choice(Literal(e), Literal(E)), Repetition(CharRepeat([+-], 1, 1), 0, 1), [cycle]), 0, 1), Not(CharRepeat([0-9.eE+-], 1, 1)))), Rule(json-bool, Choice(Literal(true), Literal(false))), Rule(json-null, Literal(null)))), {"type":"object","properties":{"arg1":{"type":"string"},"arg2":{"type":"number"}},"required":["arg1"]})), Space, Atomic(Tag(tool-close, Literal(})))))), Rule(tool-get-time, Tag(tool, Sequence(Atomic(Tag(tool-open, Literal({))), Space, Epsilon, Repetition(Atomic(Sequence(Literal("tool_call_id"), Space, Literal(:), Space, Choice(Sequence(Literal("), Atomic(Tag(tool-id, String("))), Literal(")), Atomic(Tag(tool-id, Rule(json-number, Sequence(Repetition(Literal(-), 0, 1), Choice(Literal(0), Sequence(CharRepeat([1-9], 1, 1), CharRepeat([0-9], 0, unbounded))), Repetition(Sequence(Literal(.), CharRepeat([0-9], 1, unbounded)), 0, 1), Repetition(Sequence(Choice(Literal(e), Literal(E)), Repetition(CharRepeat([+-], 1, 1), 0, 1), [cycle]), 0, 1), Not(CharRepeat([0-9.eE+-], 1, 1))))))))), 0, 1), Space, Literal(,), Space, Literal("tool_name"), Space, Literal(:), Space, Atomic(Sequence(Literal("), Atomic(Tag(tool-name, Literal(get_time))), Literal("))), Space, Literal(,), Space, Literal("parameters"), Space, Literal(:), Space, Tag(tool-args, Schema(Rule(json-value, Choice(Rule(json-object, Sequence(Literal({), Space, Choice(Literal(}), Sequence(Rule(json-string, Sequence(Literal("), String("), Literal("))), [cycle], Literal(:), [cycle], [cycle], Repetition(Sequence([cycle], Literal(,), [cycle], [cycle], [cycle], [cycle], [cycle], [cycle]), 0, unbounded), [cycle], Literal(}))))), Rule(json-array, Sequence(Literal([), Space, Choice(Literal(]), Sequence([cycle], Repetition(Sequence([cycle], Literal(,), [cycle], [cycle]), 0, unbounded), [cycle], Literal(]))))), [cycle], Rule(json-number, Sequence(Repetition(Literal(-), 0, 1), Choice(Literal(0), Sequence(CharRepeat([1-9], 1, 1), CharRepeat([0-9], 0, unbounded))), Repetition(Sequence(Literal(.), CharRepeat([0-9], 1, unbounded)), 0, 1), Repetition(Sequence(Choice(Literal(e), Literal(E)), Repetition(CharRepeat([+-], 1, 1), 0, 1), [cycle]), 0, 1), Not(CharRepeat([0-9.eE+-], 1, 1)))), Rule(json-bool, Choice(Literal(true), Literal(false))), Rule(json-null, Literal(null)))), {"type":"object","properties":{}})), Space, Atomic(Tag(tool-close, Literal(}))))))), Repetition(Sequence(Space, Literal(,), Space, [cycle]), 0, unbounded), Space, Literal(]), Space, Literal(<|END_ACTION|>))), 0, 1), End)
```

## Case 15: tag_tagged_glm_par

Template `./GLM-4.6.jinja`:

```jinja
[gMASK]<sop>
{%- if tools -%}
<|system|>
# Tools

You may call one or more functions to assist with the user query.

You are provided with function signatures within <tools></tools> XML tags:
<tools>
{% for tool in tools %}
{{ tool | tojson(ensure_ascii=False) }}
{% endfor %}
</tools>

For each function call, output the function name and arguments within the following XML format:
<tool_call>{function-name}
<arg_key>{arg-key-1}</arg_key>
<arg_value>{arg-value-1}</arg_value>
<arg_key>{arg-key-2}</arg_key>
<arg_value>{arg-value-2}</arg_value>
...
</tool_call>{%- endif -%}
{%- macro visible_text(content) -%}
    {%- if content is string -%}
        {{- content }}
    {%- elif content is iterable and content is not mapping -%}
        {%- for item in content -%}
            {%- if item is mapping and item.type == 'text' -%}
                {{- item.text }}
            {%- elif item is string -%}
                {{- item }}
            {%- endif -%}
        {%- endfor -%}
    {%- else -%}
        {{- content }}
    {%- endif -%}
{%- endmacro -%}
{%- set ns = namespace(last_user_index=-1) %}
{%- for m in messages %}
    {%- if m.role == 'user' %}
        {% set ns.last_user_index = loop.index0 -%}
    {%- endif %}
{%- endfor %}
{% for m in messages %}
{%- if m.role == 'user' -%}<|user|>
{{ visible_text(m.content) }}
{{- '/nothink' if (enable_thinking is defined and not enable_thinking and not visible_text(m.content).endswith("/nothink")) else '' -}}
{%- elif m.role == 'assistant' -%}
<|assistant|>
{%- set reasoning_content = '' %}
{%- set content = visible_text(m.content) %}
{%- if m.reasoning_content is string %}
    {%- set reasoning_content = m.reasoning_content %}
{%- else %}
    {%- if '</think>' in content %}
        {%- set reasoning_content = content.split('</think>')[0].rstrip('\n').split('<think>')[-1].lstrip('\n') %}
        {%- set content = content.split('</think>')[-1].lstrip('\n') %}
    {%- endif %}
{%- endif %}
{%- if loop.index0 > ns.last_user_index and reasoning_content -%}
{{ '\n<think>' + reasoning_content.strip() +  '</think>'}}
{%- else -%}
{{ '\n<think></think>' }}
{%- endif -%}
{%- if content.strip() -%}
{{ '\n' + content.strip() }}
{%- endif -%}
{% if m.tool_calls %}
{% for tc in m.tool_calls %}
{%- if tc.function %}
    {%- set tc = tc.function %}
{%- endif %}
{{ '\n<tool_call>' + tc.name }}
{% set _args = tc.arguments or {} %}
{% if _args is not mapping %}
    {{ raise_exception("Invalid tool call arguments passed: " + _args | string) }}
{% endif %}
{% for k, v in _args.items() %}
<arg_key>{{ k }}</arg_key>
<arg_value>{{ v | tojson(ensure_ascii=False) if v is not string else v }}</arg_value>
{% endfor %}
</tool_call>{% endfor %}
{% endif %}
{%- elif m.role == 'tool' -%}
{%- if m.content is string -%}
{%- if loop.first or (messages[loop.index0 - 1].role != "tool") %}
    {{- '<|observation|>' }}
{%- endif %}
{{- '\n<tool_response>\n' }}
{{- m.content }}
{{- '\n</tool_response>' }}
{%- else -%}
<|observation|>{% for tr in m.content %}

<tool_response>
{{ tr.output if tr.output is defined else tr }}
</tool_response>{% endfor -%}
{% endif -%}
{%- elif m.role == 'system' -%}
<|system|>
{{ visible_text(m.content) }}
{%- endif -%}
{%- endfor -%}
{%- if add_generation_prompt -%}
    <|assistant|>{{- '\n<think></think>' if (enable_thinking is defined and not enable_thinking) else '' -}}
{%- endif -%}
```

```
template=./GLM-4.6.jinja
params: tools=1 tool_choice=AUTO reasoning=0 json_schema=0 add_generation_prompt=1 parallel=1
analysis: user_start=[<|user|>] assistant_start=[<|assistant|>]
analysis: reasoning=TAG_BASED start=[<think>] end=[</think>\n]
analysis: content=PLAIN start=[] end=[]
analysis: wrapped=0 fkey=0 owrap=0 tolerate=0
analysis: fields name=[name] args=[arguments] id=[] gen=[] order=[]
analysis: callid pos=0 prefix=[] suffix=[]
analysis: func prefix=[] suffix=[\n] close=[] args_start=[] args_end=[] arg_name=[<arg_key>]|[</arg_key>\n] arg_value=[<arg_value>]|[</arg_value>\n]
analysis: tool_mode=TAG_WITH_TAGGED section_start=[] section_end=[] per_call_start=[<tool_call>] per_call_end=[</tool_call>]
generation_prompt=[<|assistant|>\n<think></think>]
```

Expected dump (native oracle):

```text
Sequence(Literal(<|assistant|>
), Space, Epsilon, Repetition(Tag(content, Until(<tool_call>)), 0, 1), Repetition(Rule(tool-call, Sequence(Literal(<tool_call>), Space, Choice(Rule(tool-get-weather, Sequence(Atomic(Tag(tool-open, Sequence(Literal(), Atomic(Tag(tool-name, Literal(get_weather))), Literal(
)))), Epsilon, Space, Epsilon, Rule(tool-get-weather-arg-arg1, Tag(tool-arg, Sequence(Atomic(Tag(tool-arg-open, Sequence(Literal(<arg_key>), Atomic(Tag(tool-arg-name, Literal(arg1))), Literal(</arg_key>
)))), Literal(<arg_value>), Ac(</arg_value>
, Sequence(Tag(tool-arg-string-value, Rule(until-suffix, Until(</arg_value>
))), Atomic(Tag(tool-arg-close, Literal(</arg_value>
)))))))), Repetition(Sequence(Space, Choice(Rule(tool-get-weather-arg-arg2, Tag(tool-arg, Sequence(Atomic(Tag(tool-arg-open, Sequence(Literal(<arg_key>), Atomic(Tag(tool-arg-name, Literal(arg2))), Literal(</arg_key>
)))), Literal(<arg_value>), Tag(tool-arg-value, Schema(Rule(json-value, Choice(Rule(json-object, Sequence(Literal({), Space, Choice(Literal(}), Sequence(Rule(json-string, Sequence(Literal("), String("), Literal("))), [cycle], Literal(:), [cycle], [cycle], Repetition(Sequence([cycle], Literal(,), [cycle], [cycle], [cycle], [cycle], [cycle], [cycle]), 0, unbounded), [cycle], Literal(}))))), Rule(json-array, Sequence(Literal([), Space, Choice(Literal(]), Sequence([cycle], Repetition(Sequence([cycle], Literal(,), [cycle], [cycle]), 0, unbounded), [cycle], Literal(]))))), [cycle], Rule(json-number, Sequence(Repetition(Literal(-), 0, 1), Choice(Literal(0), Sequence(CharRepeat([1-9], 1, 1), CharRepeat([0-9], 0, unbounded))), Repetition(Sequence(Literal(.), CharRepeat([0-9], 1, unbounded)), 0, 1), Repetition(Sequence(Choice(Literal(e), Literal(E)), Repetition(CharRepeat([+-], 1, 1), 0, 1), [cycle]), 0, 1), Not(CharRepeat([0-9.eE+-], 1, 1)))), Rule(json-bool, Choice(Literal(true), Literal(false))), Rule(json-null, Literal(null)))), {"type":"number"})), Atomic(Tag(tool-arg-close, Literal(</arg_value>
)))))))), 0, unbounded), Atomic(Tag(tool-close, And(Literal(</tool_call>)))))), Rule(tool-get-time, Sequence(Atomic(Tag(tool-open, Sequence(Literal(), Atomic(Tag(tool-name, Literal(get_time))), Literal(
)))), Epsilon, Space, Epsilon, Atomic(Tag(tool-close, And(Literal(</tool_call>))))))), Space, Literal(</tool_call>), Repetition(Sequence(Space, [cycle], [cycle], [cycle], [cycle], [cycle]), 0, unbounded), Space)), 0, 1), End)
```

## Case 16: tag_tagged_glm_ser

Template `./GLM-4.6.jinja`:

```jinja
[gMASK]<sop>
{%- if tools -%}
<|system|>
# Tools

You may call one or more functions to assist with the user query.

You are provided with function signatures within <tools></tools> XML tags:
<tools>
{% for tool in tools %}
{{ tool | tojson(ensure_ascii=False) }}
{% endfor %}
</tools>

For each function call, output the function name and arguments within the following XML format:
<tool_call>{function-name}
<arg_key>{arg-key-1}</arg_key>
<arg_value>{arg-value-1}</arg_value>
<arg_key>{arg-key-2}</arg_key>
<arg_value>{arg-value-2}</arg_value>
...
</tool_call>{%- endif -%}
{%- macro visible_text(content) -%}
    {%- if content is string -%}
        {{- content }}
    {%- elif content is iterable and content is not mapping -%}
        {%- for item in content -%}
            {%- if item is mapping and item.type == 'text' -%}
                {{- item.text }}
            {%- elif item is string -%}
                {{- item }}
            {%- endif -%}
        {%- endfor -%}
    {%- else -%}
        {{- content }}
    {%- endif -%}
{%- endmacro -%}
{%- set ns = namespace(last_user_index=-1) %}
{%- for m in messages %}
    {%- if m.role == 'user' %}
        {% set ns.last_user_index = loop.index0 -%}
    {%- endif %}
{%- endfor %}
{% for m in messages %}
{%- if m.role == 'user' -%}<|user|>
{{ visible_text(m.content) }}
{{- '/nothink' if (enable_thinking is defined and not enable_thinking and not visible_text(m.content).endswith("/nothink")) else '' -}}
{%- elif m.role == 'assistant' -%}
<|assistant|>
{%- set reasoning_content = '' %}
{%- set content = visible_text(m.content) %}
{%- if m.reasoning_content is string %}
    {%- set reasoning_content = m.reasoning_content %}
{%- else %}
    {%- if '</think>' in content %}
        {%- set reasoning_content = content.split('</think>')[0].rstrip('\n').split('<think>')[-1].lstrip('\n') %}
        {%- set content = content.split('</think>')[-1].lstrip('\n') %}
    {%- endif %}
{%- endif %}
{%- if loop.index0 > ns.last_user_index and reasoning_content -%}
{{ '\n<think>' + reasoning_content.strip() +  '</think>'}}
{%- else -%}
{{ '\n<think></think>' }}
{%- endif -%}
{%- if content.strip() -%}
{{ '\n' + content.strip() }}
{%- endif -%}
{% if m.tool_calls %}
{% for tc in m.tool_calls %}
{%- if tc.function %}
    {%- set tc = tc.function %}
{%- endif %}
{{ '\n<tool_call>' + tc.name }}
{% set _args = tc.arguments or {} %}
{% if _args is not mapping %}
    {{ raise_exception("Invalid tool call arguments passed: " + _args | string) }}
{% endif %}
{% for k, v in _args.items() %}
<arg_key>{{ k }}</arg_key>
<arg_value>{{ v | tojson(ensure_ascii=False) if v is not string else v }}</arg_value>
{% endfor %}
</tool_call>{% endfor %}
{% endif %}
{%- elif m.role == 'tool' -%}
{%- if m.content is string -%}
{%- if loop.first or (messages[loop.index0 - 1].role != "tool") %}
    {{- '<|observation|>' }}
{%- endif %}
{{- '\n<tool_response>\n' }}
{{- m.content }}
{{- '\n</tool_response>' }}
{%- else -%}
<|observation|>{% for tr in m.content %}

<tool_response>
{{ tr.output if tr.output is defined else tr }}
</tool_response>{% endfor -%}
{% endif -%}
{%- elif m.role == 'system' -%}
<|system|>
{{ visible_text(m.content) }}
{%- endif -%}
{%- endfor -%}
{%- if add_generation_prompt -%}
    <|assistant|>{{- '\n<think></think>' if (enable_thinking is defined and not enable_thinking) else '' -}}
{%- endif -%}
```

```
template=./GLM-4.6.jinja
params: tools=1 tool_choice=AUTO reasoning=0 json_schema=0 add_generation_prompt=1 parallel=0
analysis: user_start=[<|user|>] assistant_start=[<|assistant|>]
analysis: reasoning=TAG_BASED start=[<think>] end=[</think>\n]
analysis: content=PLAIN start=[] end=[]
analysis: wrapped=0 fkey=0 owrap=0 tolerate=0
analysis: fields name=[name] args=[arguments] id=[] gen=[] order=[]
analysis: callid pos=0 prefix=[] suffix=[]
analysis: func prefix=[] suffix=[\n] close=[] args_start=[] args_end=[] arg_name=[<arg_key>]|[</arg_key>\n] arg_value=[<arg_value>]|[</arg_value>\n]
analysis: tool_mode=TAG_WITH_TAGGED section_start=[] section_end=[] per_call_start=[<tool_call>] per_call_end=[</tool_call>]
generation_prompt=[<|assistant|>\n<think></think>]
```

Expected dump (native oracle):

```text
Sequence(Literal(<|assistant|>
), Space, Epsilon, Repetition(Tag(content, Until(<tool_call>)), 0, 1), Repetition(Rule(tool-call, Sequence(Literal(<tool_call>), Space, Choice(Rule(tool-get-weather, Sequence(Atomic(Tag(tool-open, Sequence(Literal(), Atomic(Tag(tool-name, Literal(get_weather))), Literal(
)))), Epsilon, Space, Epsilon, Rule(tool-get-weather-arg-arg1, Tag(tool-arg, Sequence(Atomic(Tag(tool-arg-open, Sequence(Literal(<arg_key>), Atomic(Tag(tool-arg-name, Literal(arg1))), Literal(</arg_key>
)))), Literal(<arg_value>), Ac(</arg_value>
, Sequence(Tag(tool-arg-string-value, Rule(until-suffix, Until(</arg_value>
))), Atomic(Tag(tool-arg-close, Literal(</arg_value>
)))))))), Repetition(Sequence(Space, Choice(Rule(tool-get-weather-arg-arg2, Tag(tool-arg, Sequence(Atomic(Tag(tool-arg-open, Sequence(Literal(<arg_key>), Atomic(Tag(tool-arg-name, Literal(arg2))), Literal(</arg_key>
)))), Literal(<arg_value>), Tag(tool-arg-value, Schema(Rule(json-value, Choice(Rule(json-object, Sequence(Literal({), Space, Choice(Literal(}), Sequence(Rule(json-string, Sequence(Literal("), String("), Literal("))), [cycle], Literal(:), [cycle], [cycle], Repetition(Sequence([cycle], Literal(,), [cycle], [cycle], [cycle], [cycle], [cycle], [cycle]), 0, unbounded), [cycle], Literal(}))))), Rule(json-array, Sequence(Literal([), Space, Choice(Literal(]), Sequence([cycle], Repetition(Sequence([cycle], Literal(,), [cycle], [cycle]), 0, unbounded), [cycle], Literal(]))))), [cycle], Rule(json-number, Sequence(Repetition(Literal(-), 0, 1), Choice(Literal(0), Sequence(CharRepeat([1-9], 1, 1), CharRepeat([0-9], 0, unbounded))), Repetition(Sequence(Literal(.), CharRepeat([0-9], 1, unbounded)), 0, 1), Repetition(Sequence(Choice(Literal(e), Literal(E)), Repetition(CharRepeat([+-], 1, 1), 0, 1), [cycle]), 0, 1), Not(CharRepeat([0-9.eE+-], 1, 1)))), Rule(json-bool, Choice(Literal(true), Literal(false))), Rule(json-null, Literal(null)))), {"type":"number"})), Atomic(Tag(tool-arg-close, Literal(</arg_value>
)))))))), 0, unbounded), Atomic(Tag(tool-close, And(Literal(</tool_call>)))))), Rule(tool-get-time, Sequence(Atomic(Tag(tool-open, Sequence(Literal(), Atomic(Tag(tool-name, Literal(get_time))), Literal(
)))), Epsilon, Space, Epsilon, Atomic(Tag(tool-close, And(Literal(</tool_call>))))))), Space, Literal(</tool_call>), Space)), 0, 1), End)
```

## Case 17: tag_tagged_minimax_sec

Template `./MiniMax-M2.jinja`:

```jinja
{# ----------‑‑‑ special token variables ‑‑‑---------- #}
{%- set toolcall_begin_token   = '<minimax:tool_call>'         -%}
{%- set toolcall_end_token     = '</minimax:tool_call>'        -%}
{#- Tool Rendering Functions ============================================== -#}
{%- macro render_tool_namespace(namespace_name, tool_list) -%}
{%- for tool in tool_list -%}
<tool>{{ tool.function | tojson(ensure_ascii=False) }}</tool>
{% endfor -%}
{%- endmacro -%}
{%- macro visible_text(content) -%}
    {%- if content is string -%}
        {{ content }}
    {%- elif content is iterable and content is not mapping -%}
        {%- for item in content -%}
            {%- if item is mapping and item.type == 'text' -%}
                {{- item.text }}
            {%- elif item is string -%}
                {{- item }}
            {%- endif -%}
        {%- endfor -%}
    {%- else -%}
        {{- content }}
    {%- endif -%}
{%- endmacro -%}
{#- System Message Construction ============================================ -#}
{%- macro build_system_message(system_message) -%}
    {%- if system_message and system_message.content -%}
        {{- visible_text(system_message.content) }}
    {%- else -%}
        {%- if model_identity is not defined -%}
            {%- set model_identity = "You are a helpful assistant." -%}
        {%- endif -%}
        {{- model_identity }}
    {%- endif -%}
    
    {#- Handle current_date -#}
    {%- if system_message and system_message.current_date -%}
        {{- '\n' ~ 'Current date: ' + system_message.current_date }}
    {%- endif -%}
    {#- Handle current_location -#}
    {%- if system_message and system_message.current_location -%}
        {{- '\n' ~ 'Current location: ' + system_message.current_location }}
    {%- endif -%}
{%- endmacro -%}
{#- Main Template Logic ================================================= -#}
{#- Extract system message (only first message if it's system) -#}
{%- set system_message = none -%}
{%- set conversation_messages = messages -%}
{%- if messages and messages[0].role == "system" -%}
    {%- set system_message = messages[0] -%}
    {%- set conversation_messages = messages[1:] -%}
{%- endif -%}
{#- Get the last user message turn, for interleved thinking -#}
{%- set ns = namespace(last_user_index=-1) %}
{% for m in conversation_messages %}
    {%- if m.role == 'user' %}
        {% set ns.last_user_index = loop.index0 -%}
    {%- endif %}
{%- endfor %}
{#- Render system message -#}
{{- ']~!b[' ~ ']~b]system' ~ '\n' }}
{{- build_system_message(system_message) }}
{#- Render tools if available -#}
{%- if tools -%}
    {{- '\n\n' ~ '# Tools' ~ '\n' ~ 'You may call one or more tools to assist with the user query.\nHere are the tools available in JSONSchema format:' ~ '\n' }}
    {{- '\n' ~ '<tools>' ~ '\n' }}
    {{- render_tool_namespace("functions", tools) }}
    {{- '</tools>' ~ '\n\n' }}
{{- 'When making tool calls, use XML format to invoke tools and pass parameters:' ~ '\n' }}
{{- '\n' ~ toolcall_begin_token }}
<invoke name="tool-name-1">
<parameter name="param-key-1">param-value-1</parameter>
<parameter name="param-key-2">param-value-2</parameter>
...
</invoke>
{{- '\n' ~ toolcall_end_token }}
{%- endif -%}
{{- '[e~[\n' }}

{#- Render messages -#}
{%- set last_tool_call = namespace(name=none) -%}
{%- for message in conversation_messages -%}
    {%- if message.role == 'assistant' -%}
        {#- Only render reasoning_content if no user message follows -#}
        {{- ']~b]ai' ~ '\n' }}

        {%- set reasoning_content = '' %}
        {%- set content = visible_text(message.content) %}
        {%- if message.reasoning_content is string %}
            {%- set reasoning_content = message.reasoning_content %}
        {%- else %}
            {%- if '</think>' in content %}
                {%- set reasoning_content = content.split('</think>')[0].strip('\n').split('<think>')[-1].strip('\n') %}
                {%- set content = content.split('</think>')[-1].strip('\n') %}
            {%- endif %}
        {%- endif %}
        {%- if reasoning_content and loop.index0 > ns.last_user_index -%}
            {{- '<think>' ~ '\n' ~ reasoning_content ~ '\n' ~ '</think>' ~ '\n\n' }}
        {%- endif -%}
        {%- if content -%}
            {{- content }}
        {%- endif -%}
        {%- if message.tool_calls -%}
            {{- '\n' ~ toolcall_begin_token ~ '\n' }}

            {%- for tool_call in message.tool_calls -%}
                {%- if tool_call.function %}
                    {%- set tool_call = tool_call.function %}
                {%- endif %}
                {{- '<invoke name="' + tool_call.name + '">' }}
                {% set _args = tool_call.arguments %}
                {%- for k, v in _args.items() %}
                {{- '<parameter name="' + k + '">' }}
                {{- v | tojson(ensure_ascii=False) if v is not string else v }}
                {{- '</parameter>' }}
                {% endfor %}
                {{- '</invoke>' ~ '\n' }}
            {%- endfor -%}
            
            {{- toolcall_end_token}}
            {%- set last_tool_call.name = message.tool_calls[-1].function.name -%}
        {%- else -%}
            {%- set last_tool_call.name = none -%}
        {%- endif -%}
        {{- '[e~[' ~ '\n' }}
        
    {%- elif message.role == 'tool' -%}
    {%- if last_tool_call.name is none -%}
        {{- raise_exception("Message has tool role, but there was no previous assistant message with a tool call!") }}
    {%- endif -%}
    {%- if loop.first or (conversation_messages[loop.index0 - 1].role != 'tool') -%}
        {{- ']~b]tool' }}
    {%- endif -%}
    {%- if message.content is string -%}
        {{- '\n<response>' }}
        {{- message.content }}
        {{- '</response>' }}
    {%- else -%}
        {%- for tr in message.content -%}
            {{- '\n<response>' }}
            {{- tr.output if tr.output is defined else (tr.text if tr.type == 'text' and tr.text is defined else tr) }}
            {{- '\n</response>' }}
        {%- endfor -%}
    {%- endif -%}
    {%- if loop.last or (conversation_messages[loop.index0 + 1].role != 'tool') -%}
        {{- '[e~[\n' -}}
    {%- endif -%}
        
    {%- elif message.role == 'user' -%}
        {{- ']~b]user' ~ '\n' }}
        {{- visible_text(message.content) }}
        {{- '[e~[' ~ '\n' }}
    {%- endif -%}
{%- endfor -%}

{#- Generation prompt -#}
{%- if add_generation_prompt -%}
{{- ']~b]ai' ~ '\n' ~ '<think>' ~ '\n' }}
{%- endif -%}
```

```
template=./MiniMax-M2.jinja
params: tools=1 tool_choice=AUTO reasoning=0 json_schema=0 add_generation_prompt=1 parallel=1
analysis: user_start=[]~b]user] assistant_start=[]~b]ai]
analysis: reasoning=TAG_BASED start=[<think>\n] end=[\n</think>\n\n]
analysis: content=PLAIN start=[] end=[]
analysis: wrapped=0 fkey=0 owrap=0 tolerate=0
analysis: fields name=[name] args=[arguments] id=[] gen=[] order=[]
analysis: callid pos=0 prefix=[] suffix=[]
analysis: func prefix=[<invoke name="] suffix=[">\n] close=[</invoke>\n] args_start=[] args_end=[] arg_name=[<parameter name="]|[] arg_value=[">]|[</parameter>\n]
analysis: tool_mode=TAG_WITH_TAGGED section_start=[<minimax:tool_call>\n] section_end=[</minimax:tool_call>] per_call_start=[] per_call_end=[]
generation_prompt=[]~b]ai\n<think>\n]
```

Expected dump (native oracle):

```text
Sequence(Literal(]~b]ai
), Space, Epsilon, Repetition(Tag(content, Until(<minimax:tool_call>
)), 0, 1), Repetition(Rule(tool-call, Sequence(Literal(<minimax:tool_call>
), Space, Choice(Rule(tool-get-weather, Sequence(Atomic(Tag(tool-open, Sequence(Literal(<invoke name="), Atomic(Tag(tool-name, Literal(get_weather))), Literal(">
)))), Epsilon, Space, Epsilon, Rule(tool-get-weather-arg-arg1, Tag(tool-arg, Sequence(Atomic(Tag(tool-arg-open, Sequence(Literal(<parameter name="), Atomic(Tag(tool-arg-name, Literal(arg1))), Literal()))), Literal(">), Ac(</parameter>
, Sequence(Tag(tool-arg-string-value, Rule(until-suffix, Until(</parameter>
))), Atomic(Tag(tool-arg-close, Literal(</parameter>
)))))))), Repetition(Sequence(Space, Choice(Rule(tool-get-weather-arg-arg2, Tag(tool-arg, Sequence(Atomic(Tag(tool-arg-open, Sequence(Literal(<parameter name="), Atomic(Tag(tool-arg-name, Literal(arg2))), Literal()))), Literal(">), Tag(tool-arg-value, Schema(Rule(json-value, Choice(Rule(json-object, Sequence(Literal({), Space, Choice(Literal(}), Sequence(Rule(json-string, Sequence(Literal("), String("), Literal("))), [cycle], Literal(:), [cycle], [cycle], Repetition(Sequence([cycle], Literal(,), [cycle], [cycle], [cycle], [cycle], [cycle], [cycle]), 0, unbounded), [cycle], Literal(}))))), Rule(json-array, Sequence(Literal([), Space, Choice(Literal(]), Sequence([cycle], Repetition(Sequence([cycle], Literal(,), [cycle], [cycle]), 0, unbounded), [cycle], Literal(]))))), [cycle], Rule(json-number, Sequence(Repetition(Literal(-), 0, 1), Choice(Literal(0), Sequence(CharRepeat([1-9], 1, 1), CharRepeat([0-9], 0, unbounded))), Repetition(Sequence(Literal(.), CharRepeat([0-9], 1, unbounded)), 0, 1), Repetition(Sequence(Choice(Literal(e), Literal(E)), Repetition(CharRepeat([+-], 1, 1), 0, 1), [cycle]), 0, 1), Not(CharRepeat([0-9.eE+-], 1, 1)))), Rule(json-bool, Choice(Literal(true), Literal(false))), Rule(json-null, Literal(null)))), {"type":"number"})), Atomic(Tag(tool-arg-close, Literal(</parameter>
)))))))), 0, unbounded), Space, Atomic(Tag(tool-close, Literal(</invoke>
))))), Rule(tool-get-time, Sequence(Atomic(Tag(tool-open, Sequence(Literal(<invoke name="), Atomic(Tag(tool-name, Literal(get_time))), Literal(">
)))), Epsilon, Space, Epsilon, Space, Atomic(Tag(tool-close, Literal(</invoke>
)))))), Repetition(Sequence(Literal(, ), [cycle]), 0, unbounded), Space, Literal(</minimax:tool_call>))), 0, 1), End)
```

## Case 18: tag_tagged_laguna_tol

Template `./poolside-Laguna-S-2.1.jinja`:

```jinja
{#- Iteration on laguna_glm_thinking_v8/chat_template.jinja -#}
{#- No formatting instructions -#}
{{- "〈|EOS|〉" -}}
{%- set enable_thinking = enable_thinking | default(true) -%}
{%- set add_generation_prompt = add_generation_prompt | default(false) -%}
{%- set preserve_thinking = preserve_thinking | default(false) -%}

{#- ───── header (system message) ───── -#}
{#- A caller-supplied system message with empty content opts out of the default below, producing no <system> block — used to train without a system message. -#}
{%- set system_message = "You are a helpful, conversationally-fluent assistant made by Poolside. You are here to be helpful to users through natural language conversations." -%}
{%- if messages and messages[0].role == "system" -%}
  {%- set system_message = messages[0].content -%}
  {%- set messages = messages[1:] -%}
{%- endif -%}

{%- set has_sys = system_message and system_message.strip() -%}
{%- if has_sys or tools or enable_thinking -%}
  {{- "<system>" -}}

  {%- if has_sys -%}
    {{- system_message.rstrip() -}}
    {%- if tools -%}{{- "\n\n" -}}{%- endif -%}
  {%- endif -%}

  {%- if tools -%}
    {{- "### Tools\n\n" -}}
    {{- "You may call functions to assist with the user query.\n" -}}
    {{- "All available function signatures are listed below:\n" -}}
    {{- "<available_tools>\n" -}}
    {%- for tool in tools -%}
      {{- (tool | tojson) ~ "\n" -}}
    {%- endfor -%}
    {{- "</available_tools>" -}}
  {%- endif -%}

  {{- "</system>\n" -}}
{%- endif -%}

{#- ───── main loop ───── -#}
{%- for message in messages -%}
  {%- set content = message.content if message.content is string else "" -%}
  {%- if message.role == "user" -%}
    {{- "<user>" + content + "</user>\n" -}}
  {%- elif message.role == "assistant" -%}
    {%- generation -%}
      {{- "<assistant>" -}}
      {#- Extract reasoning content from message.reasoning (vLLM field name) or message.reasoning_content -#}
      {%- set reasoning_content = '' -%}
      {%- if message.reasoning is string -%}
        {%- set reasoning_content = message.reasoning -%}
      {%- elif message.reasoning_content is string -%}
        {%- set reasoning_content = message.reasoning_content -%}
      {%- endif -%}
      {#- Display reasoning content for all messages if enable_thinking -#}
      {%- if enable_thinking or preserve_thinking -%}
        {{- '<think>' + reasoning_content + '</think>' -}}
      {%- else -%}
        {{- '</think>' -}}
      {%- endif -%}
      {#- Display main content (trailing newline only when no tool_calls follow) -#}
      {%- if content -%}
        {{- content -}}
      {%- endif -%}
      {%- if message.tool_calls -%}
        {%- for tool_call in message.tool_calls -%}
          {%- set function_data = tool_call.function -%}
          {{- '<tool_call>' + function_data.name -}}
          {%- set _args = function_data.arguments -%}
          {%- for k, v in _args.items() -%}
            {{- "<arg_key>" ~ k ~ "</arg_key>" -}}
            {{- "<arg_value>" -}}{{- v | tojson(ensure_ascii=False) if v is not string else v -}}{{- "</arg_value>" -}}
          {%- endfor -%}
          {{- "</tool_call>" -}}
        {%- endfor -%}
      {%- endif -%}
      {{- "</assistant>\n" -}}
    {%- endgeneration -%}
  {%- elif message.role == "tool" -%}
    {{- "<tool_response>" + content + "</tool_response>\n" -}}
  {%- elif message.role == "system" -%}
    {#- Render additional system messages (the first one, if any, is handled separately in the header and was sliced off above) -#}
    {{- "<system>" + content + "</system>\n" -}}
  {%- endif -%}
{%- endfor -%}
{#- ───── generation prompt ───── -#}
{%- if add_generation_prompt -%}
  {{- "<assistant>" -}}
  {#- ───── Include reasoning mode directive ───── -#}
  {%- if enable_thinking -%}
    {{- '<think>' -}}
  {%- else -%}
    {{- '</think>' -}}
  {%- endif -%}
{%- endif -%}
```

```
template=./poolside-Laguna-S-2.1.jinja
params: tools=1 tool_choice=AUTO reasoning=0 json_schema=0 add_generation_prompt=1 parallel=1
analysis: user_start=[<user>] assistant_start=[<assistant>]
analysis: reasoning=TAG_BASED start=[<think>] end=[</think>]
analysis: content=PLAIN start=[] end=[]
analysis: wrapped=0 fkey=0 owrap=0 tolerate=1
analysis: fields name=[name] args=[arguments] id=[] gen=[] order=[]
analysis: callid pos=0 prefix=[] suffix=[]
analysis: func prefix=[] suffix=[] close=[] args_start=[] args_end=[] arg_name=[<arg_key>]|[</arg_key>] arg_value=[<arg_value>]|[</arg_value>]
analysis: tool_mode=TAG_WITH_TAGGED section_start=[] section_end=[] per_call_start=[<tool_call>] per_call_end=[</tool_call>]
generation_prompt=[<assistant></think>]
```

Expected dump (native oracle):

```text
Sequence(Literal(<assistant></think>), Space, Epsilon, Repetition(Tag(content, Until(<tool_call>)), 0, 1), Repetition(Rule(tool-call, Sequence(Literal(<tool_call>), Space, Choice(Rule(tool-get-weather, Sequence(Atomic(Sequence(Atomic(Tag(tool-open, Sequence(Literal(), Atomic(Tag(tool-name, Literal(get_weather))), Literal()))), Epsilon, Space, And(Literal(<arg_key>)))), Epsilon, Rule(tool-get-weather-arg-arg1, Tag(tool-arg, Sequence(Atomic(Tag(tool-arg-open, Sequence(Literal(<arg_key>), Atomic(Tag(tool-arg-name, Literal(arg1))), Literal(</arg_key>)))), Literal(<arg_value>), Ac(</arg_value>, Sequence(Tag(tool-arg-string-value, Rule(until-suffix, Until(</arg_value>))), Atomic(Tag(tool-arg-close, Literal(</arg_value>)))))))), Repetition(Sequence(Space, Choice(Rule(tool-get-weather-arg-arg2, Tag(tool-arg, Sequence(Atomic(Tag(tool-arg-open, Sequence(Literal(<arg_key>), Atomic(Tag(tool-arg-name, Literal(arg2))), Literal(</arg_key>)))), Literal(<arg_value>), Tag(tool-arg-value, Schema(Rule(json-value, Choice(Rule(json-object, Sequence(Literal({), Space, Choice(Literal(}), Sequence(Rule(json-string, Sequence(Literal("), String("), Literal("))), [cycle], Literal(:), [cycle], [cycle], Repetition(Sequence([cycle], Literal(,), [cycle], [cycle], [cycle], [cycle], [cycle], [cycle]), 0, unbounded), [cycle], Literal(}))))), Rule(json-array, Sequence(Literal([), Space, Choice(Literal(]), Sequence([cycle], Repetition(Sequence([cycle], Literal(,), [cycle], [cycle]), 0, unbounded), [cycle], Literal(]))))), [cycle], Rule(json-number, Sequence(Repetition(Literal(-), 0, 1), Choice(Literal(0), Sequence(CharRepeat([1-9], 1, 1), CharRepeat([0-9], 0, unbounded))), Repetition(Sequence(Literal(.), CharRepeat([0-9], 1, unbounded)), 0, 1), Repetition(Sequence(Choice(Literal(e), Literal(E)), Repetition(CharRepeat([+-], 1, 1), 0, 1), [cycle]), 0, 1), Not(CharRepeat([0-9.eE+-], 1, 1)))), Rule(json-bool, Choice(Literal(true), Literal(false))), Rule(json-null, Literal(null)))), {"type":"number"})), Atomic(Tag(tool-arg-close, Literal(</arg_value>)))))))), 0, unbounded), Atomic(Tag(tool-close, And(Sequence(Space, Literal(</tool_call>))))))), Rule(tool-get-time, Atomic(Sequence(Atomic(Tag(tool-open, Sequence(Literal(), Atomic(Tag(tool-name, Literal(get_time))), Literal()))), Epsilon, Space, Epsilon, Atomic(Tag(tool-close, And(Sequence(Space, Literal(</tool_call>))))))))), Space, Literal(</tool_call>), Repetition(Sequence(Space, [cycle], [cycle], [cycle], [cycle], [cycle]), 0, unbounded), Space)), 0, 1), End)
```

## Case 19: tag_json_callid_par

Template `./Mistral-Small-3.2-24B-Instruct-2506.jinja`:

```jinja
{%- set today = strftime_now("%Y-%m-%d") %}
{%- set default_system_message = "You are Mistral Small 3, a Large Language Model (LLM) created by Mistral AI, a French startup headquartered in Paris.\nYour knowledge base was last updated on 2023-10-01. The current date is " + today + ".\n\nWhen you're not sure about some information or when the user's request requires up-to-date or specific data, you must use the available tools to fetch the information. Do not hesitate to use tools whenever they can provide a more accurate or complete response. If no relevant tools are available, then clearly state that you don't have the information and avoid making up anything.

If the user's question is not clear, ambiguous, or does not provide enough context for you to accurately answer the question, you do not try to answer it right away and you rather ask the user to clarify their request (e.g. \"What are some good restaurants around me?\" => \"Where are you?\" or \"When is the next flight to Tokyo\" => \"Where do you travel from?\").
You are always very attentive to dates, and when asked about information at specific dates, you discard information that is at another date.
You follow these instructions in all languages, and always respond to the user in the language they use or request.
Next sections describe the capabilities that you have.

# WEB BROWSING INSTRUCTIONS

You cannot perform any web search or access internet to open URLs, links etc. If it seems like the user is expecting you to do so, you clarify the situation and ask the user to copy paste the text directly in the chat.

# MULTI-MODAL INSTRUCTIONS

You have the ability to read images, but you cannot generate images. You also cannot transcribe audio files or videos.
You cannot read nor transcribe audio files or videos.

# TOOL CALLING INSTRUCTIONS

You may have access to tools that you can use to fetch information or perform actions. You must use these tools in the following situations:

1. When the request requires up-to-date information.
2. When the request requires specific data that you do not have in your knowledge base.
3. When the request involves actions that you cannot perform without tools.

Always prioritize using tools to provide the most accurate and helpful response. If tools are not available, inform the user that you cannot perform the requested action at the moment." %}

{{- bos_token }}

{%- set system_prompt = default_system_message %}
{%- set loop_messages = messages %}

{%- if not tools is defined %}
    {%- set tools = none %}
{%- endif %}

{%- if messages|length > 0 and messages[0]['role'] == 'system' %}
    {%- if messages[0]['content'] is string %}
        {%- set system_prompt = messages[0]['content'] %}
    {%- else %}
        {%- set system_prompt = messages[0]['content'][0]['text'] %}
    {%- endif %}
    {%- set loop_messages = messages[1:] %}
{%- endif %}

{%- set user_messages = loop_messages | selectattr("role", "equalto", "user") | list %}

{%- set ns = namespace(index=0) %}
{%- for message in loop_messages %}
    {%- if not (message.role == "tool" or (message.get('tool_calls'))) %}
        {%- if (message["role"] == "user") != (ns.index % 2 == 0) %}
            {{- raise_exception("After the optional system message, conversation roles must alternate user/assistant/user/assistant/...") }}
        {%- endif %}
        {%- set ns.index = ns.index + 1 %}
    {%- endif %}
{%- endfor %}

{{- '[SYSTEM_PROMPT]' + system_prompt + '[/SYSTEM_PROMPT]' }}

{%- for message in loop_messages %}
    {%- if message['role'] == 'system' %}
        {%- if message['content'] is string %}
            {{- '[SYSTEM_PROMPT]' + message['content'] + '[/SYSTEM_PROMPT]' }}
        {%- else %}
            {{- '[SYSTEM_PROMPT]' + message['content'][0]['text'] + '[/SYSTEM_PROMPT]' }}
        {%- endif %}
    {%- elif message['role'] == 'user' %}
        {%- if tools is not none and (message == user_messages[-1]) %}
            {{- '[AVAILABLE_TOOLS]' + tools|tojson + '[/AVAILABLE_TOOLS]' }}
        {%- endif %}
        {{- '[INST]' }}
        {%- if message['content'] is string %}
            {{- message['content'] }}
        {%- else %}
            {%- for block in message['content'] %}
                {%- if block['type'] == 'text' %}
                    {{- block['text'] }}
                {%- elif block['type'] in ['image', 'image_url'] %}
                    {{- '[IMG]' }}
                {%- else %}
                    {{- raise_exception('Only text and image blocks are supported in message content!') }}
                {%- endif %}
            {%- endfor %}
        {%- endif %}
        {{- '[/INST]' }}
    {%- elif message['role'] == 'assistant' %}
        {%- if message.get('tool_calls') %}
            {%- for tool_call in message.tool_calls %}
                {{- '[TOOL_CALLS]' + tool_call.function.name }}
                {%- if not tool_call.id is defined or tool_call.id is not string or tool_call.id|length != 9 %}
                    {{- raise_exception("Tool call IDs should be alphanumeric strings with length 9!") }}
                {%- endif %}
                {{- '[CALL_ID]' + tool_call.id }}
                {{- '[ARGS]' + tool_call['function']['arguments']|tojson }}
            {%- endfor %}
            {{- eos_token }}
        {%- elif message['content'] is string %}
            {{- message['content'] + eos_token }}
        {%- else %}
            {%- for block in message['content'] %}
                {%- if block['type'] == 'text' %}
                    {{- block['text'] }}
                {%- elif block['type'] in ['image', 'image_url'] %}
                    {{- '[IMG]' }}
                {%- else %}
                    {{- raise_exception('Only text and image blocks are supported in assistant content!') }}
                {%- endif %}
            {%- endfor %}
            {{- eos_token }}
        {%- endif %}
    {%- elif message['role'] == 'tool_results' or message['role'] == 'tool' %}
        {%- if message.content is defined and message.content.content is defined %}
            {%- set content = message.content.content %}
        {%- else %}
            {%- set content = message.content %}
        {%- endif %}
        {%- if not message.tool_call_id is defined or message.tool_call_id is not string or message['tool_call_id']|length != 9 %}
            {{- raise_exception("Tool call IDs should be alphanumeric strings with length 9!") }}
        {%- endif %}
        {{- '[TOOL_RESULTS]' + message.tool_call_id + '[TOOL_CONTENT]' + content|string + '[/TOOL_RESULTS]' }}
    {%- else %}
        {{- raise_exception('Only system, user, assistant, and tool roles are supported!') }}
    {%- endif %}
{%- endfor %}
```

```
template=./Mistral-Small-3.2-24B-Instruct-2506.jinja
params: tools=1 tool_choice=AUTO reasoning=0 json_schema=0 add_generation_prompt=1 parallel=1
analysis: user_start=[[INST]] assistant_start=[]
analysis: reasoning=NONE start=[] end=[]
analysis: content=PLAIN start=[] end=[]
analysis: wrapped=0 fkey=0 owrap=0 tolerate=0
analysis: fields name=[name] args=[arguments] id=[] gen=[] order=[]
analysis: callid pos=2 prefix=[[CALL_ID]] suffix=[[ARGS]]
analysis: func prefix=[] suffix=[] close=[] args_start=[] args_end=[] arg_name=[]|[] arg_value=[]|[]
analysis: tool_mode=TAG_WITH_JSON section_start=[] section_end=[] per_call_start=[[TOOL_CALLS]] per_call_end=[]
generation_prompt=[]
```

Expected dump (native oracle):

```text
Sequence(Epsilon, Space, Epsilon, Repetition(Tag(content, Until([TOOL_CALLS])), 0, 1), Repetition(Rule(tool-call, Sequence(Literal([TOOL_CALLS]), Choice(Rule(tool-get-weather, Sequence(Atomic(Sequence(Atomic(Tag(tool-open, Sequence(Literal(), Atomic(Tag(tool-name, Literal(get_weather))), Literal()))), Repetition(Sequence(Literal([CALL_ID]), Atomic(Tag(tool-id, Until([ARGS])))), 0, 1), Literal([ARGS]))), Space, Tag(tool-args, Schema(Rule(json-value, Choice(Rule(json-object, Sequence(Literal({), Space, Choice(Literal(}), Sequence(Rule(json-string, Sequence(Literal("), String("), Literal("))), [cycle], Literal(:), [cycle], [cycle], Repetition(Sequence([cycle], Literal(,), [cycle], [cycle], [cycle], [cycle], [cycle], [cycle]), 0, unbounded), [cycle], Literal(}))))), Rule(json-array, Sequence(Literal([), Space, Choice(Literal(]), Sequence([cycle], Repetition(Sequence([cycle], Literal(,), [cycle], [cycle]), 0, unbounded), [cycle], Literal(]))))), [cycle], Rule(json-number, Sequence(Repetition(Literal(-), 0, 1), Choice(Literal(0), Sequence(CharRepeat([1-9], 1, 1), CharRepeat([0-9], 0, unbounded))), Repetition(Sequence(Literal(.), CharRepeat([0-9], 1, unbounded)), 0, 1), Repetition(Sequence(Choice(Literal(e), Literal(E)), Repetition(CharRepeat([+-], 1, 1), 0, 1), [cycle]), 0, 1), Not(CharRepeat([0-9.eE+-], 1, 1)))), Rule(json-bool, Choice(Literal(true), Literal(false))), Rule(json-null, Literal(null)))), {"type":"object","properties":{"arg1":{"type":"string"},"arg2":{"type":"number"}},"required":["arg1"]})), Atomic(Tag(tool-close, Space)))), Rule(tool-get-time, Sequence(Atomic(Sequence(Atomic(Tag(tool-open, Sequence(Literal(), Atomic(Tag(tool-name, Literal(get_time))), Literal()))), Repetition(Sequence(Literal([CALL_ID]), Atomic(Tag(tool-id, Until([ARGS])))), 0, 1), Literal([ARGS]))), Space, Tag(tool-args, Schema(Rule(json-value, Choice(Rule(json-object, Sequence(Literal({), Space, Choice(Literal(}), Sequence(Rule(json-string, Sequence(Literal("), String("), Literal("))), [cycle], Literal(:), [cycle], [cycle], Repetition(Sequence([cycle], Literal(,), [cycle], [cycle], [cycle], [cycle], [cycle], [cycle]), 0, unbounded), [cycle], Literal(}))))), Rule(json-array, Sequence(Literal([), Space, Choice(Literal(]), Sequence([cycle], Repetition(Sequence([cycle], Literal(,), [cycle], [cycle]), 0, unbounded), [cycle], Literal(]))))), [cycle], Rule(json-number, Sequence(Repetition(Literal(-), 0, 1), Choice(Literal(0), Sequence(CharRepeat([1-9], 1, 1), CharRepeat([0-9], 0, unbounded))), Repetition(Sequence(Literal(.), CharRepeat([0-9], 1, unbounded)), 0, 1), Repetition(Sequence(Choice(Literal(e), Literal(E)), Repetition(CharRepeat([+-], 1, 1), 0, 1), [cycle]), 0, 1), Not(CharRepeat([0-9.eE+-], 1, 1)))), Rule(json-bool, Choice(Literal(true), Literal(false))), Rule(json-null, Literal(null)))), {"type":"object","properties":{}})), Atomic(Tag(tool-close, Space))))), Literal(), Repetition(Sequence(Space, [cycle], [cycle], [cycle]), 0, unbounded))), 0, 1), End)
```

## Case 20: tag_json_callid_ser

Template `./Mistral-Small-3.2-24B-Instruct-2506.jinja`:

```jinja
{%- set today = strftime_now("%Y-%m-%d") %}
{%- set default_system_message = "You are Mistral Small 3, a Large Language Model (LLM) created by Mistral AI, a French startup headquartered in Paris.\nYour knowledge base was last updated on 2023-10-01. The current date is " + today + ".\n\nWhen you're not sure about some information or when the user's request requires up-to-date or specific data, you must use the available tools to fetch the information. Do not hesitate to use tools whenever they can provide a more accurate or complete response. If no relevant tools are available, then clearly state that you don't have the information and avoid making up anything.

If the user's question is not clear, ambiguous, or does not provide enough context for you to accurately answer the question, you do not try to answer it right away and you rather ask the user to clarify their request (e.g. \"What are some good restaurants around me?\" => \"Where are you?\" or \"When is the next flight to Tokyo\" => \"Where do you travel from?\").
You are always very attentive to dates, and when asked about information at specific dates, you discard information that is at another date.
You follow these instructions in all languages, and always respond to the user in the language they use or request.
Next sections describe the capabilities that you have.

# WEB BROWSING INSTRUCTIONS

You cannot perform any web search or access internet to open URLs, links etc. If it seems like the user is expecting you to do so, you clarify the situation and ask the user to copy paste the text directly in the chat.

# MULTI-MODAL INSTRUCTIONS

You have the ability to read images, but you cannot generate images. You also cannot transcribe audio files or videos.
You cannot read nor transcribe audio files or videos.

# TOOL CALLING INSTRUCTIONS

You may have access to tools that you can use to fetch information or perform actions. You must use these tools in the following situations:

1. When the request requires up-to-date information.
2. When the request requires specific data that you do not have in your knowledge base.
3. When the request involves actions that you cannot perform without tools.

Always prioritize using tools to provide the most accurate and helpful response. If tools are not available, inform the user that you cannot perform the requested action at the moment." %}

{{- bos_token }}

{%- set system_prompt = default_system_message %}
{%- set loop_messages = messages %}

{%- if not tools is defined %}
    {%- set tools = none %}
{%- endif %}

{%- if messages|length > 0 and messages[0]['role'] == 'system' %}
    {%- if messages[0]['content'] is string %}
        {%- set system_prompt = messages[0]['content'] %}
    {%- else %}
        {%- set system_prompt = messages[0]['content'][0]['text'] %}
    {%- endif %}
    {%- set loop_messages = messages[1:] %}
{%- endif %}

{%- set user_messages = loop_messages | selectattr("role", "equalto", "user") | list %}

{%- set ns = namespace(index=0) %}
{%- for message in loop_messages %}
    {%- if not (message.role == "tool" or (message.get('tool_calls'))) %}
        {%- if (message["role"] == "user") != (ns.index % 2 == 0) %}
            {{- raise_exception("After the optional system message, conversation roles must alternate user/assistant/user/assistant/...") }}
        {%- endif %}
        {%- set ns.index = ns.index + 1 %}
    {%- endif %}
{%- endfor %}

{{- '[SYSTEM_PROMPT]' + system_prompt + '[/SYSTEM_PROMPT]' }}

{%- for message in loop_messages %}
    {%- if message['role'] == 'system' %}
        {%- if message['content'] is string %}
            {{- '[SYSTEM_PROMPT]' + message['content'] + '[/SYSTEM_PROMPT]' }}
        {%- else %}
            {{- '[SYSTEM_PROMPT]' + message['content'][0]['text'] + '[/SYSTEM_PROMPT]' }}
        {%- endif %}
    {%- elif message['role'] == 'user' %}
        {%- if tools is not none and (message == user_messages[-1]) %}
            {{- '[AVAILABLE_TOOLS]' + tools|tojson + '[/AVAILABLE_TOOLS]' }}
        {%- endif %}
        {{- '[INST]' }}
        {%- if message['content'] is string %}
            {{- message['content'] }}
        {%- else %}
            {%- for block in message['content'] %}
                {%- if block['type'] == 'text' %}
                    {{- block['text'] }}
                {%- elif block['type'] in ['image', 'image_url'] %}
                    {{- '[IMG]' }}
                {%- else %}
                    {{- raise_exception('Only text and image blocks are supported in message content!') }}
                {%- endif %}
            {%- endfor %}
        {%- endif %}
        {{- '[/INST]' }}
    {%- elif message['role'] == 'assistant' %}
        {%- if message.get('tool_calls') %}
            {%- for tool_call in message.tool_calls %}
                {{- '[TOOL_CALLS]' + tool_call.function.name }}
                {%- if not tool_call.id is defined or tool_call.id is not string or tool_call.id|length != 9 %}
                    {{- raise_exception("Tool call IDs should be alphanumeric strings with length 9!") }}
                {%- endif %}
                {{- '[CALL_ID]' + tool_call.id }}
                {{- '[ARGS]' + tool_call['function']['arguments']|tojson }}
            {%- endfor %}
            {{- eos_token }}
        {%- elif message['content'] is string %}
            {{- message['content'] + eos_token }}
        {%- else %}
            {%- for block in message['content'] %}
                {%- if block['type'] == 'text' %}
                    {{- block['text'] }}
                {%- elif block['type'] in ['image', 'image_url'] %}
                    {{- '[IMG]' }}
                {%- else %}
                    {{- raise_exception('Only text and image blocks are supported in assistant content!') }}
                {%- endif %}
            {%- endfor %}
            {{- eos_token }}
        {%- endif %}
    {%- elif message['role'] == 'tool_results' or message['role'] == 'tool' %}
        {%- if message.content is defined and message.content.content is defined %}
            {%- set content = message.content.content %}
        {%- else %}
            {%- set content = message.content %}
        {%- endif %}
        {%- if not message.tool_call_id is defined or message.tool_call_id is not string or message['tool_call_id']|length != 9 %}
            {{- raise_exception("Tool call IDs should be alphanumeric strings with length 9!") }}
        {%- endif %}
        {{- '[TOOL_RESULTS]' + message.tool_call_id + '[TOOL_CONTENT]' + content|string + '[/TOOL_RESULTS]' }}
    {%- else %}
        {{- raise_exception('Only system, user, assistant, and tool roles are supported!') }}
    {%- endif %}
{%- endfor %}
```

```
template=./Mistral-Small-3.2-24B-Instruct-2506.jinja
params: tools=1 tool_choice=AUTO reasoning=0 json_schema=0 add_generation_prompt=1 parallel=0
analysis: user_start=[[INST]] assistant_start=[]
analysis: reasoning=NONE start=[] end=[]
analysis: content=PLAIN start=[] end=[]
analysis: wrapped=0 fkey=0 owrap=0 tolerate=0
analysis: fields name=[name] args=[arguments] id=[] gen=[] order=[]
analysis: callid pos=2 prefix=[[CALL_ID]] suffix=[[ARGS]]
analysis: func prefix=[] suffix=[] close=[] args_start=[] args_end=[] arg_name=[]|[] arg_value=[]|[]
analysis: tool_mode=TAG_WITH_JSON section_start=[] section_end=[] per_call_start=[[TOOL_CALLS]] per_call_end=[]
generation_prompt=[]
```

Expected dump (native oracle):

```text
Sequence(Epsilon, Space, Epsilon, Repetition(Tag(content, Until([TOOL_CALLS])), 0, 1), Repetition(Rule(tool-call, Sequence(Literal([TOOL_CALLS]), Choice(Rule(tool-get-weather, Sequence(Atomic(Sequence(Atomic(Tag(tool-open, Sequence(Literal(), Atomic(Tag(tool-name, Literal(get_weather))), Literal()))), Repetition(Sequence(Literal([CALL_ID]), Atomic(Tag(tool-id, Until([ARGS])))), 0, 1), Literal([ARGS]))), Space, Tag(tool-args, Schema(Rule(json-value, Choice(Rule(json-object, Sequence(Literal({), Space, Choice(Literal(}), Sequence(Rule(json-string, Sequence(Literal("), String("), Literal("))), [cycle], Literal(:), [cycle], [cycle], Repetition(Sequence([cycle], Literal(,), [cycle], [cycle], [cycle], [cycle], [cycle], [cycle]), 0, unbounded), [cycle], Literal(}))))), Rule(json-array, Sequence(Literal([), Space, Choice(Literal(]), Sequence([cycle], Repetition(Sequence([cycle], Literal(,), [cycle], [cycle]), 0, unbounded), [cycle], Literal(]))))), [cycle], Rule(json-number, Sequence(Repetition(Literal(-), 0, 1), Choice(Literal(0), Sequence(CharRepeat([1-9], 1, 1), CharRepeat([0-9], 0, unbounded))), Repetition(Sequence(Literal(.), CharRepeat([0-9], 1, unbounded)), 0, 1), Repetition(Sequence(Choice(Literal(e), Literal(E)), Repetition(CharRepeat([+-], 1, 1), 0, 1), [cycle]), 0, 1), Not(CharRepeat([0-9.eE+-], 1, 1)))), Rule(json-bool, Choice(Literal(true), Literal(false))), Rule(json-null, Literal(null)))), {"type":"object","properties":{"arg1":{"type":"string"},"arg2":{"type":"number"}},"required":["arg1"]})), Atomic(Tag(tool-close, Space)))), Rule(tool-get-time, Sequence(Atomic(Sequence(Atomic(Tag(tool-open, Sequence(Literal(), Atomic(Tag(tool-name, Literal(get_time))), Literal()))), Repetition(Sequence(Literal([CALL_ID]), Atomic(Tag(tool-id, Until([ARGS])))), 0, 1), Literal([ARGS]))), Space, Tag(tool-args, Schema(Rule(json-value, Choice(Rule(json-object, Sequence(Literal({), Space, Choice(Literal(}), Sequence(Rule(json-string, Sequence(Literal("), String("), Literal("))), [cycle], Literal(:), [cycle], [cycle], Repetition(Sequence([cycle], Literal(,), [cycle], [cycle], [cycle], [cycle], [cycle], [cycle]), 0, unbounded), [cycle], Literal(}))))), Rule(json-array, Sequence(Literal([), Space, Choice(Literal(]), Sequence([cycle], Repetition(Sequence([cycle], Literal(,), [cycle], [cycle]), 0, unbounded), [cycle], Literal(]))))), [cycle], Rule(json-number, Sequence(Repetition(Literal(-), 0, 1), Choice(Literal(0), Sequence(CharRepeat([1-9], 1, 1), CharRepeat([0-9], 0, unbounded))), Repetition(Sequence(Literal(.), CharRepeat([0-9], 1, unbounded)), 0, 1), Repetition(Sequence(Choice(Literal(e), Literal(E)), Repetition(CharRepeat([+-], 1, 1), 0, 1), [cycle]), 0, 1), Not(CharRepeat([0-9.eE+-], 1, 1)))), Rule(json-bool, Choice(Literal(true), Literal(false))), Rule(json-null, Literal(null)))), {"type":"object","properties":{}})), Atomic(Tag(tool-close, Space))))), Literal())), 0, 1), End)
```

## Case 21: json_schema_response

Template `./t0.jinja`:

```jinja
{% for message in messages %}{{ message['role'] }}: {{ message['content'] }}
{% endfor %}{% if add_generation_prompt %}assistant: {% endif %}
```

```
template=./t0.jinja
params: tools=0 tool_choice=NONE reasoning=0 json_schema=1 add_generation_prompt=1 parallel=1
analysis: user_start=[user:] assistant_start=[assistant:]
analysis: reasoning=NONE start=[] end=[]
analysis: content=PLAIN start=[] end=[]
analysis: wrapped=0 fkey=0 owrap=0 tolerate=0
analysis: fields name=[name] args=[arguments] id=[] gen=[] order=[]
analysis: callid pos=0 prefix=[] suffix=[]
analysis: func prefix=[] suffix=[] close=[] args_start=[] args_end=[] arg_name=[]|[] arg_value=[]|[]
analysis: tool_mode=NONE section_start=[] section_end=[] per_call_start=[] per_call_end=[]
generation_prompt=[assistant: ]
```

Expected dump (native oracle):

```text
Sequence(Literal(assistant: ), Space, Epsilon, Space, Choice(Sequence(Literal(```json), Space, Rule(response-format, Tag(content, Schema(Rule(json-value, Choice(Rule(json-object, Sequence(Literal({), Space, Choice(Literal(}), Sequence(Rule(json-string, Sequence(Literal("), String("), Literal("))), [cycle], Literal(:), [cycle], [cycle], Repetition(Sequence([cycle], Literal(,), [cycle], [cycle], [cycle], [cycle], [cycle], [cycle]), 0, unbounded), [cycle], Literal(}))))), Rule(json-array, Sequence(Literal([), Space, Choice(Literal(]), Sequence([cycle], Repetition(Sequence([cycle], Literal(,), [cycle], [cycle]), 0, unbounded), [cycle], Literal(]))))), [cycle], Rule(json-number, Sequence(Repetition(Literal(-), 0, 1), Choice(Literal(0), Sequence(CharRepeat([1-9], 1, 1), CharRepeat([0-9], 0, unbounded))), Repetition(Sequence(Literal(.), CharRepeat([0-9], 1, unbounded)), 0, 1), Repetition(Sequence(Choice(Literal(e), Literal(E)), Repetition(CharRepeat([+-], 1, 1), 0, 1), [cycle]), 0, 1), Not(CharRepeat([0-9.eE+-], 1, 1)))), Rule(json-bool, Choice(Literal(true), Literal(false))), Rule(json-null, Literal(null)))), {"type":"object","properties":{"answer":{"type":"string"}},"required":["answer"]}))), Space, Literal(```)), Sequence(Space, [cycle], Space)), End)
```

