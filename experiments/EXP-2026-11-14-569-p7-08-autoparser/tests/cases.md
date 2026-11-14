# Autoparser generator oracle cases

Eight template/analysis pairs captured from the native `peg_generator::generate_parser` (no model weights).

Oracle build and run:

```sh
bash build_probe.sh          # links probe.cpp against build/llama-cuda/libllama-common.so
./probe . > probe.out       # 8 cases, analysis + common_peg_arena::dump
```

The analysis lines are the struct fields printed by `autoparser::analyze_template`; `dump=` is the verbatim `common_peg_arena::dump(arena.root())` result. Dumps are single lines. Three templates are used: `t0.jinja` (plain), `t2.jinja` (wrapped content), `t3.jinja` (tag with JSON tool calls).

## Case 1: plain_roles_none

Template `./t0.jinja`:

```jinja
{% for message in messages %}{{ message['role'] }}: {{ message['content'] }}
{% endfor %}{% if add_generation_prompt %}assistant: {% endif %}
```

```
template=./t0.jinja
params: tools=0 tool_choice=NONE reasoning=0 json_schema=0 add_generation_prompt=1
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

## Case 2: plain_roles_auto

Template `./t0.jinja`:

```jinja
{% for message in messages %}{{ message['role'] }}: {{ message['content'] }}
{% endfor %}{% if add_generation_prompt %}assistant: {% endif %}
```

```
template=./t0.jinja
params: tools=0 tool_choice=NONE reasoning=1 json_schema=0 add_generation_prompt=1
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

## Case 3: wrapped_content_none

Template `./t2.jinja`:

```jinja
{% for message in messages %}{% if message['role'] == 'user' %}<user>{{ message['content'] }}</user>{% else %}<assistant>{% if message['content'] %}<response>{{ message['content'] }}</response>{% endif %}</assistant>{% endif %}{% endfor %}{% if add_generation_prompt %}<assistant>{% endif %}
```

```
template=./t2.jinja
params: tools=0 tool_choice=NONE reasoning=0 json_schema=0 add_generation_prompt=1
analysis: user_start=[<user>] assistant_start=[<assistant><response>]
analysis: reasoning=NONE start=[] end=[]
analysis: content=ALWAYS_WRAPPED start=[<response>] end=[</response>]
analysis: tool_mode=NONE section_start=[] section_end=[] per_call_start=[] per_call_end=[]
generation_prompt=[<assistant>]
grammar_lazy=0 grammar_len=0 triggers=0
```

Expected dump (native oracle):

```text
Sequence(Literal(<assistant>), Tag(content, Until(<response>)), Literal(<response>), Tag(content, Until(</response>)), Literal(</response>), End)
```

## Case 4: wrapped_content_auto

Template `./t2.jinja`:

```jinja
{% for message in messages %}{% if message['role'] == 'user' %}<user>{{ message['content'] }}</user>{% else %}<assistant>{% if message['content'] %}<response>{{ message['content'] }}</response>{% endif %}</assistant>{% endif %}{% endfor %}{% if add_generation_prompt %}<assistant>{% endif %}
```

```
template=./t2.jinja
params: tools=0 tool_choice=NONE reasoning=1 json_schema=0 add_generation_prompt=1
analysis: user_start=[<user>] assistant_start=[<assistant><response>]
analysis: reasoning=NONE start=[] end=[]
analysis: content=ALWAYS_WRAPPED start=[<response>] end=[</response>]
analysis: tool_mode=NONE section_start=[] section_end=[] per_call_start=[] per_call_end=[]
generation_prompt=[<assistant>]
grammar_lazy=0 grammar_len=0 triggers=0
```

Expected dump (native oracle):

```text
Sequence(Literal(<assistant>), Tag(content, Until(<response>)), Literal(<response>), Tag(content, Until(</response>)), Literal(</response>), End)
```

## Case 5: tag_tools_auto

Template `./t3.jinja`:

```jinja
{% for message in messages %}{% if message['role'] == 'user' %}<user>{{ message['content'] }}</user>{% else %}<assistant>{{ message['content'] }}{% if message['tool_calls'] %}{% for tc in message['tool_calls'] %}<tool_call>{"name": "{{ tc['function']['name'] }}", "arguments": {{ tc['function']['arguments'] | tojson }}}</tool_call>{% endfor %}{% endif %}</assistant>{% endif %}{% endfor %}{% if add_generation_prompt %}<assistant>{% endif %}
```

