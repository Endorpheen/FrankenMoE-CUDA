# EXP-2026-11-19-1313: the ported BPE tokenizer agrees with the shipped one on real text, 1678 times

The tokenizer cross-check exists in the binary behind `FG_TK_DIAG`, and it counts the port's decisions against the
shipped implementation on **real** input. This card runs it on text chosen to be hard.

## The input

Mixed scripts and the shapes a BPE comparator is most likely to get wrong: Latin prose, Cyrillic, Japanese, Greek,
Arabic, digits, JSON punctuation, `snake_case`, `camelCase`, `kebab-case`, an e-mail address and a code fragment,
repeated six times to make the prompt long enough for many tokenizer calls.

## The result

```
FG_TK_BPE_PROGRESS calls=500   disagreements=0
FG_TK_BPE_PROGRESS calls=1000  disagreements=0
FG_TK_BPE_PROGRESS calls=1500  disagreements=0
FG_TK_BPE_TOTAL    calls=1678  disagreements=0
```

No diff line of any kind appeared. The total prints at exit, which is the number to quote: 178 of the calls came
after the last progress line, and the total covers all of them.

## What this adds to P7-06

The BPE port had differential evidence against arrays built by a harness and against a canonical implementation. What
it did not have is a count from the shipped server tokenizing real model input - the path where a disagreement would
actually corrupt output. 1678 comparisons with zero disagreements is that count.

## What it does not establish

It covers tokenization, not detokenization or incremental text handling (P7-07), and one prompt is one sample of
text: rare scripts absent from it remain untested. The prompt was chosen for breadth, not for proof of universality,
and the card says so.

## Housekeeping

Server in its own session on port 9101, terminated by group, port checked free afterwards.
