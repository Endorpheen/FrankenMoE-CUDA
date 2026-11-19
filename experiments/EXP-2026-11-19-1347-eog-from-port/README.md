# EXP-2026-11-19-1347: the end-of-generation decision is taken by the port

## What was converted

`llama_vocab::impl::is_eog` is the decision that stops generation. It now asks the ported door, which the shipped
library computes for all 248 320 tokens:

```
FG_TK_EOG_TOTAL  tokens=248320 eog=5 disagreements=0
FG_TK_EOG_SERVED from_port=1
FG_TK_EOG_SERVED from_port=65
FG_TK_EOG_SERVED from_port=129
```

The native set remains as the fallback when the door cannot be resolved.

## The numbers, and why a triple was needed

One run after the change read decode 62.04-62.35, which is **below** the 62.69 the previous checkpoint measured, and a
single number would have been read as a cost of the conversion. Three paired runs:

```
run1 61.33 62.54 62.79 / 2683.1 2934.5 2939.4
run2 62.27 62.88 62.76 / 2677.6 2942.3 2940.7
run3 62.54 62.91 63.06 / 2694.2 2948.3 2950.2

median of nine: decode 62.76   prefill 2939.4
against 62.73 / 2948.0: +0.0 % and -0.3 %
```

So the conversion costs nothing measurable, and the first run's number was the noise this binary has shown all session
(60.4-63.5 across the day). The hash is `c2ff42cc1219` in every run.

## Why this matters beyond the count

Until now every converted decision lived in the loader, on the path that prepares the model. This one is taken **while
tokens are generated**: it is the first generation-time decision the ported code makes. The equality was established
over the whole vocabulary before the conversion (EXP-2026-11-19-1319), which is why a change this central could be
made without a risk period.

## What remains

The detokeniser's sequence form (`bt_detokenize` over ids) is still unmeasured and unconverted; the piece cache and the
buffer path remain native. Both are the next block, with the equality for pieces already established over all 248 320
tokens.
