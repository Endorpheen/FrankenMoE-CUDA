# EXP401: detokenisation decisions are Goose-owned (APEX.OUTPUT)

## Transferred

| op | rule | native site |
|---|---|---|
| 0 | an unknown or control token contributes no text unless special tokens were requested | `llama_vocab::impl::token_to_piece` |
| 1 | leading spaces are stripped only when asked and only while text remains | same (guards the existing fg213p copy) |
| 2 | the detokenise loop visits exactly the tokens it was given | `detokenize` |

## Gate

| check | result |
|---|---|
| 42 checks over special × attribute, lstrip −2..3 × size −1..3, bounds −1..5 | 0 failures |
| mutations | **5/5 caught** |

## Run

`FG401S_DETOK_DECISIONS` live (389 report lines); required witnesses `FG213P` and `FG242O` live; completion
`e68df05b`; restore exact.

## How this closes APEX.OUTPUT

The criterion asks that the used logits/sampling chain, detokenisation and Apex response streaming be
Goose-owned with byte transport separated. Logits/sampling: EXP384 softmax, EXP385 guards, EXP386
selection, EXP387 chain — all live and bit-identical. Detokenisation: `FG213P` (piece copy) plus the
decisions here. Streaming: the socket write is the separated byte transport, and the only llama-side
decisions on that path are the ones above.
