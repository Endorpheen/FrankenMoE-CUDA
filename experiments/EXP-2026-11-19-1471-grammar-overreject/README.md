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

## The state the wrong rejection was made in

With the state dump the three failing calls are no longer opaque:

```
FG171_GRAMMAR_DIFF  cand=4753 id=4754 goose_reject=1 native_reject=0 cp_len=2 pnr=0
FG171_GRAMMAR_STATE cps=123,34 stacks=1 pval=34 pnr=0          <- "{" then '"'
FG171_GRAMMAR_STACK st=0 depth=1 head= r0+0(type=3)             <- a character class at rule 0, offset 0

FG171_GRAMMAR_DIFF  cand=0 id=763 goose_reject=1 native_reject=0 cp_len=2 pnr=0
FG171_GRAMMAR_STATE cps=34,58 stacks=1 pval=58 pnr=0            <- '"' then ':'
FG171_GRAMMAR_STACK st=0 depth=1 head= r0+7(type=3)

FG171_GRAMMAR_DIFF  cand=0 id=328 goose_reject=1 native_reject=0 cp_len=2 pnr=0
FG171_GRAMMAR_STATE cps=32,34 stacks=2 pval=34 pnr=0            <- ' ' then '"'
FG171_GRAMMAR_STACK st=0 depth=2 head= r0+11(type=2) r5+0(type=3)  <- a rule reference above a class
```

Every failing candidate is a **two-codepoint piece** decided at a character class, and the second codepoint is one the grammar
allows only after finishing or skipping the current rule (`space ::= " "?` sits between `{` and `"city"`).

That is the shape of the defect: when the codepoint does not match the class at the top, the port rejects, while the native
automaton can finish the current rule - or skip an optional one - and retry the same codepoint in the parent. The third case
shows the stack a rule reference above a class, which is exactly the state where that retry is required.

The next change is therefore in the class branch of `goose/sampling/sampler-chain-full.goose`: on a failed match, advance past
the end of the current rule and retry the same codepoint at the parent before rejecting. The shadow mode is the gate for it -
the disagreement count on this grammar must go to zero, and the pair run must produce the same text with the door on and off.
