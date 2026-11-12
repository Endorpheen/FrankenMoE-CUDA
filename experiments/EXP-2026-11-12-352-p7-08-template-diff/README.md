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
