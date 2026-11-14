# EXP561: dump visited sets by depth (P7-08 slice 6, part 6)

## Finding

EXP559 modelled the native dump's `visited` set with two arrays chosen by the
parity of a slot. That only holds at Tag depth 1: a Tag entered from the second
area passes the *other* array together with the *first* slot number, so the
counter and the array disagree, the reset clears somebody else's area, and a
`[cycle]` that native prints can disappear (or appear by accident).

Native semantics (`common/peg-parser.cpp`): `visited` accumulates along the walk
and is never cleared on return; entering `Tag` starts a **new empty set**; leaving
`Tag` leaves the parent set untouched.

## Fix

One array, one cursor, and a base index passed down:

- a node is looked up only in `vis[vbase .. st[71])`;
- on a miss it is appended at `st[71]` and the cursor grows;
- children keep the same base;
- `Tag` sets `base = st[71]`, walks its child with that base, then restores
  `st[71] = base`.

No parity, unlimited nesting depth, and `vis2` becomes unused.

## Gate

```
FG561_RUNTIME_VISITED cases=678 values=1158 failures=0 skipped=0
mutations 6/6 caught (range 0:6)
mutations 6/6 caught (range 6:12)
baseline after restore: rc=0 cases=678 values=1158 failures=0 skipped=0
```

Six new constructions: a repeat inside one Tag, a repeat across a Tag boundary
that must not print `[cycle]`, a repeat inside a nested Tag at depth 2, a shared
subtree outside a nested Tag, and repeats at Tag depths 4 and 5. Together with the
existing shared-subtree, nested-tag and permute cases that is 678 cases. Expected
dumps come from `common_peg_arena::dump`.

Twelve mutations: the search ignoring or emptying the area window, reading the
wrong array, appending at the wrong index, a cursor that does not advance, a Tag
area started at zero, one below, or kept from the parent, a missing cursor
restore, a negative root base, the arena size used as the window, and a different
cycle marker.

## Repository note

EXP560 was built on a copy of the canonical runtime taken before EXP559 was
synchronised, so the EXP560 commit dropped `permute` and the cycle-aware dump from
`goose/jinja/runtime.goose`. This slice rebuilds the canonical file from the
EXP559 runtime, adds the base AST mapper from EXP560, applies the depth-based
visited fix, and is gated on both the permute and the depth constructions.
