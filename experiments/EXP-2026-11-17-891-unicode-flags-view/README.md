# EXP-2026-11-17-891 — step 1 of the unicode batch: a layout-independent classification view

The unicode batch needs the per-codepoint classification inside Goose without an FFI call per character. Reading
the code (EXP-890, three revisions) settled what that requires, and it was less than expected:

- `unicode_tolower` is already Goose-routed (`fg218t_tolower`);
- the flags table is built once and only read, so it must not move - its address is what a caller needs.

## What was added

`fg_unicode_flags_view()` in `unicode.cpp`, exporting a plain `uint16_t[MAX_CODEPOINTS]` where each entry is
`unicode_cpt_flags_from_cpt(cpt).as_uint()`. `as_uint()` is the struct's own encoder: explicit masks from the enum,
both byte orders handled. So Goose tests documented bits instead of depending on C++ bitfield layout, which the
header itself flags with a TODO.

This is the documented exception for C: a representation change with no logic in it.

## Verified

```
FG_UNICODE_FLAGS codepoints=1114112 stray=0 PASS
```

Not a tautology. The expected classes are written from each character's meaning - `A` letter+uppercase, `z`
letter+lowercase, `7` number, space whitespace+separator, tab whitespace+control, `.` punctuation, `$` symbol,
U+00E9 letter+lowercase, U+4E2D letter, U+0301 accent mark - and then all 1,114,112 entries are checked to carry
no bits outside the documented mask.

## No model run, deliberately

The change is additive: a new function that nothing calls yet. Nothing on the request path changed, so there is
nothing for Apex to confirm, and a run would only produce a number to quote. The model run belongs to the batch,
after the splitters are wired.
