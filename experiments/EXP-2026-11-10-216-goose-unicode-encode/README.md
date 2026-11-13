# EXP216: UTF-8 codepoint encoding in Goose — ACCEPTED

`unicode_cpt_to_utf8` (unicode.cpp:1262-1290), the exact inverse of EXP215's decoder. It runs when
text is assembled for output and when the tokenizer rebuilds a word from codepoints, so a wrong
length or a wrong continuation byte corrupts generated text rather than failing loudly.

## Result — ACCEPTED

    FG216E_UNICODE_ENC calls=268
    2551.65 / 25.49 first run, tokens and all 128 probabilities exact, sha e68df05b

268 calls per request against EXP215's 2,003,762 — encoding happens on far fewer characters than
decoding, because most output bytes are copied rather than re-encoded.

## Balanced interleaved measurement (A = EXP215, B = EXP216)

| pair | prefill | decode |
|---|---:|---:|
| 1 | +0.97% | −1.73% |
| 2 | −0.14% | −0.44% |
| **mean** | **+0.41%** | **−1.09%** |

Inside the 2% rule with no regression. Two pairs is the minimum this stage's noise allows.

## Gate

1617 checks / 0 failures over 800 random codepoints — half drawn below U+110000 and half up to
U+120000 so the out-of-range branch is exercised — plus the nine explicit boundaries
(`0, 0x7f, 0x80, 0x7ff, 0x800, 0xffff, 0x10000, 0x10ffff, 0x110000`). Both the byte count and the
bytes themselves are compared against a transcription of the native function.

Verified the gate has teeth: changing the 3-byte lead mask from `0xe0` to `0xe1` makes it fail with
27 errors.

## Wiring

`integration/unicode.cpp` routes the function when `GGML_GOOSE_UNICODE_ENC=1`. Goose writes into a
4-byte stack buffer that the host owns; a zero length is native's empty result for a codepoint above
U+10FFFF.
