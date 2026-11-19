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

## The measurement, run

One sequence of 129 ids, detokenised with deliberate buffer sizes, the door's answer beside the native one:

```
len_max=8    goose=-18    native=-1015
len_max=64   goose=-69    native=-1015
length needed 1015, returned -1015 by both llama_detokenize calls at any size <= 1014
```

So the door **under-counts the shortfall**. `-1015` is the full length; `-18` and `-69` are what it accumulated before
it stopped. The mechanism I named as a hypothesis - pieces contributing nothing once the room is gone - is what the
numbers show, and it is now a measurement rather than a guess.

## Why the two agree anyway

`llama_detokenize` falls back to native when the door reports short, so callers always receive `-1015`. The
disagreement is invisible today and would become visible the moment the fallback were removed - which is exactly why
it stays until the accumulation is fixed.

## The fix this defines

`dt_detokenize_body` must accumulate the size of **every** remaining piece once the room is exhausted, as the native
loop does, so `-total` is the whole needed length rather than where it stopped. The harness above is the check: after
the fix it must print `goose=-1015` beside `native=-1015` for every `len_max <= 1014`.

## The earlier measurement note

One harness run that calls the door directly with three deliberate buffer sizes - too small, exactly enough, one byte
short - and prints the door's answer beside the native's for each. If the door returns the true total for any of them,
the accumulation is right and the difference is in my earlier flags; if it returns the room exhausted, the port needs
the accumulation fixed before either fallback can go.

## What this does not change

Every decision served by the port today is served on the success path, where equality is measured. The fallbacks are
exactly the case where that equality was never claimed.
