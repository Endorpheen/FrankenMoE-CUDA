# EXP402: byte-fallback decisions are Goose-owned (APEX.TOKENIZER)

## Transferred

| op | rule |
|---|---|
| 0 | a byte-encoded vocabulary looks a byte up as a one-character string, otherwise the token text uses `<0xXX>` |
| 1 | the two upper-case hex digits of a byte |
| 2 | a byte that resolves to a token is emitted, one that does not is dropped |
| 3 | a word that resolves as a whole is pushed, otherwise the byte fallback runs |

## Gate

| check | result |
|---|---|
| 525 checks over all 256 byte values, both nibbles, both paths and token/null cases | 0 failures |
| mutations | **5/5 caught** |

## Run

`FG402S_BYTE_FALLBACK` live; completion `e68df05b`; restore exact.
