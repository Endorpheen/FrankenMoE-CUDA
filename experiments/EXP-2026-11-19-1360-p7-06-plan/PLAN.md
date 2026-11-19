# P7-06: the encode decision - what exists, what is missing, and the order to close it

## What exists

| piece | where | state |
|---|---|---|
| BPE encoder core | `goose/model/bpe-tokenize.goose`, `export fn bt_bpe` | written and exported; 989 lines with its helpers |
| vocabulary load for it | `btl_load` | written; used by the CLI |
| dependency closure | `bt_build`, `bt_attrs`, `bt_special_order`, `bt_eog_ids` | exported, same module |
| a runnable entry point | `integration/goose-standalone/cli/tokenize-main.c` -> `frankenmoe-tokenize` | written, built by `tools/build-standalone-runtime.sh`, **no binary currently exists** |
| comparator agreement on the real vocabulary | `FG_TK_DIAG` at the queue's pop site in `llama-vocab.cpp` | measured, 1678 comparisons, 0 disagreements |
| the encode decision in the shipped library | - | **native** |

So the port is not absent: it is complete enough to be a program, and it has never been compared against the shipped
library on the same input, nor wired in.

## What is missing, in order

1. **Build the CLI and compare it with `llama_tokenize`.** One fixture vocabulary would not be representative; the real
   Apex vocabulary and a mixed-script prompt set is. This is a build plus one run, and it produces the number P7-06
   currently lacks: the encoder's agreement on the model that matters.
2. **Serve the encode path from `bt_bpe` in `llama_vocab::tokenize`.** `bt_bpe` takes nine scratch arrays (`cpts`,
   `seg`, `enc`, `sym`, `heap`, `hc`, `info`); their sizes must come from the native call's own bounds rather than
   guessed - the same rule the GGUF reader followed, where a guessed accessor index was caught by a probe on a known
   value.
3. **Compare before serving.** Same discipline as every door on the decode side: shadow comparison at the call site,
   then the port's answer, then the acceptance hash.
4. **Then remove the native encoder**, per rule 15, only after steps 1-3 produce equal output on the acceptance prompt
   set.

## Why this order and not another

The encoder decides the prompt's token ids, so a wrong answer changes everything downstream while looking plausible -
the same failure class as the DRY orientation defect, which was silent and on a default path. Measuring it on the real
vocabulary before serving it is the only way to know the difference is absent rather than invisible.

## Bound on the work

`bt_bpe` is one exported function with a fixed scratch shape, so serving it is a call-site change, not a new port. The
uncertainty is the scratch sizing (step 2), and it is bounded by the native call's own limits.
