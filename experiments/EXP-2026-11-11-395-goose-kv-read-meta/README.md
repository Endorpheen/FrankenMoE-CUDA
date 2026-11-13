# EXP395: KV metadata read is Goose-owned (APEX.KV, part 4)

## Transferred

| op | rule |
|---|---|
| 0 | a sequence-agnostic cell must carry exactly one sequence id, else the restore is refused |
| 1 | the extension block is read only when the cache has extensions |
| 2 | a mirrored layout is usable only when non-empty, single-stream and holding exactly the cells being restored |

## Gate

| check | result |
|---|---|
| 103 checks over n_seq_id 0..3, extension flag, emptiness × streams × cells × wanted | 0 failures |
| mutations | **6/6 caught** |

## Run

`FG395S_KV_READ_META` live (66 report lines, ≈1 600 calls); completion `e68df05b`; restore exact.

## Note on my own script

The staging driver aborted half-way because a `\n` inside a C++ log string was consumed by two layers of
escaping, and the next run executed without the lost-bridge disable and died with
`FG205G validation code 21`. The replacement is now expressed as a single line, which cannot be corrupted
that way. A half-applied staging fails loudly, which is the behaviour we want.
