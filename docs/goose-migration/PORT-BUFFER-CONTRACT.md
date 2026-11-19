# What a door may write, and what the host must check

The crash of 2026-11-19 came from a contract, not from arithmetic: the port answered with a piece of 21 bytes and the host
copied it into a 15-byte buffer. The rule below exists so that the next port cannot repeat it, and the list is every place
the shipped runtime lets the port write into memory it does not own.

## The rule

A door that writes into the caller's memory takes that memory as a slice **with its length**, and answers `-needed` when the
content does not fit. The host never copies more than the length it passed: it either passes the caller's buffer directly, or
it copies a bounded amount after comparing the answer with the bound.

A door that is given a window of its own (a scratch buffer) has no idea what the real caller's limit was, so the host must
do that comparison itself. That is exactly the case that was wrong.

## Every site in the shipped runtime

| site | what it does | capacity rule | wrong before? |
|---|---|---|---|
| `llama-vocab.cpp:5295` piece form | the door writes into a private 4096-byte window, the host copies out | returns `-needed` when the answer exceeds `length` | **yes** - it copied without comparing, and that overflowed the caller |
| `llama-vocab.cpp:5386` sequence form | the caller's slice is handed to the door | the door answers `-needed` itself | no |
| `llama-vocab.cpp:5233` encoder | the door writes ids into a host array | `got + 2 <= n_tokens_max` is required before writing; otherwise the native answer stands | no |
| `llama-arch.cpp:1281,1311` registry sweep | the door writes a name into a 128-byte buffer | `sizeof(buf)` is passed and the length is compared with the expected name | no |
| `llama-model-loader.cpp` readers | the door fills vectors the host sized from the port's own counts | the door's count is the size, and a short fill is a refusal | no |

## How each is held

- piece form: `experiments/EXP-2026-11-19-1443-detok-length-contract` - 25 123 short-buffer calls under canaries;
- sequence form: the same experiment, 8 short-buffer calls under canaries;
- encoder: `experiments/EXP-2026-11-19-1442-encoder-served-default` - the served answer equals the native one, and the bound
  is checked by its own condition;
- registry sweep: `FG_ARCH_TOTAL ... disagreements=0` on every model load;
- loader readers: the metadata comparison, 744 231 elements.

## The question that was not asked

Every equality test compared the bytes the port produced. None asked what happens when the caller's buffer is **smaller** than
the answer. That is the gap this document closes, and the reason a port is not accepted on equality alone.
