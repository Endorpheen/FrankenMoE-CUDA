# EXP-2026-11-19-1283: the token-selection door, compared by what it selects

```
dist: 8000 draws identical, door reached (81 counter lines)
```

## What is compared

Not a formula - the **selected token**. Forty probability distributions, 200 draws each from a fixed seed, the
Goose door on and off, and the chosen token ids compared in order. The counter prints at call 1 and every 100th
thereafter, so 81 lines means the door handled all 8000 draws: the agreement is between two live implementations
of the same decision.

This is the door every sampled token passes through, which makes the drawing order the thing most worth checking.
The native code consumes RNG words in a specific order and hands them to the door; a port that guessed the order
would produce plausible tokens that differ from the native run, and nothing downstream would notice.

## What it adds to the record

The remaining items on this port were "RNG stream", "chain connected to the shipped library" and "proof of real
calls". These 8000 draws are simultaneously all three for the selection path: the stream is the same, the library
is the one the server links, and the counter is the witness that the door was reached rather than bypassed.

## Scope

Two doors exist on this path (`dist_words` and `dist_pick`); the counter exercised is the `dist_words` one, which
is the branch taken when a distribution is applied. `dist_pick` belongs to the other entry and is not covered by
this card.
