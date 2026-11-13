# EXP230: `set_input_k_shift` in Goose — ported, gated, NOT exercised by the Apex profile

## Gate

    17270 checks, 0 failures

5 named cases plus 3000 random ones. The two cases that matter:

- `empties` — an empty cell must write **0**, never its stale shift value;
- `ragged` — stream 0 has 1 cell and stream 1 has 5, so the row offset must use each stream's own size
  (`row += sizes[s]`), not a single shared width.

Mutation caught: removing the emptiness check (writing the raw shift) fails 5659 checks. The second
mutation — using `sizes[0]` as every stream's width — does not fail a check but **aborts**: the Goose
module writes past the buffer and the `ensure()` guard traps. That is a detection, but a crude one; the
`ragged` case exists to make the same error visible as a mismatched value instead.

## Not exercised in this profile

    prefill 2556.5  decode 25.32
    tokens True  probabilities True  sha e68df05b
    FG230S_KV_K_SHIFT calls=0

Zero calls. `set_input_k_shift` only runs when the context is shifted, which the fixed 32K/128 profile
never does. Tokens and all 128 probabilities are exact, and the witness superset against `exp229` shows
no missing routes (nine present in the object, verified with `nm -u` before linking).

So this is the same category as EXP218's `unicode_tolower`: correct, gated, and dead in this profile.
It is shipped because the switch defaults to enabled and costs nothing when unreachable, and because a
profile that does shift (long-context reuse) would otherwise need the port written under time pressure.

## Integration note

The bridge pre-reads `shift[i]` and emptiness into two flat arrays per stream, because the cell objects
cannot cross the boundary. That copy is wasted when the route is unreachable, but it happens only inside
`set_input_k_shift`, which is exactly when the values are needed.

Guards: `n_stream <= 64` and `cells.size() <= 65536`; above either, the native loop runs.
