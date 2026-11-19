# EXP-2026-11-19-1320: the sequence detokenizer comparison failed in my measurement again; reverted

## What was attempted

The last unexercised detokenizer door, `bt_detokenize` over token sequences, compared inside
`llama_vocab::impl::detokenize` with the flags that function uses: `remove_special`, `unparse_special`,
`add_space_prefix`, `add_bos`, `special_bos_id`, `add_eos`, `special_eos_id`.

## What it reported, and why the report is worthless

```
FG_TK_DETOKSEQ_DIFF n=523 native=1590 goose=1590 at=0 native_ctx= goose_ctx=The quick brown fox jump
```

Equal lengths, first difference at offset 0, and the native side printed **nothing** at that offset - which means the
first byte of the buffer I decided was the output was a NUL. A detokenized prompt cannot begin with a NUL. My
pointer arithmetic (`text - total`, with `text` advanced byte by byte through the loop) does not point where I assumed
it does, so the comparison compares garbage against real text.

Third instrument failure in this thread, all in my measurement code, none in the port.

## What was done

`llama-vocab.cpp` restored from the committed revision and everything re-verified, including the two checks that do
work:

```
FG_TK_DETOK_TOTAL  calls=248320 disagreements=0     (per-token piece)
FG_TK_EOG_TOTAL    tokens=248320 disagreements=0    (end of generation)
acceptance         decode 62.48 / 62.64, hash c2ff42cc1219
```

## The lesson this thread keeps repeating

Printing the two sides before interpreting a count is what caught every one of these. The check exists; the
measurement had to be fixed first. Here the fix is structural rather than incremental: compare at the level where the
native function **returns a string to its caller**, not inside the function where the buffer pointer moves.

## Status

Detokenizer: per-token and end-of-generation proven over the whole vocabulary; sequence form unexercised.
Next attempt: hook `llama_detokenize` (the public entry) rather than the impl loop.
