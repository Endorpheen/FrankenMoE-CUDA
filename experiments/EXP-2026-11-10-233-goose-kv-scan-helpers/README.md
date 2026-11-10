# EXP233: KV scanning helpers in Goose — ACCEPTED on correctness

Three functions that look trivial individually and are each easy to get subtly wrong:

    get_has_shift   result |= cells[s].get_has_shift()   over all streams
    pos_min         first entry of the per-sequence position set, or -1 when empty
    used_max_p1     largest used slot index PLUS ONE, or 0 when nothing is used

The trap in each: an OR loop that assigns instead of accumulating reports only the last stream; a min that
returns 0 for an empty set instead of -1 is indistinguishable from a real position 0; and returning the
largest index rather than index+1 changes how many rows get serialized.

## Gate

    10003 checks, 0 failures

Randomised over 3000 / 3000 / 4000 cases respectively (position sets sorted, as the native container
keeps them ordered and the last element dominates `used_max_p1`), plus explicit edges:
`pos_min` of an empty set is -1, `used_max_p1` of an empty set is 0, and `used = {0}` gives **1**.

Mutations, all caught:

| mutation | failures |
|---|---:|
| OR replaced by assignment | 652 |
| empty `pos_min` returns 0 instead of -1 | 726 |
| `used_max_p1` returns max, not max+1 | 3326 |

## Live

    FG233C_KV_SCAN calls=146
    prefill 1555.4  decode 25.58
    tokens True  probabilities True  sha e68df05b

146 calls — one per `get_has_shift()`, which the update path calls per batch. Witness superset vs
`exp232`: no missing, `new FG233C`. All ten KV routes present in the object before linking:

    fg199k_ fg204x_ fg205g_ fg206d_ fg209i_ fg227n_ fg228e_ fg229i_ fg230s_ fg233c_

## Only `get_has_shift` is routed

`pos_min` and `used_max_p1` are ported and gated but **not wired into a call site yet**. They exist as
building blocks for the serialization work; wiring them without measuring would add a boundary crossing
for a function whose argument is already a nearly-empty set. Recorded so a reader does not assume all
three are active — the witness covers `get_has_shift` only.

## Performance — nothing concluded

**Прогон выполнен при параллельной нагрузке. Показатели производительности могут быть искажены.**
Prefill read 1555.4 here against 25xx in every other run today; that is a 40% collapse, far beyond
anything this component could cause, so the figure is recorded and not used. Decode 25.58 is in the
normal band.
