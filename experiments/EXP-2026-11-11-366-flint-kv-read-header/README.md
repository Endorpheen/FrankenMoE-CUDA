# EXP366: KV state-read header decisions, bridge gated and a format constraint found

The read path's eight header checks as a Goose decision, and the reason it cannot simply replace them.

## Verdict

**BRIDGE GATED; INTEGRATION BLOCKED BY THE FORMAT.** The decision is exact, but wiring it as one call
would change the order in which the payload is read.

## What the module decides

Layer count, capacity, V layout, key rotation, value rotation, then per layer the key type and row
size and the value type and row size — the working line's order, which differs from the recurrent
variant (there `s_trans`, here `v_trans`; no PLE section here). Output: first rejection code or zero
with two per-layer read masks.

| check | result |
|---|---|
| 5 000 draws, codes 1–9 reachable, masks compared | 5 072 checks / 0 failures |

## Why it is not wired

Native reads, per layer: key type, key row, **then the key payload**, value type, value row, **then
the value payload** — and only then moves to the next layer. A single decision call needs every
header first, which moves all payload reads after all headers. The bytes read would be the same set in
a different interleaving, so payloads would land in the wrong tensors unless the format changed.

The alternatives are worse: a call per layer turns the module into a four-comparison duplicate of the
code it is meant to replace, and changing the section layout would break compatibility with files the
accepted runtime has already written (EXP364's 387 MB slot file among them).

Recorded as a format constraint, not as missing work: this block is not a candidate for a live call
site under the current layout.
