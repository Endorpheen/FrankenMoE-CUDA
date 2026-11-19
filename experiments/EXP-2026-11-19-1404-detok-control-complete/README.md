# EXP-2026-11-19-1404: the crash was a stack overflow in my own diagnostic, and the control now passes

## What the crash was

The two failed attempts of EXP-1403 blamed nothing and proved nothing, correctly. The cause was found in the diagnostic
that the short-buffer comparison added in EXP-1356:

```cpp
static thread_local char short_buf[1];
const int32_t nat = vocab->detokenize(tokens, n_tokens, short_buf, text_len_max, ...);
```

A **one-byte** scratch buffer handed to a call that is allowed to write `text_len_max` bytes. The harness reaches
`len_max = 1014`, the native call writes 1015 bytes into that byte, and the stack is smashed. It looked like a fault in the
port for two attempts because it happened in the same place the port is consulted.

That is a real defect in the shipped library, behind `FG_TK_DETOK_DIAG`: any run with the flag set and a short buffer was
undefined behaviour. Fixed by sizing the scratch to `text_len_max + 1`:

```
sequence harness: calls=89      (was: Segmentation fault (core dumped))
```

## The control, third attempt, in the right place

The first two attempts injected into the piece door while the harness exercises the **sequence** door, which is why they
produced no `FG_TK_BUF_DIFF` even before crashing. Injected where it is actually measured - one token fewer to the port:

```
FG_TK_SEQ_DIFF     5 lines
```

and reverted:

```
FG_TK_SEQ_DIFF     0 lines
sequence harness: calls=89
```

## The pair, now complete

| comparison | shown equal | shown able to fail |
|---|---|---|
| encoder, against native ids | 248 320 token texts | yes (EXP-1402) |
| detokeniser, sequence form | 89 harness calls, 24 933 in measurement | **yes** (five DIFFs here) |

## What this cost, and the lesson

Two attempts, a segfault that survived a revert, and a wrong suspicion. The instrument crash was caused by the instrument,
and the only reason it was found was that the crash persisted after the revert - which is exactly the check that
distinguishes a fault in the code under test from a fault in the test.

## Shipped path

Acceptance after the fix: decode 62.72 / 63.20 / 63.12, prefill 2694.2 / 2940.9 / 2951.1, hash `c2ff42cc1219`.
