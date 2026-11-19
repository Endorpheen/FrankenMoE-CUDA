# EXP-2026-11-19-1402: the encoder comparison falsified - it detects a difference, and serving refuses

## Why this was needed

The encoder comparison has reported `disagreements=0` for many iterations, and the serving path only acts when the two
sides agree. Neither claim means anything until someone shows the comparison **can** report a difference and that
serving **stops** when it does.

## The control

One byte was removed from the text the port sees - a change inside the port's input, not in the file, so the two sides
genuinely differ. Nothing else changed:

```
FG_TK_BPE_DIFF  native=2 goose=3  text_len=11
FG_TK_BPE_DIFF  native=5 goose=6  text_len=20
FG_TK_BPE_DIFF  native=8 goose=7  text_len=21
FG_TK_BPE_DIFF  native=2 goose=3  text_len=18
FG_TK_BPE_COMPARE calls=1 disagreements=1
```

and, decisively:

- **no `FG_TK_BPE_SERVED` line at all** - the serving path refused;
- the counts the caller received were the **native** ones (2, 5, 8, 2), so the native answer stayed in charge.

## After reverting

```
FG_TK_BPE_DIFF      (none)
FG_TK_BPE_SERVED    1
```

Four of four equal again and the port serves - the same state the previous cards reported, now with the knowledge that
the report is capable of saying otherwise.

## What this establishes

| claim | evidence |
|---|---|
| the comparison detects a divergence | four DIFF lines with the exact counts on each side |
| serving is conditioned on equality | zero served calls while they differed |
| a disagreement does not reach the caller | the returned counts were the native ones |

That is the same standard the component gate was held to (EXP-1367) and the GGUF check was held to (EXP-1401): a check
that has never failed has not been shown to work.
