# EXP-2026-11-19-1233: the BPE merge comparator, on the live path

After five instrumentations that landed on paths the model does not take, this one is on the BPE path by
construction: the hook sits in `llm_tokenizer_bpe_session::tokenize`, immediately after `pop_move()`, and
checks the relation the queue itself must satisfy.

```
popped(rank_a,left_a) must come before top(rank_b,left_b)  ->  fg_tk_bpe_before(...) == 1
```

```
FG_TK_BPE goose calls=1 disagreements=0
FG_TK_BPE goose calls=2 disagreements=0
FG_TK_BPE goose calls=3 disagreements=0
```

## What it establishes, and how much

Establishes: the ported comparator agrees with the ordering `std::priority_queue` actually applied, on a real
vocabulary, at every pop it was asked about.

Does **not** establish much breadth: three comparisons per run, and a longer, word-heavy prompt did not raise
it. That is a property of the tokenizer, not of the hook - the merge queue is drained a handful of times per
word - so the decision is only lightly exercised here and the synthetic gate (the real `priority_queue` plus
an antisymmetry sweep over 64 pairs) remains the stronger evidence.

## Recorded because the number is small

A counter that moves three times is not a reassurance, and writing "0 disagreements" without the count would
be misleading. The card carries both, per the project rule that a figure is quoted with its sample.

## The check that would have saved five iterations, applied here first

```
strings libllama.so.0 | grep -c FG_TK_BPE     -> present before the run
read which tokenizer the model's pre-type selects -> BPE, and this loop is inside it
then run                                       -> counter moved on the first request
```