```
template=./t3.jinja
params: tools=1 tool_choice=AUTO reasoning=0 json_schema=0 add_generation_prompt=1
analysis: user_start=[<user>] assistant_start=[<assistant>]
analysis: reasoning=NONE start=[] end=[]
analysis: content=PLAIN start=[] end=[]
analysis: tool_mode=JSON_NATIVE section_start=[] section_end=[] per_call_start=[<tool_call>] per_call_end=[</tool_call>]
generation_prompt=[<assistant>]
grammar_lazy=1 grammar_len=1675 triggers=1
```

Expected dump (native oracle):

```text
Sequence(Literal(<assistant>), Space, Epsilon, Repetition(Tag(content, Until(<tool_call>)), 0, 1), Rule(tool-calls, Repetition(Sequence(Repetition(Rule(tool-call, Sequence(Literal(<tool_call>), Space, Choice(Rule(tool-get-weather, Tag(tool, Sequence(Atomic(Tag(tool-open, Literal({))), Space, Epsilon, Literal("name"), Space, Literal(:), Space, Atomic(Sequence(Literal("), Atomic(Tag(tool-name, Literal(get_weather))), Literal("))), Space, Literal(,), Space, Literal("arguments"), Space, Literal(:), Space, Tag(tool-args, Schema(Rule(json-value, Choice(Rule(json-object, Sequence(Literal({), Space, Choice(Literal(}), Sequence(Rule(json-string, Sequence(Literal("), String("), Literal("))), [cycle], Literal(:), [cycle], [cycle], Repetition(Sequence([cycle], Literal(,), [cycle], [cycle], [cycle], [cycle], [cycle], [cycle]), 0, unbounded), [cycle], Literal(}))))), Rule(json-array, Sequence(Literal([), Space, Choice(Literal(]), Sequence([cycle], Repetition(Sequence([cycle], Literal(,), [cycle], [cycle]), 0, unbounded), [cycle], Literal(]))))), [cycle], Rule(json-number, Sequence(Repetition(Literal(-), 0, 1), Choice(Literal(0), Sequence(CharRepeat([1-9], 1, 1), CharRepeat([0-9], 0, unbounded))), Repetition(Sequence(Literal(.), CharRepeat([0-9], 1, unbounded)), 0, 1), Repetition(Sequence(Choice(Literal(e), Literal(E)), Repetition(CharRepeat([+-], 1, 1), 0, 1), [cycle]), 0, 1), Not(CharRepeat([0-9.eE+-], 1, 1)))), Rule(json-bool, Choice(Literal(true), Literal(false))), Rule(json-null, Literal(null)))), {"type":"object","properties":{"arg1":{"type":"string"},"arg2":{"type":"number"}},"required":["arg1"]})), Space, Atomic(Tag(tool-close, Literal(})))))), Rule(tool-get-time, Tag(tool, Sequence(Atomic(Tag(tool-open, Literal({))), Space, Epsilon, Literal("name"), Space, Literal(:), Space, Atomic(Sequence(Literal("), Atomic(Tag(tool-name, Literal(get_time))), Literal("))), Space, Literal(,), Space, Literal("arguments"), Space, Literal(:), Space, Tag(tool-args, Schema(Rule(json-value, Choice(Rule(json-object, Sequence(Literal({), Space, Choice(Literal(}), Sequence(Rule(json-string, Sequence(Literal("), String("), Literal("))), [cycle], Literal(:), [cycle], [cycle], Repetition(Sequence([cycle], Literal(,), [cycle], [cycle], [cycle], [cycle], [cycle], [cycle]), 0, unbounded), [cycle], Literal(}))))), Rule(json-array, Sequence(Literal([), Space, Choice(Literal(]), Sequence([cycle], Repetition(Sequence([cycle], Literal(,), [cycle], [cycle]), 0, unbounded), [cycle], Literal(]))))), [cycle], Rule(json-number, Sequence(Repetition(Literal(-), 0, 1), Choice(Literal(0), Sequence(CharRepeat([1-9], 1, 1), CharRepeat([0-9], 0, unbounded))), Repetition(Sequence(Literal(.), CharRepeat([0-9], 1, unbounded)), 0, 1), Repetition(Sequence(Choice(Literal(e), Literal(E)), Repetition(CharRepeat([+-], 1, 1), 0, 1), [cycle]), 0, 1), Not(CharRepeat([0-9.eE+-], 1, 1)))), Rule(json-bool, Choice(Literal(true), Literal(false))), Rule(json-null, Literal(null)))), {"type":"object","properties":{}})), Space, Atomic(Tag(tool-close, Literal(}))))))), Repetition(Sequence(Space, Literal(,), Space, [cycle]), 0, unbounded), Space, Literal(</tool_call>))), 0, 1), Space), 1, unbounded)), End)
```

## Case 6: tag_tools_required

Template `./t3.jinja`:

