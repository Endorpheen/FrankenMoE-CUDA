# EXP-2026-11-19-1321: sequence detokenization - two attempts, both inconclusive, both mine

## What the second attempt fixed

The first comparison reported the native side empty at offset zero because I reconstructed the buffer start as
`text - total` while `text` had been advanced. This attempt captured the start pointer explicitly
(`const char * const text_begin = text;` at the top of the function) and compared from there.

## What it reported

```
FG_TK_DETOKSEQ_DIFF n=523 native=1590 goose=1590 at=516 n_ctx= g_ctx=@example.com. Th
```

Equal length, first difference at byte 516, and the native byte there prints as nothing - a NUL - inside a
detokenised prompt. Either the native buffer really contains a hole (which `total` says it does not), or my comparison
is still measuring the wrong thing.

## Why it is not being interpreted

Three failures in this thread have all been in my measurement code, and this one has the same smell: a NUL in the
middle of text that the same function reports as 1590 valid bytes. Calling it a port defect would be the exact
mistake the session keeps paying for. The hook was reverted rather than kept, so no run reports a diff it cannot
explain.

## State after the revert

```
per-token piece     FG_TK_DETOK_TOTAL calls=248320 disagreements=0
end of generation   FG_TK_EOG_TOTAL    tokens=248320 disagreements=0
sequence form       unexercised
```

## What a correct third attempt needs

A reference that cannot be misaddressed: compare the **return value** at the public `llama_detokenize` entry, or
build the expected string from the per-token pieces that are already proven equal (concatenate them with the same
leading-space rule) and compare against the port's sequence output. The second route needs no native buffer at all,
which removes the class of error that has now failed twice.

## Status

The sequence door is the only detokeniser door without evidence, and the honest label for it is "not measured",
not "broken".
