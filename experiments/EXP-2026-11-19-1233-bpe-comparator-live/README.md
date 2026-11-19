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

## The first numbers in this card were wrong, and the fault was reading, not measuring

I first wrote "three comparisons per run" from output that said `calls=3`. That line is the **ordinal of the
call being reported**, printed only on the first three calls and every 20000 - it is not a total. After adding a
progress line every 500 and an `atexit` summary:

```
FG_TK_BPE_PROGRESS calls=4000 disagreements=0
FG_TK_BPE_TOTAL    calls=4156 disagreements=0
```

**4156 comparisons, zero disagreements** - on a real vocabulary, at every pop it was asked about. The earlier
"three" was my misreading of my own instrument, and it made the evidence look far weaker than it is.

The same fault affected the prefix counter: `FG_SP_PREFIX calls=5` was the fifth call, and its total line reads
`FG_SP_PREFIX_TOTAL calls=14 disagreements=0`.

Every counter in this session now prints a `_TOTAL` line at exit, so a figure like this cannot be misread
again. That is the fix: not a better reader, a counter that states its own total.

## What it establishes

The ported comparator agrees with the ordering `std::priority_queue` actually applied, 4156 times per prompt,
on real tokens. Together with the synthetic gate (the real queue plus an antisymmetry sweep over 64 pairs),
the decision is verified from both sides.

## The check that would have saved five iterations, applied here first

```
strings libllama.so.0 | grep -c FG_TK_BPE     -> present before the run
read which tokenizer the model's pre-type selects -> BPE, and this loop is inside it
then run                                       -> counter moved on the first request
```
