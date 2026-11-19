# EXP-2026-11-19-1383: MTP's memory need was its own ubatch, and what is left after removing it

## What the 2368 MiB actually was

The MTP draft context is created with `common_context_params_to_llama(params)` - the **target's** context parameters,
including `n_ctx 262144` and `n_ubatch 4096`. At 4096 the compute buffer for prefill activations is 2368.12 MiB
(2483159040 bytes, from the failing `cudaMalloc`), which does not fit beside the target's 9511 MiB on a 12 GiB card.

The head's own work is `1 + n_draft` tokens per sequence, a few dozen. So the buffer was sized for work the mechanism
never does.

## Three sizings, three outcomes

| MTP context sizing | result |
|---|---|
| `n_batch 4096`, `n_ubatch 4096` (as inherited) | **cannot start**: `allocating 2368.12 MiB ... cudaMalloc failed: out of memory` |
| `n_batch 64`, `n_ubatch 64` | starts, then `get_sub_batch: failed to add token 78503 at index 64` - the draft context is also fed the prompt in one add, so its batch must allow that many tokens |
| `n_batch 4096` (unchanged), `n_ubatch 512` | **starts and fits**, then `decode: Compute error. off = 0, n_batch = 4096, ret = -2` |

So the memory question is answered: about 2.3 GiB of the requirement was the prefill buffer, and shrinking the
micro-batch removes it from the profile without touching the profile. What remains is **not** a memory limit but a
decode-path failure inside the draft context, which is an ordinary defect to chase and not a hardware wall.

## What this changes about MTP on this machine

Before: "MTP does not fit the accepted profile." That was true of the inherited sizing and misleading as a conclusion -
it made a mechanism look impossible when its own configuration was the problem.

Now: MTP fits, and the remaining blocker is bounded to one call site and one error code.

## The rule this was done under

Rule 35: the target profile is untouched - context 262144, `-b 4096 -ub 4096`, `--moe-cache-mib 4992` all as accepted.
Only the draft context's `n_ubatch` changed, which is the mechanism's own overhead. `FG_MTP_SMALL_BATCH=0` restores the
inherited sizing for an A/B.

## Next

1. read what `ret = -2` means for that context type and whether the draft pipeline expects `n_ubatch == n_batch`;
2. if it does, the fix is inside the draft feed - split the add into micro-batches - not a parameter;
3. then the first in-profile MTP measurement, with the acceptance rate it produces.
