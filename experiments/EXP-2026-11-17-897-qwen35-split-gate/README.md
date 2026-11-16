# The qwen35 splitter port is written, gated, and NOT correct yet - and it is not wired

State, recorded plainly so the next session does not mistake it for finished work.

## What exists

- `goose/misc/qwen35-split.goose` - the nine-rule state machine from EXP-896's specification, typechecking;
- `fg_unicode_split_qwen35_ref` in `unicode.cpp` - a test-only seam exposing the static native function by
  codepoints, so the gate compares against the real implementation instead of a copy;
- `tests/gate.c` - 30 cases chosen to separate the rules (contractions in both cases, words after symbols, digits
  beside letters, `\r\n` inside and at the end of whitespace runs, runs reaching the segment end, CJK, combining
  marks);
- the module and its bridge are compiled into the library.

## Result

```
Q35S_GATE cases=30 mismatches=20 FAIL
```

Not wired: nothing in `unicode.cpp` calls `q35s_split`, so the library's behaviour is unchanged and the reference
hash is unaffected. An incorrect implementation sitting in the build but outside the call path is inert; an
incorrect implementation on the call path is the EXP-892 mistake.

## Two defects already found and fixed by the gate

1. Packet fields were read with the 32-bit accessor, truncating pointers - the walk became unbounded and the gate
   hung. Fixed by adding a 64-bit load for packet fields.
2. `n_off` was read at byte 8 instead of 24, so the segment count came from the codepoint length. Fixed.

Both were found because the gate exists. That is the argument for the gate, not against it.

## The remaining divergence, first data point

```
"hello world"   native [5, 6]      goose [9, 2]
```

The first token covers nine codepoints instead of five, i.e. rule 3's "keep advancing while letter or accent mark"
did not stop at the space. Either the flags value read for U+0020 is not what `fg_unicode_flags_view` holds (the
view was separately verified to give `0x0108` for space), or the index into it is wrong for the walking position.

That is one specific question with a one-line experiment, not a mystery: print the flags the module sees for
`"hello world"` at each position and compare with `fg_unicode_flags_view()[c]`. Deferred here only because the
session's remaining context is not enough to fix, re-gate and verify properly - and an unverified fix is the thing
this file exists to prevent.

## What is not claimed

Nothing was removed, nothing was wired, no speed claim is made. C++ removed by this work: **0 lines**. The native
splitter remains the only implementation on the request path.
