# EXP215: UTF-8 codepoint decoding in Goose — ACCEPTED

`unicode_cpt_from_utf8` (unicode.cpp:30-74). Every non-ASCII character in a prompt, and every
non-ASCII byte of generated text, is decoded here, and the five validation branches are what reject
malformed input instead of emitting a wrong codepoint.

## Result — ACCEPTED

    FG215U_UNICODE_CPT calls=2003762
    2572.56 / 26.47, tokens and all 128 probabilities exact, sha e68df05b

**2,003,762 calls in one request** — the hottest path ported so far, seven times EXP213's
detokenization count. Text is mostly ASCII here, but the decoder is still entered for every
character examined.

## Balanced interleaved measurement (A = EXP214, B = EXP215)

| pair | prefill | decode |
|---|---:|---:|
| 1 | +1.19% | +0.52% |
| 2 | −0.10% | +2.58% |
| **mean** | **+0.54%** | **+1.55%** |

Inside the 2% rule, with no regression. Pair 2 decode is above 2% on its own in the *positive*
direction, so the honest reading is the same as before: this stage's noise is around 1-2%, and only
the mean is usable.

## Gate

1596 checks / 0 failures over 600 cases: random byte soup (often malformed) mixed with well-formed
encodings of random codepoints excluding the surrogate range. Each case forks so a Goose trap is
observable, and compares three things against a transcription of the native body: **validity**,
the **consumed length**, and the **decoded codepoint**.

Two defects of mine while building that gate, both worth recording:

1. **The fork hid the result.** The child wrote `adv` into its own copy of the variable, so the
   parent always read −1 and 498 cases "failed". Fixed by sending the value back through a pipe.
2. **The gate did not compare the codepoint at all** — only validity and length. A decoder that
   returns the right length and a wrong value would have passed. Added, then verified the gate has
   teeth: mutating the 3-byte mask from `0x0f` to `0x0e` makes it fail with 14 errors.

## Wiring

`integration/unicode.cpp` routes the function when `GGML_GOOSE_UNICODE_CPT=1`. The caller advances
`offset` by the length Goose returns, exactly as the native branches do. Trap codes 701-706 name
which native `throw` site was hit instead of a generic "invalid character".
