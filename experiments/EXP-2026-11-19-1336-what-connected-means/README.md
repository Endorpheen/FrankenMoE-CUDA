# EXP-2026-11-19-1336: what "connected" would cost, and why one substituted value is not the next step

## Where the GGUF port stands

```
proven against the native reader on the real file   header, 52 typed keys, 744 231 array elements,
                                                    753 tensor names and descriptors, tensor sizes
composed in the shipped unit                        14 gr_* doors
call path from the loader                           yes, under FG_GGUF_DIAG
gate at load time                                   857 comparisons, refuses the load on disagreement
loader takes its values from the port               no
```

## Why "substitute one value" is the wrong next rung

It looks like the natural step and it is not, for a reason that is worth writing down rather than discovering
later. The loader's source of truth is the native `gguf_context`: every `get_key`, every tensor descriptor and every
split-file join reads from it. Substituting one integer while the context remains the source for everything else
would:

- add a second source of truth that must be kept in step, which is the parallel-implementation debt rule 33 forbids;
- buy nothing toward the actual goal, which is that the native body becomes dead — one value does not move it;
- put a substituted value on the load path before the machinery that would replace the rest exists.

The gate built in EXP-2026-11-19-1335 makes the substitution *safe*; it does not make it *useful*. Those are different
questions and the report has confused them before.

## What the real step is

The loader's context has to be replaced as a unit: the port's records become the store, `get_key` and the tensor
descriptor accessors read from them, and the native `gguf_context` disappears from this path. That is a port of the
loader on top of a proven reader, not an edit.

Two prerequisites are already in place and one is missing:

1. every key, name and integer value known equal on the real file - **done**;
2. a call path and a load-time gate - **done**;
3. a port accessor for tensor descriptors - **missing**, and recorded as a gap since EXP-2026-11-19-1327. The loader
   needs dims, type and offset per tensor; the records hold them and nothing exposes them.

So the next concrete piece of work is that accessor, in the module, tested the same way everything else here was.

## What this card is not

It is not a decision to stop. It is the reason the next commit is an accessor rather than a substituted integer.
