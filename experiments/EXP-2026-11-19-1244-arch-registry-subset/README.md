# EXP-2026-11-19-1244 (corrected): the MoE recipe registry is complete; my check was wrong twice

This card first claimed "the architecture registry holds eight of 163 names". That claim was wrong, and the
two errors were both in my check, not in the port.

## Error one: comparing against a different registry

The port's own header names its source: `runtime/core/src/moe/arch_registry.cpp`, the nine-row `k_recipes`
table of **MoE recipes**. I compared it against llama.cpp's 163-name `LLM_ARCH_NAMES` - a different registry
that this module never claimed to port. 154 "disagreements" were the distance between two unrelated tables.

## Error two: a regex that ate a hyphen

Rewriting the check against the right table, I transcribed the nine names with
`grep -oE '\{"[a-z0-9]+"'`, which silently skipped **`gpt-oss`**. The check then compared by index and reported
four disagreements - an off-by-one caused entirely by my missing entry:

```
FG_ARCH_SWEEP recipes=8 disagreements=4       <- my transcription
  name=lfm2moe row=5 expected=4
  ...
```

With the hyphenated name restored:

```
FG_ARCH_SWEEP recipes=9 disagreements=0
```

## What the port actually does

Nine recipes in the native order, index-for-index, and `ar_arch(ar_find(name))` returns the same name for every
one of them. That is the whole table, not a subset.

## What this leaves for P9-01

Nothing is owed on the recipe registry. The 163-name `LLM_ARCH_NAMES` table is a **different** artefact belonging
to the model loader, and it is not ported - that stays with P7-12, and the coverage block says so.

## The lesson, and it is the third time this session

The pattern is always the same shape and never the same content: a check whose reference is wrong, or whose
input is incomplete, produces a confident number about nothing. Grammar (permissive workload), Mirostat
(ceiling vs truncation), and now this (wrong table, then a hyphen). The defence that works is reading the port's
own stated source before writing the comparison.
