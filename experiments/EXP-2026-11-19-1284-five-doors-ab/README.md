# EXP-2026-11-19-1284: five sampler doors, all identical - after fixing my own harness twice

```
penalties: identical
typical:   identical
xtc:       identical
mirostat:  identical
adaptive:  identical
```

Each sampler is constructed with the public init, driven with `llama_sampler_apply`, and the resulting array is
compared between the door-enabled and door-disabled runs - values, order and size. `penalties` carries accepted
history through `llama_sampler_accept`; `mirostat` and `adaptive` are applied four times in a row because their
state advances across draws.

## Two harness defects, both mine, in this one card

**First: an unsorted input.** The default chain runs top-k before these samplers, so the array arrives sorted and
flagged `sorted`. The first version fed it unsorted and produced differences that belonged to the harness.

**Second, and worse: the dump ignored the pointer.** `llama_sample_xtc_apply` trims by advancing the array
itself - `cur_p->data += pos_last; cur_p->size -= pos_last`. The harness printed from the original `d.data()`, so
the discarded leading entries appeared in the output as if they had been kept, and XTC looked like it was dropping
the most probable tokens while the native kept them.

The numbers that exposed it are in the header the harness now prints:

```
xtc 0 prob=0.648377 thr=0.121527 mk=1 n_in=30 size=28
```

Both paths return 28 of 30. With `p = 0.586, 0.225, 0.157, 0.015`, `pos_last = 2`, and the native advances by
exactly those two. Reading `cur.data[i]` instead of `d[i]` makes the two runs byte-identical.

## What this says about the three findings before it

This is the third time in this session that a red number accused the code and turned out to be the instrument -
after DRY's mirrored harness and the `g++`-mangled `wrap_ship.c`. The DRY defect was real and the fix stands; but
the majority of alarms have been mine, which is the argument for the pattern now in use: drive the **shipped**
code from both switches, print enough state in the header to settle the dispute with numbers, and check the
printing before checking the port.

## Scope

These are the samplers with a door whose state and array effects can be driven without a model. `temp_ext`
(EXP-2026-11-19-1282) remains open on a genuine contract question about sorting and size.
