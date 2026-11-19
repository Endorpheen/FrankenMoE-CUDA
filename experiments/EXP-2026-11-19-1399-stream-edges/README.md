# EXP-2026-11-19-1399: truncation mid-character and cancellation - the two cases EXP-1398 left open

## Truncation in the middle of multi-byte text

A Japanese prompt answered with `n_predict` 2, 3 and 5 - the lengths where the cut lands inside a multi-byte sequence:

```
n_predict= 2  equal  utf8-clean=True  len=4
n_predict= 3  equal  utf8-clean=True  len=6
n_predict= 5  equal  utf8-clean=True  len=10
```

"equal" is the streamed reassembly against the plain answer; "utf8-clean" is checked by the absence of U+FFFD in both,
which is what a split sequence would produce. The ported detokeniser holds a partial character across pieces rather than
emitting its bytes, and both paths show the same truncated text.

## Cancellation

A streaming request aborted after 179 fragments, then:

```
after cancel: http=200 content='**ok**'
server alive
```

The next request is served normally and the process survives, so an abandoned stream does not leave the slot or the
detokeniser state wedged.

## What this closes

P7-07's two named gaps. Together with EXP-1398 - three prompts, 26/49/49 fragments, byte-identical reassembly including
a Japanese answer - the streaming path has evidence for its ordinary case, its truncation edge and its cancellation
edge.

## What it still does not cover

Concurrent slots (the profile runs `-np 1`), a cancel during prefill rather than during decode, and a client that
disconnects without reading the body at all. Those are stated as gaps rather than folded into the verdict.

## Reproduce

```
bash experiments/EXP-2026-11-19-1399-stream-edges/run.sh
```
