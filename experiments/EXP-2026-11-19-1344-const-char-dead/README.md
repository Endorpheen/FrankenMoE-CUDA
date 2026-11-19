# EXP-2026-11-19-1344: the `const char *` key form is dead code, not a remaining native path

## What the plan said, and what is true

The replacement plan listed `const char *` keys among the native reads still to convert, and the previous card
deliberately left them alone because serving them would need a NUL-terminated copy with an undeclared lifetime.

Checking before building that machinery:

```
grep for a get_key call with a const char * target   -> nothing
grep for GKV<const char *> instantiation             -> nothing
```

The specialisation exists in `llama-model-loader.cpp` and is **never instantiated**. No key is read in that form, so
there is no native path to convert - the entry was a phantom in the work list, and the copy-with-a-lifetime it would
have required would have been machinery for a case that does not occur.

## Why this is worth a card

Two corrections to the same plan in two cards (six descriptor sites that were one; a `const char *` path that is
nothing) is a pattern worth naming: the plan was written from greps, and greps count declarations as readily as uses.
The remaining work is now short enough to be enumerated exactly rather than estimated:

| remaining | where |
|---|---|
| array walk | `llama-impl.cpp`, one site |
| array walks | `llama-adapter.cpp`, two sites |
| split-file path | `llama_model_loader`, no port comparison exists for it |

## What happens to the dead specialisation

Recorded, not removed. Rule 28 says dead code may be deleted when provably unreachable, and a never-instantiated
template specialisation qualifies - but it is template infrastructure that a future model format might use, and
deleting it is a separate decision from the port. It stays, with this card as the record that nothing reaches it.
