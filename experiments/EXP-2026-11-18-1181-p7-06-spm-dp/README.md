# EXP-2026-11-18-1181: P7-06 score-path DP step

The score-based tokenizer path fills positions backwards: a candidate piece at `i` costs
`scores[i + len] - score`, and it replaces the current choice only when **strictly** better.
An unknown piece (score `-1`) also bills the UTF-8 byte count of the codepoint, and the native
loop **stops** at that piece rather than continuing past it - both facts are easy to lose.

```
spm dp: cases=6 fails=0
```

Six cases: a better piece winning on cost, a longer piece being cheaper, an exact tie (which
must keep the first candidate, proving the comparison is strict), an unknown piece arriving
last, and the unknown byte-count bump for a two-byte codepoint.

The gate holds scores, length, token id and token count for both implementations and requires
all four to agree, not just the chosen candidate - a wrong byte count with the right token
would otherwise pass.

The DP driver over all positions is not gated yet; it is a loop over this step, and the
remaining risk there is the table's suffix search, which needs the vocabulary table rather
than a synthetic one.