```jinja
{% for message in messages %}{% if message['role'] == 'user' %}<user>{{ message['content'] }}</user>{% else %}<assistant>{{ message['content'] }}{% if message['tool_calls'] %}{% for tc in message['tool_calls'] %}<tool_call>{"name": "{{ tc['function']['name'] }}", "arguments": {{ tc['function']['arguments'] | tojson }}}</tool_call>{% endfor %}{% endif %}</assistant>{% endif %}{% endfor %}{% if add_generation_prompt %}<assistant>{% endif %}
```

```
template=./t3.jinja
params: tools=1 tool_choice=REQUIRED reasoning=0 json_schema=0 add_generation_prompt=1
analysis: user_start=[<user>] assistant_start=[<assistant>]
analysis: reasoning=NONE start=[] end=[]
analysis: content=PLAIN start=[] end=[]
analysis: tool_mode=JSON_NATIVE section_start=[] section_end=[] per_call_start=[<tool_call>] per_call_end=[</tool_call>]
generation_prompt=[<assistant>]
grammar_lazy=0 grammar_len=2454 triggers=0
```

Expected dump (native oracle):

```text
Sequence(Literal(<assistant>), Space, Epsilon, Repetition(Tag(content, Until(<tool_call>)), 0, 1), Rule(tool-calls, Repetition(Sequence(Rule(tool-call, Sequence(Literal(<tool_call>), Space, Choice(Rule(tool-get-weather, Tag(tool, Sequence(Atomic(Tag(tool-open, Literal({))), Space, Epsilon, Literal("name"), Space, Literal(:), Space, Atomic(Sequence(Literal("), Atomic(Tag(tool-name, Literal(get_weather))), Literal("))), Space, Literal(,), Space, Literal("arguments"), Space, Literal(:), Space, Tag(tool-args, Schema(Rule(json-value, Choice(Rule(json-object, Sequence(Literal({), Space, Choice(Literal(}), Sequence(Rule(json-string, Sequence(Literal("), String("), Literal("))), [cycle], Literal(:), [cycle], [cycle], Repetition(Sequence([cycle], Literal(,), [cycle], [cycle], [cycle], [cycle], [cycle], [cycle]), 0, unbounded), [cycle], Literal(}))))), Rule(json-array, Sequence(Literal([), Space, Choice(Literal(]), Sequence([cycle], Repetition(Sequence([cycle], Literal(,), [cycle], [cycle]), 0, unbounded), [cycle], Literal(]))))), [cycle], Rule(json-number, Sequence(Repetition(Literal(-), 0, 1), Choice(Literal(0), Sequence(CharRepeat([1-9], 1, 1), CharRepeat([0-9], 0, unbounded))), Repetition(Sequence(Literal(.), CharRepeat([0-9], 1, unbounded)), 0, 1), Repetition(Sequence(Choice(Literal(e), Literal(E)), Repetition(CharRepeat([+-], 1, 1), 0, 1), [cycle]), 0, 1), Not(CharRepeat([0-9.eE+-], 1, 1)))), Rule(json-bool, Choice(Literal(true), Literal(false))), Rule(json-null, Literal(null)))), {"type":"object","properties":{"arg1":{"type":"string"},"arg2":{"type":"number"}},"required":["arg1"]})), Space, Atomic(Tag(tool-close, Literal(})))))), Rule(tool-get-time, Tag(tool, Sequence(Atomic(Tag(tool-open, Literal({))), Space, Epsilon, Literal("name"), Space, Literal(:), Space, Atomic(Sequence(Literal("), Atomic(Tag(tool-name, Literal(get_time))), Literal("))), Space, Literal(,), Space, Literal("arguments"), Space, Literal(:), Space, Tag(tool-args, Schema(Rule(json-value, Choice(Rule(json-object, Sequence(Literal({), Space, Choice(Literal(}), Sequence(Rule(json-string, Sequence(Literal("), String("), Literal("))), [cycle], Literal(:), [cycle], [cycle], Repetition(Sequence([cycle], Literal(,), [cycle], [cycle], [cycle], [cycle], [cycle], [cycle]), 0, unbounded), [cycle], Literal(}))))), Rule(json-array, Sequence(Literal([), Space, Choice(Literal(]), Sequence([cycle], Repetition(Sequence([cycle], Literal(,), [cycle], [cycle]), 0, unbounded), [cycle], Literal(]))))), [cycle], Rule(json-number, Sequence(Repetition(Literal(-), 0, 1), Choice(Literal(0), Sequence(CharRepeat([1-9], 1, 1), CharRepeat([0-9], 0, unbounded))), Repetition(Sequence(Literal(.), CharRepeat([0-9], 1, unbounded)), 0, 1), Repetition(Sequence(Choice(Literal(e), Literal(E)), Repetition(CharRepeat([+-], 1, 1), 0, 1), [cycle]), 0, 1), Not(CharRepeat([0-9.eE+-], 1, 1)))), Rule(json-bool, Choice(Literal(true), Literal(false))), Rule(json-null, Literal(null)))), {"type":"object","properties":{}})), Space, Atomic(Tag(tool-close, Literal(}))))))), Repetition(Sequence(Space, Literal(,), Space, [cycle]), 0, unbounded), Space, Literal(</tool_call>))), Space), 1, unbounded)), End)
```

