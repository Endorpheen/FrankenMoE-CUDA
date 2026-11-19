# EXP-2026-11-19-1356: the door's short-buffer answer does not match the native one, and that is why the fallbacks stay

## The observation

While measuring the sequence form with a door buffer of 65 536 bytes for a 32 768-token sequence:

```
FG_TK_SEQ_DIFF  n_tokens=32768 native=184528 goose=-65544
```

The native returned the **needed total**, `184528`. The door returned `-65544`, which is not that number.

## Why the door's code looks right

`dt_detokenize_body` already implements the native accumulation: a piece that does not fit returns `0 - size`, the
caller sets `room = 0` and `total = total - r`, and at the end `total > out.len` yields `-total`. Read against the
native loop, the two are the same algorithm.

So the difference is somewhere the reading does not reach - most plausibly that the pieces after `room` reaches zero do
not accumulate their full size (an empty piece returns 0, and `0 - 0` adds nothing), which would make `-total` the
**room exhausted at** rather than the **total needed**. That is a hypothesis, and this card deliberately does not act
on it: the numbers needed to confirm it are one harness run away, and guessing here would be the fourth time in this
report that a plausible mechanism was wrong.

## Why the fallbacks therefore stay

Both the buffer form and the sequence form fall back to native when the door reports a short buffer, and this
observation is the reason: `-65544` served as if it were `-needed` would tell a caller to allocate 65 544 bytes for a
string that needs 184 528 - a wrong answer that only appears on the resize path, which no successful-call comparison
touches.

## The next measurement

One harness run that calls the door directly with three deliberate buffer sizes - too small, exactly enough, one byte
short - and prints the door's answer beside the native's for each. If the door returns the true total for any of them,
the accumulation is right and the difference is in my earlier flags; if it returns the room exhausted, the port needs
the accumulation fixed before either fallback can go.

## What this does not change

Every decision served by the port today is served on the success path, where equality is measured. The fallbacks are
exactly the case where that equality was never claimed.
