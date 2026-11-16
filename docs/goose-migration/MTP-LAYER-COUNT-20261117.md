# RESOLVED: the MTP layer count, and a harness that was wrong twice

**Resolution:** `apex_plan` was wrong; `gdn_plan` and `stack_dispatch` were right. Fixed and re-verified.

Found on the final sweep of the standalone harnesses. It is recorded as an open question rather than a
defect, because the arithmetic says both readings are internally sound and only one of them can be what the
loader does.

## The disagreement

```
gdn_plan       n=41  ->  10 attn + 30 recr + 1 mtp = 41
apex_plan      n=42  ->  10 attn + 31 recr + 1 mtp = 42
stack_dispatch n=41  ->  10 attn + 30 recr + 1 mtp, pos_bad=0, first_attn=3 last_attn=39
```

The model file declares `block_count = 41` (blocks 0..40). Attention sits every 4th layer from 3 to 39:
`(39-3)/4 + 1 = 10`, which accounts for the 10 in every reading.

The two totals differ by exactly the MTP block:

- `gdn_plan` and `stack_dispatch` pass **41** and count the MTP as the 41st block - present in the file.
- `apex_plan` passes **42** (`n_layer_all = n_layer + 1`, with a comment saying "n_layer_all covers the MTP
  block the loader appends") and counts it as an extra 42nd.

## Why this was not caught earlier

`apex_plan` contains no `PASS` string at all - it prints the plan and never asserts. So it was first misread
by `verify-standalone-apex.sh` as a failure, then reclassified as informational, and in neither state did
anything compare its numbers with `gdn_plan`'s. Two harnesses agreeing on 10 attention layers while differing
by one on everything else went unnoticed because only the first number was ever looked at.

## The question this leaves

**Does the MTP block come from the file, or is it appended by the loader?**

It matters beyond bookkeeping: if the pass loop iterates `n_layer_all` and the loader appends a block that the
file does not carry, the loop will run one layer too many - or read a block that has no weights. Getting this
wrong at composition time would show up as a shape failure in the last layer, which is easy to misread as a
builder defect.

Settling it needs one observation, not an argument: read `blk.40.*` and see whether it is an ordinary
recurrent block or the MTP head. `stack_dispatch`'s `last_attn=39` plus 41 total suggests block 40 is
recurrent and the MTP is not a separate file block - which would make `gdn_plan`'s reading the right one and
`apex_plan`'s `+1` wrong.

## What is not done

Neither harness was changed. Changing one to match the other before knowing which matches the loader would be
guessing with a green gate as the reward.


## Resolution

One observation settled it, as it should have from the start. Reading the tensor names out of the GGUF:

```
blk.40 tensors include: blk.40.nextn.eh_proj.weight, blk.40.nextn.enorm.weight,
                        blk.40.nextn.hnorm.weight, blk.40.nextn.shared_head_norm.weight
blk.41: does not exist
qwen35moe.block_count        = 41
qwen35moe.nextn_predict_layers = 1
```

**The MTP block is blk.40, stored in the file.** Nothing is appended. So `n_layer = block_count - nextn =
40` and `n_layer_all = 41`, giving 10 attention (3,7,...,39) + 30 recurrent + 1 MTP = 41.

That is exactly what `gdn_plan` and `stack_dispatch` reported all along. `gdn_plan` even carries the rule in a
comment; `apex_plan` did not use it.

## apex_plan was wrong in two different ways

| version | expression | result | why wrong |
|---|---|---|---|
| original | `n_layer` (from block_count) **+ 1** | 42 / 10 / 31 / 1 | invented a 31st recurrent layer |
| intermediate | `n_layer_all = n_layer` | 41 / 10 / 31 / **0** | made the MTP block look recurrent |
| **fixed** | `block_count - nextn` and `+ nextn` | **41 / 10 / 30 / 1** | matches gdn_plan and stack_dispatch |

The intermediate is worth recording: my first correction removed the `+1` and made the plan *still* wrong, in
a different way, and it produced a plausible-looking 41. A fix that lands on the right total by the wrong
route is harder to notice than the original error.

## Why it mattered

A composition loop iterating `n_layer_all` with the original `42` would have run one layer too many and read a
block that does not exist. The failure would have surfaced as a shape error in the last layer - the failure
mode that is easiest to misread as a builder defect, which is exactly what this sweep was looking for.
