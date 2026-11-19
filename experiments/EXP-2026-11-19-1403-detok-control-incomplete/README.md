# EXP-2026-11-19-1403: the detokeniser's falsification was attempted twice and did not complete

## What was tried

The encoder's comparison was falsified in EXP-1402 by feeding the port one byte fewer; the same standard was owed to the
detokeniser, whose comparison has reported zero differences since it was written. The fault: ask the piece door for a
different token than the native call, so the bytes must differ.

| attempt | fault | result |
|---|---|---|
| 1 | `token + 1` | **segfault**, no `FG_TK_BUF_DIFF` |
| 2 | `token - 1` when positive | **segfault**, no `FG_TK_BUF_DIFF` |
| - | reverted, rebuilt | **segfault again**, same place |

Attempt 1 is explained: `token + 1` walks past the array and the door has no bounds guard of its own, so asking out of
range is undefined. Attempt 2 stays in range and still crashes, and - decisively - **the crash does not go away when the
fault is reverted**. So the fault is not the cause, and reporting it as one would be exactly the mistake this report keeps
correcting.

## What is true of the library

The shipped runtime is healthy after the revert:

```
decode 62.72 / 63.17 / 63.30   prefill 2692.5 / 2943.3 / 2948.7   hash c2ff42cc1219
accepted components: ok
```

## What is not established

The detokeniser comparison **has still never been shown to fail**. Its zero is therefore still a number, not a proof, and
it stays listed as such. The concrete next step is to find why the harness `/tmp/seq_harness` crashes after its
short-buffer section - it is a stale binary built before the last few library changes - and rebuild it before attempting
the control a third time.

Two attempts is where the repository's blocker rule says to stop and report, so that is what this card does.

## The honest summary of the pair

| comparison | shown equal | shown able to fail |
|---|---|---|
| encoder, against native ids | yes, 248 320 token texts | **yes** (EXP-1402) |
| detokeniser, piece and sequence | yes, 24 933 calls in measurement | **no** - attempted twice, blocked on a crashing harness |
