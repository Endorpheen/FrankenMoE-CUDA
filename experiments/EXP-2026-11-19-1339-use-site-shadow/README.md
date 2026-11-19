# EXP-2026-11-19-1339: the loader's key reads are compared at the moment of use

## What was added

`GKV::get_kv` is the single funnel through which `llama_model_loader` reads every typed metadata key - 17 call sites
go through it. It now performs a shadow read through the port for the same key and throws on a disagreement:

```
FG_GGUF_USES compared=16 differed=0
```

Integers compare through `gr_meta_int`, strings through `gr_val_span`, and a converted site that read the wrong field
fails during the load rather than during generation. This is the "comparison at the moment of use" the replacement
plan asked for; the file-level gate could not provide it.

Acceptance with the shadow read compiled in: decode 62.81 median, hash `c2ff42cc1219`.

## The crash it caused first, and what it says

The first run **segfaulted during the load**, right after the integer check printed. Cause: the port records had been
moved into a persistent store so a use-site read could see them, and three comparisons in the file-level gate still
read the moved-from vector - an empty buffer, so `memcmp` on a null pointer.

Two things are worth keeping from that:

1. the rule the report keeps stating - run the thing after a change - is what caught it, and it took one run;
2. a refactor that moves data must be followed by a search for every remaining use of the moved-from object. I fixed
   two sites and missed three, because I patched the ones the first test reached.

## What this closes

The plan's third step, converting `get_key` sites by kind, now has its guard in place before the conversion starts:
every read the loader makes is compared on the real path, for integers and strings alike.
