# EXP400: the slot file is byte-identical across two runs

## Question

Do the Goose-owned metadata, layout and payload paths write the same bytes every time, on the same input?

## Method

Two ordinary runs of the reconstructed tree on the fixed profile, each saving the slot, and a comparison
of the file digests. No switch changes between runs.

## Result

| run | slot size | sha256 |
|---|---|---|
| 1 | 387 989 920 | `3e216c49941d30998115a8b41909a59619dd934bc94ea4d6db07b724f1b6f8a1` |
| 2 | 387 989 920 | `3e216c49941d30998115a8b41909a59619dd934bc94ea4d6db07b724f1b6f8a1` |

**Identical.** Both runs also produced completion `e68df05b` with equal probabilities and an exact
restore.

## Why it matters for the three items

A per-span offset error, a different sequence-selection rule or a different delegation order can each
produce a file of the same size with different bytes. Byte identity across independent runs means the
whole write path — KV metadata selection (EXP393), KV layout (EXP394), recurrent payload spans (EXP396)
and the hybrid delegation order (EXP397) — is deterministic and reproducible, not merely "restores fine".

## Limit

This compares the Goose path with itself. It does not prove byte identity against the accepted runtime's
file, which would need a save on that binary; that comparison is still open and is recorded as such.
