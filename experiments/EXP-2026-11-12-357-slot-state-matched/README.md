# EXP357: slot state save and restore on the matched candidate

P5-08 territory, exercised without Apex: save a slot's state, erase it, restore it, and check the continuation is unchanged.

## Method

Tiny model, one slot, `--slot-save-path`. Three POSTs: `/slots/0?action=save`, then `?action=erase`, then `?action=restore`, with a completion
before and after using `cache_prompt`.

Two API details cost a round each and are worth recording:

- without `--slot-save-path` the endpoint answers **501 Not Implemented**;
- the body takes a **filename**, not a path -- an absolute path yields **400 Bad Request**.

Both lines behaved identically at every step, including both errors, which is itself a small compatibility result.

## Result

| arm | saved | erased | restored | first completion | second completion |
|---|---|---|---|---|---|
| native | 17 | 17 | 17 | `. He likes to sh` | `. He likes to sh` |
| matched candidate | 17 | 17 | 17 | `. He likes to sh` | `. He likes to sh` |

Same token counts at every step, same continuation before and after the round trip.

## Liveness

With witnesses enabled, the save also reports:

    FG345W_KV_STATE_WRITE first call calls=1

so the merged state-write bridge is the code that produced the file, rather than the path merely working by accident.

## Scope

This is a 17-token state file on a 260K model. It says the merged candidate saves, erases and restores a slot consistently with native on this
input; it says nothing about the 787 MB restore or the cross-line compatibility questions that P5-08 actually asks, which remain open.
