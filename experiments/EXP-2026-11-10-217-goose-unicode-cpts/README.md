# EXP217: whole-string UTF-8 decoding with recovery in Goose — ACCEPTED

`unicode_cpts_from_utf8`. Six pre-tokenizer call sites use it (unicode.cpp lines 222, 241, 359,
499, 647, 783), so it covers the whole prompt and every piece of text the tokenizer re-examines.

## Result — ACCEPTED

    FG217C_UNICODE_CPTS calls=255553 bytes=3163646
    2564.79 / 26.60, tokens and all 128 probabilities exact, sha e68df05b

255,553 calls over 3.16 MB of text in one request.

## Balanced interleaved measurement (A = EXP216, B = EXP217)

| pair | prefill | decode |
|---|---:|---:|
| 1 | +1.18% | +1.46% |
| 2 | −0.15% | −1.14% |
| **mean** | **+0.52%** | **+0.16%** |

Inside the 2% rule, no regression.

## What actually had to be ported: the recovery policy

The interesting part is not the decode — EXP215 already did that. It is what native does when the
bytes are **invalid**:

- advance by exactly **one byte**, not by the length the lead byte claims, and emit **U+FFFD**;
- never fail the string. Native catches `std::invalid_argument` here on purpose so it cannot escape
  `llama_tokenize`.

So EXP215's trapping decoder could not be reused as-is. This module carries a decoder returning
`-1` instead of trapping, identical in every range, overlong and U+10FFFF check, plus the loop.

## Gate

1524 checks / 0 failures: 200 well-formed random strings, 300 pure random-byte strings (where the
recovery path dominates), and 8 truncation/continuation shapes. Compares the **whole output vector**
against a transcription of the native decoder plus native's own catch loop, and asserts nothing is
written past the returned count.

Two mutations, both caught:

| mutation | failures |
|---|---:|
| U+FFFD → U+FFFE | 266 |
| advance 1 byte → 2 bytes on error | 228 |

## Three of my own defects before the gate went green

1. **`adv as u64` passed a value, not an address** — instant segfault, because the callee wrote to
   absolute address 0. The scratch slot has to be real memory: it is now a `int32_t scratch[1]` on
   the bridge's stack frame, per call, so it is also thread-safe.
2. **`let adv` then assigned** — Goose rejects reassigning a `let`; needed `var`.
3. **`gg_function_1` arity changed** when the scratch parameter was added, and a `sed` fix left
   stray text in the bridge. Rewritten cleanly rather than patched further.

## Wiring

`integration/unicode.cpp` routes when `GGML_GOOSE_UNICODE_CPTS=1`. No exception can escape, which
is what native's catch guarantees. The `reserve(utf8.size())` above stays correct because every
iteration consumes at least one byte, so the count can never exceed the input size.
