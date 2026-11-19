# EXP-2026-11-19-1362: the penalty door on a profile that uses it

## Why

The recorded gap was "the penalty door has no live counter because the profile leaves the repeat penalty at its
default" - the acceptance profile does, so no request in it ever reaches the door. This run leaves the acceptance
profile on purpose.

## The run

`--repeat-penalty 1.15 --repeat-last-n 256 --temp 0.8 --top-p 0.95`, one request over HTTP:

```
FG171_PENALTIES goose calls=1 hn=17
```

The door is reached and the port's logits are the ones used (`kept == cur_p->size`). `hn = 17` is the history length
the repetition penalty was given, which is the value the native path would see for the same request.

## The empty answer, explained rather than ignored

The response body was `content: ""` with `finish_reason: "length"` and a populated `reasoning_content`:

```
reasoning_content: "The user wants me to count from one to five in words. ... One, two, three, four, five."
```

This model answers in a reasoning channel first, and 48 tokens ended inside it. Nothing to do with the penalty door -
but an empty answer is exactly the shape a real defect takes, so it was read before being written down.

## What the counter can and cannot say

It prints at the first call and every hundredth, so one line proves the door was reached and does not count how many
times. For a live-call witness that is enough; for a rate it is not, and the card says which it is.

## Closure

The penalty door now has a live counter on a profile that exercises it, which was the gap in P6-10's record. The
acceptance profile still leaves the penalty at its default, so the acceptance hash remains comparable to EXP1298.
