# EXP213: detokenization piece copy in Goose — ACCEPTED

`_try_copy` inside `token_to_piece` (llama-vocab.cpp:3980-3994). Every generated token goes through
it: strip up to `lstrip` leading spaces, then either copy the remaining bytes or report how many
would be needed. The negative return is the "buffer too small, retry bigger" signal, so a wrong sign
or count truncates output text rather than failing loudly.

## Result — ACCEPTED

    FG213P_PIECE_COPY calls=321941
    2501.59 / 25.47 first run, tokens and all 128 probabilities exact, sha e68df05b

321,941 calls in one request: this is the hottest path ported so far by call count.

## Measurement, and why the first verdict was wrong again

Single sequential runs suggested a regression (−2.61% prefill, −3.13% decode) and I did **not**
accept that. EXP212 had just taught me that sequential runs at this stage cannot separate code from
environment. An interleaved A/B sequence:

| pair | prefill | decode |
|---|---:|---:|
| 1 | +0.04% | −0.33% |
| 2 | +0.32% | +2.37% |
| **mean** | **+0.18%** | **+1.01%** |

Parity. The −3% was the environment, exactly as with EXP212. Recorded so the pattern is visible:
**at this stage no single run may be used to accept or reject a stage.**

## Gate

1271 checks / 0 failures over 500 randomised pieces with random `lstrip` and buffer capacities from
−4 to 27, comparing both the returned code and the copied bytes against a transcription of the
native lambda, plus explicit empty / overflow / two-space cases.

## Wiring

`integration/llama-vocab.cpp` routes `_try_copy` through the module when
`GGML_GOOSE_PIECE=1`; the engine owns both buffers, Goose decides the skip and the result code. No
intermediate copy is made, which is why this one does not repeat the EXP212 marshal cost.
