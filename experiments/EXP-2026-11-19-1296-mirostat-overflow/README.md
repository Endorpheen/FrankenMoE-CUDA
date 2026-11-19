# EXP-2026-11-19-1296: the residual is an `int` conversion in the native path, and the port is the safer side

## The measurement

Tracing mirostat's truncation width per round in both modes, over 100 rounds:

```
rounds where k differs        75 of 100
  native k = 1, port k = 1e6   in the majority of them
port k > 1e6                  59 rounds
native k > 1e6                17 rounds
largest native k              1 027 758 848
```

## What the native does with those values

`std::max(int(k), 1)` converts a float that has reached about a billion into an `int`. Values near a billion still
fit, but the float has long since lost whole units, and on the rounds where the conversion range is exceeded the
result is **undefined behaviour**; on this platform it clamps to 1. The native run then keeps **one** candidate where
the port keeps all of them.

## What the port does

```
while ki + 1 <= k_f && ki < 1000000 { ki = ki + 1; }
```

The bound keeps the loop finite and the value representable. It cannot reproduce a conversion whose result is
undefined.

## What this means for the requirement

On mirostat the port cannot be token-for-token equal to the native in the configurations where the native is
undefined. What can be verified is what was verified: the port is bounded, deterministic, and equal to the native
wherever the native is defined. Reproducing the rest would mean carrying undefined behaviour into the Goose runtime.

## What it explains

The 42 size differences seen at adaptive-p's entry in the chain containing mirostat: with k clamped to 1 the native
passes one candidate on, with the cap the port passes them all. That closes the chain of attributions in this
session - flag, then survivor count, then this.
