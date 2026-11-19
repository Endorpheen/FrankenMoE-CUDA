# EXP-2026-11-19-1371: the compatibility door, measured and still unwired

## Why it had no evidence

`fg_sp_compatible` is exported, implements `common_speculative_are_compatible`, and is resolved nowhere - not by a
direct call, not by `dlsym`, not by another Goose module (EXP-2026-11-19-1370). Its route runs only when a **draft
model** is loaded, and there is none on this machine, so a live comparison could not run even if it were wired.

## What was measured

The door is a pure function of thirteen integers, so the harness calls it directly and compares with the native rule
written out from the source: equal vocab type; equal `add_bos` and, when set, equal bos id; equal `add_eos` and, when
set, equal eos id; and `|n_vocab_t - n_vocab_d| <= max_diff`.

The grid flips **one field at a time** by -2..+2 around a compatible baseline, so a wrong field is identifiable from the
failing case rather than averaged away, and does it at three values of `max_diff` including zero.

```
compatibility door: calls=180 disagreements=0
```

## What the door does not cover, and why the comparison does not either

The native function ends with a per-token **text** comparison over the smaller vocabulary. The door takes no text, so it
cannot answer that part, and the harness does not claim it: the door's answer is "compatible modulo the text check".

That is why wiring the door changes nothing by itself: the call site would have to keep the text loop, and the door
would decide only the leading guards. When a draft model exists, the right wiring is the door for the guards and the
vocab comparison for the text - and this card is the evidence the guards half is right.

## Reproduction

```
bash experiments/EXP-2026-11-19-1371-compatibility-door-measured/verify.sh
```

No model load, one second.
