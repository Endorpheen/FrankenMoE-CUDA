# EXP-2026-11-19-1261: the workload file format, parsed in Goose and compared with the real loader

The previous attempt at this was thrown away because its interface packed a position and a value into one
integer. This version has two explicit readers - `wl_word` and `wl_num` - that write their result through an
out array and return separately, which is what makes it readable and reviewable.

```
workload parse: cases=9 fails=0
```

Nine crafted files, each written to disk and read by the **real** `DeterministicWorkload::load()` (white-box
include - the class needs only `fstream`/`sstream`), compared with the ported rule index:

| case | native rule |
|---|---|
| valid | 0 |
| bad magic | 1 |
| bad version | 1 |
| bad metadata | 3 |
| truncated prompt | 4 |
| truncated output | 5 |
| wrong routes key | 6 |
| route with `n_ids != top_k * token_n` | 7 |
| truncated route ids | 8 |

## Two things the run corrected in my understanding

1. **There is no rule 2.** The native code fuses magic and version into one condition ("unsupported header"), so a
   bad version is rule 1, not a separate rule. My port had invented a rule boundary that does not exist.
2. **`n_ids` must equal `top_k * token_n`**, not merely be present: my "valid" file failed because the count
   disagreed with the two numbers beside it. That check is the one real piece of structure the format enforces
   on itself, and it is now covered.

Both were defects in my test data and my rule numbering, not in the ported logic - and both were caught because
the comparison is against the compiled loader.

## Interface note

The rule indices are this harness's own numbering after (1): 0,1,3,4,5,6,7,8. The gap at 2 is deliberate and
documented, rather than renumbering to look tidy and losing the correspondence with the native conditions.
