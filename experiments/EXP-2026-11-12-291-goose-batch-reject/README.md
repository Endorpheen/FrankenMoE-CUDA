# EXP291: batch admissibility checks in Goose (P6-07 increment)

P6-07's evidence covered splitting, ubatch assembly and output mapping, all live. What stayed native was the block that
*refuses* a bad batch. Three of its rules are now Goose.

## The rules transferred

| rule | native site | refusal |
|---|---|---|
| continuity | `llama-batch.cpp`, plain-position block | with a memory present and a known last position `X`, the batch's first position must be `X+1`, and the position span must not exceed the number of tokens the sequence carries |
| decreasing positions | the plain-position walk | positions of one sequence must not decrease along the batch |
| sequence sets | the incompatible-set walk | a sequence's running set intersection must stay non-empty; the module uses a 64-bit mask per token, exact for `n_seq_max <= 64`, which it asserts |

Return value is 0 (admissible) or 1/2/3 naming the rule that refused, so the caller keeps its logging and its `false`
return.

## Gate

    cont: valid continuation / gap rejected / span over count / no memory    OK
    dec: increasing / interleaved seqs / decreasing rejected                OK
    sets: compatible / incompatible rejected                                OK
    checks: 9, failures: 0

Each case is compared with a direct transcription of the native checks. The gate was shown to fail: dropping the `+1`
from the continuity rule makes the valid-continuation case refuse (9/1). Restored, 9/0.

## What remains for P6-07

Integration into `llama-batch.cpp` behind a default-on switch, a relink, and an A/B. The accepted profile never sends a
malformed batch, so the live evidence for these three rules has to come from a forced-invalid case rather than from the
ordinary request -- that is the next step, and it is stated here rather than assumed.
