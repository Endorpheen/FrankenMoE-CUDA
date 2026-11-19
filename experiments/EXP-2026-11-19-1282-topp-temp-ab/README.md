# EXP-2026-11-19-1282: top-p is exact, temp_ext has an open contract difference

```
topp:    bit-identical including order and size
tempext: bit-identical including order and size
```

Both samplers are driven through the public API - one sampler object per case, applied twice, once with the Goose
door on and once off - so the comparison is with the native code and not with a restatement of it.

## Top-P: closed

300 cases, arrays of 2 to 61 candidates, `p` from 0.05 to 0.99, `min_keep` from 0 to 7. Values, **order and array
size** all match after the door runs. The `FG171_TOP_P` counter confirms the door was reached on every case, so the
agreement is between two live implementations, not between native code and itself.

## temp_ext: closed, and it was my input that was wrong

**Correction.** This card first reported temp_ext as OPEN with 4668 differing lines. That was the harness, not the
port: the array was fed **unsorted**, while the chain hands these samplers a sorted array flagged `sorted`
because top-k runs first. `llama_sampler_temp_ext_apply` calls `softmax_impl(cur_p, true)`, which sorted the
array in place, so position-by-position comparison read the native's own sort as a difference. With the honest
input the two runs are identical in values, order and size.

The same correction was needed in EXP-2026-11-19-1284, where the dump also ignored `cur_p->data` being advanced.
Both are recorded rather than quietly fixed, because the pattern matters: three findings in this session accused
the code and were instruments, and one (DRY) was real.

### The original observation, kept for the record

The first comparison showed 4668 differing lines, which looked alarming. Sorting each case's lines before
comparing reduced it to 42 cases with genuine value differences, and those turned out to be cases where the native
had also **changed the array**: `size=1` on one side against a full array on the other.

`llama_sampler_temp_ext_apply` calls `llama_sampler_softmax_impl(cur_p, true)`, which sorts the caller's array and
can truncate it. When the Goose branch is taken it returns early, so the door rewrites the logits in place and
leaves order and size untouched. Both compute the same temperature; they differ in what they do to the array they
were given.

That is a contract question, not an arithmetic one: it needs a statement about what a sampler may do to the
caller's array, and the answer decides whether the door should sort and truncate too. It is recorded as OPEN
rather than resolved by guesswork, and the harness prints it on every run.

## Why the earlier version was retired

The fixed acceptance profile sets `temp` with no delta, and the door is only reached when `delta > 0`. The
difference is therefore dormant in the acceptance configuration - the same situation as the DRY defect, which is
exactly why leaving it unrecorded would be wrong.
