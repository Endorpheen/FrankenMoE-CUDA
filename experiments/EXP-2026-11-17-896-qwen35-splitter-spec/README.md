# Specification: unicode_regex_split_custom_qwen35, the one splitter the pinned profile runs

Extracted by reading `unicode.cpp:800-926`, so the Goose module can be written from a complete rule list instead of
by re-reading 127 lines each time. The order matters: each rule is tried top to bottom and the first match consumes
the token.

## Interface

```
input   cpts     uint32 codepoints of the whole text (unicode_cpts_from_utf8, called once)
        offsets  per-segment lengths in codepoints (the "words" the caller already split)
        flags    uint16 per codepoint from fg_unicode_flags_view; 0 means out of range
output  bpe_offsets  length of each emitted token, in codepoints
```

Per segment `[offset_ini, offset_end)`:
- `_get_cpt(pos)` returns `cpts[pos]` or `0xFFFFFFFF` outside the segment;
- `_get_flags(pos)` returns the flags entry or `0` outside;
- `_add_token(end)` emits `end - prev_end` when positive and moves `prev_end` to `end`.

The `OutOfRange` codepoint (`0xFFFFFFFF`) is how every lookahead is bounded; a Goose implementation must keep that
sentinel rather than reading past the segment, because rules 2, 4, 7 and 8 all look one past the end.

## The rules, in order

1. **Contraction, one letter** — `(?i:'s|'t|'m|'d)`:
   current is `'`, next lowercased is one of `s t m d` → emit through `pos+2`.

2. **Contraction, two letters** — `(?i:'re|'ve|'ll)`:
   current is `'`, and lowercased `(pos+1, pos+2)` is `re`, `ve` or `ll` → emit through `pos+3`.
   Requires `pos+2 < offset_end`, so this is only reachable when the first rule did not fire.

3. **Word** — `[^\r\n\p{L}\p{N}]?[\p{L}\p{M}]+`:
   if current is not `\r`, not `\n`, not a number, and (current is letter or accent mark, **or** the next is accent
   mark or letter) → advance once, then keep advancing while letter or accent mark; emit.

4. **Single number** — `\p{N}`: current is a number → advance once; emit.

5. **Symbol run** — `<space>?[^\s\p{L}\p{M}\p{N}]+[\r\n]*`:
   let `f2` be the flags of `pos+1` when current is a space, else the current flags. If `f2` is not whitespace,
   letter, accent mark or number **and** the current flags are non-zero → skip the space if any, then advance while
   `f2` stays non-word, then advance over any `\r`/`\n`; emit.
   The `flags.as_uint()` condition is what prevents an out-of-range position (flags 0) from matching here.

6. **Whitespace ending in a newline** — `\s*[\r\n]+`:
   count whitespace from `pos`, remembering the position just after the last `\r`/`\n`. If any newline was seen →
   set `pos` there and emit.

7. **Trailing whitespace** — `\s+(?!\S)`:
   more than one whitespace **and** the codepoint after the run is not out of range (i.e. the run does not reach the
   segment end) → advance `count-1`, emit.

8. **Whitespace** — `\s+`: any whitespace → advance by the count, emit.

9. **Fallback** — nothing matched → consume one codepoint and emit.

## The three ways this can be got wrong silently

- **Rule order.** Rules 6, 7 and 8 all describe whitespace and are distinguished only by newline presence and by
  whether the run reaches the segment end. Reordering them changes tokenisation without producing an obviously wrong
  string.
- **The `flags.as_uint()` guard in rule 5** and the `!= OutOfRange` guard in rule 7 both exist to stop a
  position past the segment from matching a rule about whitespace or symbols. Flags of out-of-range positions are
  0, which reads as "not whitespace, not letter" and would match rule 5 if the guard were dropped.
- **Rule 3's lookahead.** A word may start with a single non-letter, non-digit, non-newline character (the `?` in
  the regex) before the letters. Dropping it splits `"xfoo` into three tokens instead of two.

## Verification plan

A differential gate over a corpus that separates these cases: contractions in both cases (`'S`, `'ll`), words
starting with a symbol, digits adjacent to letters, `\r\n` inside and at the end of whitespace runs, a whitespace
run reaching the segment end (rule 7 must not fire), and CJK plus combining marks. Then one Apex run - this
splitter is the Qwen family's, so the pinned profile exercises it and the reference hash is real evidence.

## Why only this one

Eight sibling splitters exist for other architectures and none of them run here (EXP-893, EXP-895). Porting this
one is verifiable end to end; the other eight would be ported-but-unverified, which is exactly the state
`can_reuse` is in and exactly what EXP-892 showed can hide a behaviour change behind a green hash.
