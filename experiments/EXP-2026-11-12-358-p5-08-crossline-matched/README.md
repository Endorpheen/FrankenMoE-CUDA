# EXP358: cross-line state compatibility on the matched candidate

P5-08 asks whether a state file written by one line can be read by the other. This runs both directions on the matched candidate.

## Result

| quantity | native writes | candidate writes |
|---|---|---|
| file size | 29,692 bytes | 29,692 bytes |
| sha256 (first 16) | `0948f1eea666f80c` | `0948f1eea666f80c` |
| tokens saved | 45 | 45 |

Byte comparison: **0 differing bytes**. The two files are identical, not merely equivalent.

Both directions then restore and continue:

| reader | file | n_restored | cache_n | continuation |
|---|---|---|---|---|
| candidate | native's | 45 | 37 | `. He was very happy. He want` |
| native | candidate's | 45 | 37 | `. He was very happy. He want` |

## What this does and does not settle

It settles cross-line readability for this input: a 45-token state file on a 260K model is byte-identical between the lines and restores in both
directions with the same continuation and the same cached-token count.

It does **not** explain the earlier +780-byte delta. That was measured on a 131 MB Apex-scale state, three orders of magnitude larger, and the
candidate at the time was still the era-mixed build. Whether the delta survives on a matched Apex build is exactly what an Apex run would show,
and it has not been run.

So the honest reading: at small scale the writer now produces identical bytes, which is the strongest possible result for that scale, and the
large-scale question is unchanged and still open.
