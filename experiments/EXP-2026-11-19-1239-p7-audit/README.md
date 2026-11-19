# EXP-2026-11-19-1239: what P7-06/P7-07 still owe, measured rather than estimated

Rule 28 asks for the remaining native total in numbers, so here are the numbers for the tokenizer, which is
the P7 family's subject.

| item | value |
|---|---|
| `vendor/flint/src/llama-vocab.cpp` | 4947 lines |
| C++ container uses in it (`std::string`, `std::vector`, `std::map`) | 189 |
| Goose tokenizer sources | 1576 lines across four modules |
| Goose touchpoints in the native file | 19 lines (two dlsym-resolved doors) |

```
goose/model/bpe-tokenize.goose          760
goose/model/bpe-detokenize.goose        337
goose/model/tokenizer-decisions.goose   324
goose/model/bpe-load.goose              155
goose/model/vocab-type.goose            550
```

## What those numbers do and do not say

They say the ported surface is substantial (1576 lines against a 4947-line native file) and that the native file
still carries 189 container uses - rule 27's "a ported decision inside a C++ host" applies to every one of them.

They do **not** say the port is two thirds done. Line counts are not coverage: the four Goose modules hold
decisions that the native code calls through two doors, and the native file also contains the UGM, WPM, RWKV
and Plamo2 sessions, the special-token table, the detokenizer's cache and the chat-template plumbing. Which of
those are needed for the supported models is the question P7-12 exists to answer, and it is not answered here.

## What is measurable today, and what is not

Measured: the merge comparator agrees with the real priority queue 4156 times per prompt; normalization is
gated and cannot be exercised on a BPE model; the five tokenizer decisions have their own synthetic gates.

Not measurable on this machine: anything that requires a WPM or UGM vocabulary (no such model present), and
anything that requires the detokenizer's cache behaviour under streaming (the server path uses a different
decode routine than the one ported).

## Next concrete step

The detokenizer port has an 11-function interface taking GGUF buffers; wiring it live needs the vocabulary
bytes on the host side, which is a real interface decision, not a hook. Recorded so the next session does not
start by writing another dlsym wrapper for a function whose inputs the host cannot supply.
