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

## M-RoPE continuity wired -- and two defects of my own

The M-RoPE variant is now ported: a sequence takes the strict rule (`X < Y`) when its first entry is a token and the
equal-allowing rule (`X <= Y`) when it is an embedding; in a mixed batch the first occurrence in token order decides.

Integration defects found by running it rather than by reading:

1. the first attempt segmented on the M-RoPE path with `seq_set[i].to_ullong()` and `is_embd_vec[i]` read unconditionally.
   `is_embd_vec` is sized for the new batch format and can be shorter than `batch.n_tokens` on the legacy path, so the
   read ran past the end and the server died with SIGSEGV during the first decode. Both reads are now bounds-guarded
   (`(size_t) i < vec.size()`), and an out-of-range entry contributes a zero mask / non-embedding flag.
2. the bridge had to define `fg291b_check_mrope_impl` from a regenerated copy of the module: the copy the bridge included
   predated the new entry, so the symbol was undefined and only lazy binding kept the failure out of the startup path.

A/B after the fix, same DSO:

| arm | content | witness |
|---|---|---|
| `GGML_GOOSE_BATCH_REJECT=1` | `e68df05b...`, tokens and probabilities equal | `continuity=0 walk=138 mrope=138 refused=0` |
| `GGML_GOOSE_BATCH_REJECT=0` | same | all zero |

`mrope=138` is the live proof: the accepted profile takes the M-RoPE branch, and both the set/position walk and the
M-RoPE continuity rule run there. The plain continuity module stays gated but dormant on this model -- that branch is
only taken by non-M-RoPE models.
