# EXP-2026-11-19-1398: streamed and non-streamed output are byte-identical, including multi-byte text

## Why this is the right test for P7-07

The detokeniser is served by the port, and the streaming path is where its output is cut into fragments. A piece
boundary that splits a multi-byte character, or a chunk that loses a piece, would show as a difference between the same
request answered with and without `stream`.

## The run

Same prompt, same seed, temperature 0, 48 tokens, once plain and once streamed; the streamed deltas are concatenated and
compared byte for byte:

```
equal    chunks= 26  plain= 77 chars  q='Say hello in Russian.'
equal    chunks= 49  plain=178 chars  q='Write one line of Python.'
equal    chunks= 49  plain=156 chars  q='Name two colours in Japanese'
```

The third prompt produces Japanese, so the multi-byte case is exercised rather than assumed - 49 fragments reassemble
into exactly the 156 characters the plain request returned.

## What it establishes

For these three requests, the ported detokeniser's output survives the streaming path unchanged: fragment boundaries do
not split a sequence incorrectly and nothing is dropped.

## What it does not

Three prompts and 48 tokens each is a sample, not a sweep; text that ends mid-character (truncation by `n_predict`) and
cancelled requests are not covered. The item stays open until those are, and the card says so.

## Reproduce

```
bash experiments/EXP-2026-11-19-1398-stream-boundaries/run.sh
```
