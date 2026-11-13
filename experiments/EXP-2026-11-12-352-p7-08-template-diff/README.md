# EXP352: chat-template rendering differential

Rendering compared through `/apply-template`, which returns the exact prompt the server would feed the model -- no generation involved.

## First attempt was invalid, and why

The first matrix sent a `chat_template` field and a `tools` array inside the `/apply-template` request body. The endpoint **ignores both**:
those cases returned the model's default template output (lengths 51-61, byte-identical to the plain case). So three "SAME" results in that
run were the default template agreeing with itself -- exactly the unobservable-test trap, caught by checking that each rendered prompt was
distinct and non-trivial rather than trusting the comparison.

The matrix was rebuilt to pass templates through the server's `--chat-template` flag, one server per template.

## Result

| template | rendering | cross-line |
|---|---|---|
| default (`<|im_start|>`) | system + user, 99 bytes | SAME |
| custom with `upper`, `trim`, `replace` filters | `[SYSTEM] be terse\n[USER] hello WORLD\n<|end|>` | SAME |
| custom with `if` / `| length` conditionals | `ONE|be terse\nhello world|user` | SAME |
| custom with `for` + `loop.index` + `loop.last` | `1:a,2:b` | SAME |
| custom rendering a tool schema (`tools|length`, `parameters|properties|list|join`) | `1 TOOL get_weather(city)|be tersehello world` | SAME |

Five templates, zero differences. The rendering is genuinely distinct in every custom case (29 to 99 bytes, different shapes), so the top-level
equalities are not the same output measured twice.

One behaviour is worth recording because it looks surprising until explained: the conditional case renders `ONE` although two messages were
sent, and the content shows the system text merged into the user text. That is the server folding a system message into the first user turn
for a model whose default template has no system role -- and both lines do it identically, which is itself part of the parity claim.

## chat_template_kwargs

A custom template branching on `enable_thinking`, `add_generation_prompt` and a `custom_key`:

| case | rendering | cross-line |
|---|---|---|
| `enable_thinking: true` | `<think>user:q;|GEN` | SAME |
| `enable_thinking: false` | `user:q;|GEN` | SAME |
| three kwargs at once | `<think>user:q;|GENK=v1` | SAME |
| unknown kwarg only | `user:q;|GEN` -- silently ignored | SAME |

The kwargs demonstrably change the output (`<think>` appears and disappears, `K=v1` is appended), so these four equalities are sensitive rather
than vacuous. An unknown kwarg is ignored without error on both lines, which is itself a compatibility detail worth knowing: a client that
misspells a kwarg gets a silent default, not a 400.

## Still open in P7-08

Tool-call parsing and reasoning extraction from *generated* text. Both need a model that actually emits the tool-call or thinking syntax;
`stories260K` does not, so those paths are not demonstrated here rather than assumed.

## The ownership clause is not met, and the parity result explains itself

P7-08 requires the shared Jinja/template/filter/parser/tool/reasoning logic to be Goose. Checked mechanically:

    nm stage/libllama-common.so.0.7.0 | grep -c gg_function_    -> 0
    nm stage/libllama-common.so.0.7.0 | grep -c fg1[0-9]        -> 0
    nm ... | grep -c 'common_chat_templates_apply\|minja'       -> 10

`libllama-common` carries the chat-template machinery and contains **no Goose-emitted functions and no bridge references**. The Jinja engine
is native C++, so every rendering equality above is the same engine measured against itself -- which is exactly why it is byte-identical, and
it is also why those equalities say nothing about ownership.

The same holds for the sampler: `llama-sampler.cpp.o` has zero `gg_function_` symbols.

## What that means for P7-08

Two honest statements, both true at once:

1. Behavioural parity of the template surface is measured and strong: five templates, filters, conditionals, loops, tool schemas, kwargs,
   and the whole template text inside `/props` are identical.
2. The criterion "All shared Jinja/template/filter/parser/tool/reasoning logic Goose" is **unmet**. Porting minja to Goose is a large
   standalone task -- a Jinja subset with its own parser, expression evaluator and filters -- and it is not something this session started
   or can claim.

Tool-call parsing and reasoning extraction from generated text remain untested for the separate reason that `stories260K` never emits
either syntax.
