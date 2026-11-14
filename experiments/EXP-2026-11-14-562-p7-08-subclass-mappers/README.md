# EXP562: gemma4 and MiniMax-M3 mappers (P7-08 slice 7, part 2)

## Hypothesis

The two native subclasses differ from the base mapper in one place each: gemma4
builds arguments from rule names (`gemma4-*`) and stops recursion at
content/reasoning/tool nodes, while minimax builds them from schema containers
(`tool-arg-object/array/item`) and passes `closed = !node.is_partial`. If those
rules are reproduced, both match the oracle on hand-built ASTs.

## Starting point and review

A helper agent produced a self-contained draft plus ten oracle pairs. It was
rebuilt and re-run here before use: `goose --standalone` builds, and the draft
fixture reports `RESULT cases=10 failures=0`. The draft was then merged into the
canonical runtime; its own helpers that still use the draft's flat ABI
(`g_esc_inner`, `g_esc_quoted`, `g_push`, `g_emit_hex4`) are kept under `g_`, and
the shared helpers (`m_app`, `m_appc`, `m_san`, `m_is_space`, `m_trim_trail`) come
from the already accepted base port.

## What the native subclasses do

- Both stop the walk at `content`/`reasoning`/`tool`, so a tagged child is not
  emitted twice, unlike the base `arena.visit`.
- `find_by_tag` searches up to depth 3, not just direct children.
- gemma4: the name is used without trimming, a partial name is rejected, the
  arguments come from the first child of `tool-args`, and `id` is never read.
- minimax: the name has no partial check, arguments come from the whole `tool`
  node as a container with `closed = !node.is_partial`, and `tool-arg` without a
  name is skipped.
- Neither calls the base `from_ast`, so whitespace-only reasoning is not cleaned
  and `id` stays empty.

## Gate

```
FG562_RUNTIME_SUBCLASS cases=13 values=65 failures=0 skipped=0
```

Thirteen hand-built AST pairs: gemma4 with content and one call, arrays and
booleans, a partial string, a partial array, a partial name, arguments before the
name, a dict key case, an id present in input but empty in the message, a tool
whose name and args are one level deeper, and an empty value node. Partial cases
are G3, G4, G5, G10 and M2.

## Approximations recorded

1. The draft's `g_*` helpers still use a flat ABI that differs from the base
   port's target buffer; unifying them is a separate slice.
2. JSON escaping does not validate UTF-8, like the base mapper.
3. Two upstream quirks are copied rather than fixed: a gemma4 string without
   children does not close its quote, and an empty container prints nothing.
4. minimax does not check partial for the name, so a streamed truncated name can
   reach `tool_calls`; a member with a name and no value produces a bare
   `"name":`.
5. The gate uses reference JSON captured from the oracle and synthetic ids; a
   real arena also needs the `sdir` mapping, and service tags must stay out of
   `tid` or `collect` treats them as arguments.

## Still open

partial streaming, the tool builders, the typed direction, autoparser/grammars,
model parsers, `use_jinja=false` and the chat API's missing time injection.
