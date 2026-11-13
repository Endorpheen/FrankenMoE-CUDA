# Audit for the sample_ratio class of defect

The MUL_MAT defect took most of a session to find: a body used a destination index directly where
the native kernel divides it by a ratio to broadcast a smaller source. The obvious follow-up is
whether the same shape of mistake exists elsewhere. Checked every module whose body deals with
channel or sample addressing.

| module | ratio usage | verdict |
|---|---|---|
| EXP184 mmvf | `channel_ratio` + `sample_ratio` | fixed this session |
| EXP185 mmvq_q4k | `channel_ratio` + `sample_ratio` | fixed this session |
| EXP186 mmvq_q6k | `channel_ratio` + `sample_ratio` | **was still broken — fixed this session** even though it is opt-in and absent from the active link, so promoting it later cannot inherit the defect |
| EXP168 norm | no ratio; uses `modulo(row/channel/sample, extent)` per broadcast dimension | correct by a different mechanism — a modulo, not a division, so the smaller source wraps rather than running off the end |
| EXP180 mmq_q8_1 | no channel/sample ratio; indexes by `blocks_per_channel` within a channel | not this class |
| EXP166, 167, 170, 171, 174, 175, 176, 177, 178, 179, 181, 182, 183, 188 | no channel or sample addressing | not this class |

EXP186 was the one find, and it was only reachable by checking every module rather than the ones
in the link. That is worth stating: the active link is not the whole tree, and an opt-in module
carries the same risk the moment someone opts in.

## What the two correct approaches look like

- **Divide by a ratio** where native does: `sample_x = sample_dst / sample_ratio`, with the ratio
  computed as `dst->ne[3] / src0->ne[3]` and a guard against zero.
- **Modulo by extent** where native broadcasts by wrapping: EXP168's `modulo(p, row, key+1)`. This
  is a different contract and must not be converted into a division.

Mixing the two up is exactly how the original defect would reappear, so both are recorded here
side by side.