## Case 7: tag_tools_schema

Template `./t3.jinja`:

```jinja
{% for message in messages %}{% if message['role'] == 'user' %}<user>{{ message['content'] }}</user>{% else %}<assistant>{{ message['content'] }}{% if message['tool_calls'] %}{% for tc in message['tool_calls'] %}<tool_call>{"name": "{{ tc['function']['name'] }}", "arguments": {{ tc['function']['arguments'] | tojson }}}</tool_call>{% endfor %}{% endif %}</assistant>{% endif %}{% endfor %}{% if add_generation_prompt %}<assistant>{% endif %}
```

```
template=./t3.jinja
params: tools=1 tool_choice=AUTO reasoning=0 json_schema=1 add_generation_prompt=1
analysis: user_start=[<user>] assistant_start=[<assistant>]
analysis: reasoning=NONE start=[] end=[]
analysis: content=PLAIN start=[] end=[]
analysis: tool_mode=JSON_NATIVE section_start=[] section_end=[] per_call_start=[<tool_call>] per_call_end=[</tool_call>]
generation_prompt=[<assistant>]
grammar_lazy=0 grammar_len=980 triggers=0
```

Expected dump (native oracle):

```text
Sequence(Literal(<assistant>), Space, Epsilon, Space, Choice(Sequence(Literal(```json), Space, Rule(response-format, Tag(content, Schema(Rule(json-value, Choice(Rule(json-object, Sequence(Literal({), Space, Choice(Literal(}), Sequence(Rule(json-string, Sequence(Literal("), String("), Literal("))), [cycle], Literal(:), [cycle], [cycle], Repetition(Sequence([cycle], Literal(,), [cycle], [cycle], [cycle], [cycle], [cycle], [cycle]), 0, unbounded), [cycle], Literal(}))))), Rule(json-array, Sequence(Literal([), Space, Choice(Literal(]), Sequence([cycle], Repetition(Sequence([cycle], Literal(,), [cycle], [cycle]), 0, unbounded), [cycle], Literal(]))))), [cycle], Rule(json-number, Sequence(Repetition(Literal(-), 0, 1), Choice(Literal(0), Sequence(CharRepeat([1-9], 1, 1), CharRepeat([0-9], 0, unbounded))), Repetition(Sequence(Literal(.), CharRepeat([0-9], 1, unbounded)), 0, 1), Repetition(Sequence(Choice(Literal(e), Literal(E)), Repetition(CharRepeat([+-], 1, 1), 0, 1), [cycle]), 0, 1), Not(CharRepeat([0-9.eE+-], 1, 1)))), Rule(json-bool, Choice(Literal(true), Literal(false))), Rule(json-null, Literal(null)))), {"type":"object","properties":{"answer":{"type":"string"}},"required":["answer"]}))), Space, Literal(```)), Sequence(Space, [cycle], Space)), End)
```

## Case 8: tag_content_only

Template `./t3.jinja`:

```jinja
{% for message in messages %}{% if message['role'] == 'user' %}<user>{{ message['content'] }}</user>{% else %}<assistant>{{ message['content'] }}{% if message['tool_calls'] %}{% for tc in message['tool_calls'] %}<tool_call>{"name": "{{ tc['function']['name'] }}", "arguments": {{ tc['function']['arguments'] | tojson }}}</tool_call>{% endfor %}{% endif %}</assistant>{% endif %}{% endfor %}{% if add_generation_prompt %}<assistant>{% endif %}
```

```
template=./t3.jinja
params: tools=0 tool_choice=NONE reasoning=0 json_schema=0 add_generation_prompt=1
analysis: user_start=[<user>] assistant_start=[<assistant>]
analysis: reasoning=NONE start=[] end=[]
analysis: content=PLAIN start=[] end=[]
analysis: tool_mode=JSON_NATIVE section_start=[] section_end=[] per_call_start=[<tool_call>] per_call_end=[</tool_call>]
generation_prompt=[<assistant>]
grammar_lazy=0 grammar_len=0 triggers=0
```

Expected dump (native oracle):

```text
Sequence(Literal(<assistant>), Epsilon, Tag(content, Until()), End)
```

