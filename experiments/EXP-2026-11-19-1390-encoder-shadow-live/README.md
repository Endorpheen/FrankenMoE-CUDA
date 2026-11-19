# EXP-2026-11-19-1390: the encoder now runs at `llama_tokenize`, and its one disagreement is the special tokens

## Four things were wrong, three of them fixed

| problem | evidence | state |
|---|---|---|
| the per-element accessor made the table build quadratic | no progress for 20 000 elements in 180 s | fixed by `gr_arr_str_offsets`, a bulk door that fills offsets into the file's window in one pass |
| the call was declared with C++ linkage inside a function | `check-unresolved-symbols: FAIL _Z13fg_bpe_encodePKclPll` | fixed, gate green |
| the GGUF check itself never finished on a real vocabulary | `FG_GGUF_DIAG` alone timed out after `FG_GGUF_STRINGS` | fixed: its array section now uses the bulk door; the check completes |
| the port has no special-token pass on this path | measured below | **open** |

The third one is worth its own line: the GGUF reader's verification was **unusable on a real vocabulary** and nobody had
noticed, because acceptance runs do not enable it. It compared every element of every string array through the
per-element door - 248 320 of them - which is the same quadratic walk. It now runs to completion.

## The live comparison

With `FG_TK_BPE_DIAG`, every `llama_tokenize` call compares the port's ids with the native ones:

```
case 0  "Hello world"                  native 2   port 2   equal
case 1  "Привет, мир"                  native 5   port 5   equal
case 2  "def f(x): return x**2"        native 8   port 8   equal
case 3  "<|im_start|>system"           native 2   port 7   DIFF
FG_TK_BPE_COMPARE calls=1 disagreements=0   (counter printed before case 3)
```

Three of four equal, and the one that differs is exactly the case the standalone CLI had to be taught: the port's
`bt_bpe` tokenises the text as ordinary pieces, while the native tokenizer matches special tokens **before** BPE. The CLI
gained that pass in EXP-2026-11-19-1375; the library path goes straight to `bt_bpe` and does not have it.

## What that means

The comparison is live and it is doing its job - it found, on the first run, the one known gap between the CLI and the
library path. The fix is to move the special-token pass to where both callers share it, which is the port itself (a
`bt_detokenize`-style door that takes the special order and the text and emits ids), not to reimplement it in C++ at the
call site - that would be the adapter debt rule 17 forbids.

## Shipped path

Unchanged: the comparison and the table live behind `FG_TK_BPE_DIAG`, and an acceptance run with the flag unset gave
decode 62.52 / 63.13 / 62.96, prefill 2689 / 2945 / 2948, hash `c2ff42cc1219`.

## Next

1. one door in Goose that tokenises text with specials, using the order `bt_special_order` already produces;
2. re-run this harness: four of four, then a wider corpus through the server;
3. only then serving, and only then the acceptance triple that proves nothing regressed.
