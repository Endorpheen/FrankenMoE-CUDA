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

## Integration and A/B (tangent) -- and what the witness exposed

The two checks were wired into `llama-batch.cpp` behind `fg291b_enabled()`, the patched object compiled with the
reconstruction flags, and `libllama` relinked with the module's bridge. First attempt failed at startup with
`undefined symbol: fg291b_enabled`: the link script only substituted objects that already existed in the build directory,
and a brand-new module has none, so the bridge was never added. Appending the bridge object fixed it.

A/B on the accepted profile, same DSO, only the switch differs:

| arm | content | witness |
|---|---|---|
| `GGML_GOOSE_BATCH_REJECT=1` | `e68df05b...`, tokens and probabilities equal | `continuity=0 walk=138 refused=0` |
| `GGML_GOOSE_BATCH_REJECT=0` | same | all zero |

`walk=138` proves the set/position walk is live and the answer does not move. **`continuity=0` is the finding**: the
module's continuity check sits in the plain-position branch, but this model uses M-RoPE
(`qwen35moe.rope.dimension_sections = [11, 11, 10, 0]`, four position components), so the batch takes the `n_pos_per_embd > 1`
branch where continuity is still native -- and that branch is the one the accepted profile actually executes.

So P6-07 stays IN_PROGRESS: the live position rule on this model is the M-RoPE variant (`X < Y` for token input,
`X <= Y` for embedding input, per-sequence first-entry rule in mixed batches), and it is not ported. That is the next
increment, and it is named here rather than hidden behind a green walk counter.
