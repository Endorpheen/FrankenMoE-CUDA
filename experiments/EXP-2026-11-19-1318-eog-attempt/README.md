# EXP-2026-11-19-1318: the EOG cross-check was attempted and not completed; the tree is intact

## What was tried

The same producer-side pattern that proved the detokenizer over the whole vocabulary
(EXP-2026-11-19-1317) applied to `bt_is_eog` - a pure membership test over the end-of-generation set, so the entire
vocabulary can be compared once.

## What went wrong

Two mechanical failures, both mine:

1. the helper namespace sat below its first use, so the insertion could not compile where it landed;
2. moving it above that use cut a line that balanced a brace, and the errors that followed were reported against
   unrelated functions (`get_type`, `type_name`) - the file was structurally broken, not semantically wrong.

Both were reverted with `git checkout` rather than patched again, and the tree re-verified:

```
rebuild                 rc=0
FG_TK_DETOK_TOTAL       calls=248320 disagreements=0     (the piece check still works)
library hash            03b87ec83b154f43
```

## What this records

The end-of-generation door is still unexercised. That is a smaller gap than the detokenizer's was, because
`bt_is_eog` is a loop over an array and its equality follows from the set it is given - but "follows from" is what this
session keeps replacing with a count, so it stays open and stated.

## The lesson that repeated here

Editing a 4000-line file by string surgery at three different points, without compiling between them, produced a
broken file and three misleading errors. The fix that worked twice today was the boring one: restore, rebuild, re-run
the check that already passed. The next attempt at this door starts from a clean file with the helper placed once,
compiled before the sweep is added.

## Status

Detokenizer: proven over 248 320 tokens, still unused at run time.
End-of-generation: unexercised, next on the list.
Tree: clean, builds, check passes.
