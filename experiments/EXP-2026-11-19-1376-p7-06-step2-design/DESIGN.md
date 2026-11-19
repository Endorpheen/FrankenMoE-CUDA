# P7-06 step 2: what serving `bt_bpe` needs, and what is missing today

## The door's inputs

`bt_bpe` takes the text, the GGUF blob, and three arrays describing the **tokens** (`tok_off`, `tok_len`, `tok_slots`)
plus three describing the **merges** (`mrg_off`, `mrg_len`, `mrg_slots`), an output slice, and seven scratch arrays.

`btl_load` builds the slot tables from those arrays, which is exactly what the standalone CLI does before it calls the
engine.

## What the library already has

| input | where it exists today |
|---|---|
| the GGUF blob | the loader (`llama-model-loader.cpp`) has it, and the detokeniser table already keeps a copy |
| token strings and offsets | the same table: `off`, `len`, `attr` - built for `bt_token_to_piece` |
| **merge strings** | **nowhere.** The vocab keeps `bpe_ranks`, a hash map of `(first, second) -> rank`; the raw merge text is consumed at load and dropped |

So the encode door is one input away, and it is the one the detokeniser never needed.

## The two ways to supply it, and the cheaper one

1. **Keep the merge KV from the loader.** The loader reads `tokenizer.ggml.merges` as a string array; copying it into
   the shared table costs about 2.5 MB once and gives the port exactly the bytes the vocabulary was built from - the same
   "one owner, accessors for readers" arrangement the detokeniser uses.
2. **Reconstruct the merge text from `bpe_ranks`.** Cheaper in memory and wrong in principle: it assumes every merge is
   `first + " " + second` with no exception, which the loader never had to assume.

Option 1 is the one to take, and the assumption behind option 2 is worth **testing** anyway, because if it holds the
port could later be fed from the vocab alone - which is what a runtime with no loader would need.

## The pieces, in order

1. the loader copies the merge KV into the shared table beside the token arrays (one owner, as before);
2. a `resolve_bpe()` next to `resolve_detok()`, and slot arrays built once by `btl_load`;
3. a **shadow comparison** at `llama_vocab::tokenize`: the native ids are used, the port's are compared, a counter and
   the first five disagreements printed under a flag;
4. only if that is clean: serve the port's ids, keep the native path behind the flag, run the acceptance triple;
5. then the decode-side fallbacks and the encode path are both from the port, and the repeated removal pass becomes
   possible.

## Why this is not started in the same commit

Steps 1-2 are a few hundred lines of allocation and index construction against arrays whose sizes must come from the
loader's own bounds, not from a guess - the rule that the GGUF reader taught when a guessed accessor index produced a
plausible wrong answer. A half-built table would compile, link, and compare nothing, which is the failure mode this
project has already paid for twice.
