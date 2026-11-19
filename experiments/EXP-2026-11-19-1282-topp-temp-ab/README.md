# EXP-2026-11-19-1282: top-p is exact, temp_ext has an open contract difference

```
topp:    bit-identical including order and size
tempext: OPEN, 4668 lines differ (ordering and size, not values)
```

Both samplers are driven through the public API - one sampler object per case, applied twice, once with the Goose
door on and once off - so the comparison is with the native code and not with a restatement of it.

## Top-P: closed

300 cases, arrays of 2 to 61 candidates, `p` from 0.05 to 0.99, `min_keep` from 0 to 7. Values, **order and array
size** all match after the door runs. The `FG171_TOP_P` counter confirms the door was reached on every case, so the
agreement is between two live implementations, not between native code and itself.

## temp_ext: open, and what the difference actually is

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

## Why it does not block

The fixed acceptance profile sets `temp` with no delta, and the door is only reached when `delta > 0`. The
difference is therefore dormant in the acceptance configuration - the same situation as the DRY defect, which is
exactly why leaving it unrecorded would be wrong.
