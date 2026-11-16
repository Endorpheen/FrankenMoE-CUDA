# The unicode splitter batch: what it actually requires

Pre-port analysis of the batch chosen in README.md (nine `unicode_regex_split_custom_*` functions, ~1000
lines). Reading the code changed the plan, which is the point of doing this before writing any Go.

## The shape

`unicode_regex_split_custom_qwen35` is a hand-written regex state machine. Per character it does three things:

```
const uint32_t cpt = _get_cpt(pos);                    // read from a codepoint vector
const auto flags   = _get_flags(pos);                  // unicode_cpt_flags_from_cpt(cpt)
uint32_t next      = unicode_tolower(_get_cpt(pos+1)); // case fold
```

The split decision - where a token boundary goes - is the state machine. The inputs to that decision are
classification: `unicode_cpt_flags_from_cpt` resolves a codepoint to letter/number/whitespace/punctuation flags
through `unicode_cpt_flags_array`, a 31-line table in the same file.

## Why that matters for the batching decision

If only the state machine moves to Goose, every character costs an FFI call to ask C++ what class it is. Pre-
tokenisation runs over the whole prompt, so that is a per-character bridge on a hot path - the same mistake as
routing `is_swa(il)` through FFI for a coverage counter (EXP-890 README), which I rejected for the same reason.

So the batch is not "nine splitters". It is **the flags table plus the nine splitters**, and both have to move
together for the batch to be performance-neutral:

| piece | lines | why it cannot be left behind |
|---|---|---|
| `unicode_cpt_flags_array` + `unicode_cpt_flags_from_cpt` | ~31 + lookup | called per character by every splitter |
| `unicode_tolower` | small | called per character, ASCII+Latin fold |
| the nine `unicode_regex_split_custom_*` | ~1000 | the decisions themselves |

The table is data, so it moves cleanly: as a table in Goose it is a lookup, not a translation. The splitters then
call Goose-to-Goose, with one FFI at the entry (the text buffer in, the offsets out), not per character.

## What would have gone wrong

Starting with `_qwen35` alone, because it is the one the pinned profile exercises, would have produced a working
end-to-end result on Apex and a permanent per-character FFI cost on the tokenizer path for every architecture -
visible in throughput, invisible in the reference hash. The reference answer would have stayed correct, which is
exactly why this had to be settled by reading the code rather than by running it.

## Revised batch

1. Move the flags table and `unicode_tolower` to Goose as one module, with a differential gate comparing every
   codepoint in 0..0x10FFFF against the native function - exhaustive, cheap, no model needed.
2. Move the nine splitters into the same module, driven from the table, with a per-splitter differential gate
   over a corpus that exercises each architecture's distinguishing rules (CJK, combining marks, emoji, digit
   runs, apostrophe contractions).
3. One Apex run for the whole batch, with `_qwen35` as the end-to-end witness.

No part of step 1 or 2 needs a model run, which is what makes the batch affordable to do in one pass.
