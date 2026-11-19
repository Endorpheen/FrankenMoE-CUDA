# The grammar port rejects tokens the native automaton accepts

The shipped path, one constrained request, temperature zero:

```
FG171_GRAMMAR_DIFF cand=4753 id=4754 goose_reject=1 native_reject=0 cp_len=2 pnr=0
FG171_GRAMMAR_DIFF cand=0    id=763  goose_reject=1 native_reject=0 cp_len=2 pnr=0
```

Both disagreements are the same shape: the port **rejects** a candidate the native accepts, on a candidate two codepoints wide
with `pnr=0`. The first call of the run (one candidate, the opening brace) agrees; the divergence starts on the second call,
which sees the whole vocabulary.

That is why the constrained answer changes: a token the native automaton allows is removed from the candidate set, so greedy
decoding picks a different - still legal - continuation. Measured on the same build:

```
off: {"city": "Tokyo", "country": "Japan"}        2 of 2 runs
on:  { "city" : "Dhokar" , "country" : "Nepal" }  2 of 2 runs
```

## How to reproduce

```
bash experiments/EXP-2026-11-19-1471-grammar-overreject/repro.sh
```

It takes the Apex lock, starts the server with `GGML_GOOSE_GRAMMAR=1 GGML_GOOSE_GRAMMAR_VERIFY=1`, sends one grammar-constrained
request and prints the shadow comparison. The verify mode applies the **native** decision, so it cannot corrupt the answer - it
only counts.

## Where to look

`vendor/flint/src/llama-grammar.cpp`, the block that builds `pval`/`pnr` from each candidate's codepoints before
`fg_grammar_reject_call`. A two-codepoint candidate with `pnr=0` is the smallest failing input, and `id=4754` is a concrete id
to test against `llama_grammar_reject_candidates` directly.

## What this does not say

It does not say the port is generally wrong: on 248 071 candidates of the same call it agreed on all but one. It says the
rejection rule differs for a narrow shape, and that shape is enough to change an answer.
