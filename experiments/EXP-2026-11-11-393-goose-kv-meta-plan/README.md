# EXP393: KV metadata field selection is Goose-owned (APEX.KV, part 2)

## Verdict

**LIVE on the reconstructed tree.**

## Transferred

| op | rule | native site |
|---|---|---|
| 0 | a cell lists every sequence it holds for a whole-context write, and only the requested one otherwise — and only when the cell actually holds it | `state_write_meta` |
| 1 | the extension block is written only when the cache has per-cell extensions | same |

The field order (position, count, extension, ids) is untouched; what moved is the selection.

## Gate

| check | result |
|---|---|
| 43 draws over scopes −1..3 × sequences 0..3 × ownership, plus the extension flag | 43 checks / 0 failures |
| mutations | **5/5 caught** |

## Run

| check | result |
|---|---|
| completion before / after restore | `e68df05b…` both |
| restore | succeeds |
| `FG393S_KV_META_PLAN` | **live, 66 report lines (≈1 600 calls)** |
| `FG392S` / `FG345W` / `FG241U` | live |

Metadata that is selected differently would produce a different slot file and a failed restore; both the
file size and the post-restore answer are unchanged.
