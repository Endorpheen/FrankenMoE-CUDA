# EXP359: two concurrent slots exposed a batch-port defect

Two requests sent at once to a server with `-np 2`. Native answers both; the matched candidate dropped both connections.

## The defect

    runtime/reconstructed/src/llama-context.cpp:2286:
    GGML_ASSERT(out_ids.size() == (size_t) n_outputs) failed

Isolation, one file at a time:

| change | result |
|---|---|
| flint's `llama-context.cpp` | still aborts |
| flint's `llama-batch.cpp` | **both answered** |
| `GGML_GOOSE_UBATCH_OUTIDS=0` | **both answered** |

So the ubatch out-ids port (`fg242o`) produced an output-id list whose size did not match the number of tokens marked as outputs, and llama-context
asserted on it. It only appears when two sequences share a ubatch, which is why every single-slot test in this session passed.

## The fix

The port's answer is now accepted only when its count agrees with the native rule, and in addition the port is limited to single-sequence ubatches
(`n_seq_max == 1`). Multi-sequence batches take the native walk. The first attempt -- accepting only on a matching count -- was not enough on its
own, because the second block in the same function alters the sequence index mapping and that is what the later `n_outputs` computation depends on.

## After the fix

- concurrent slots: both prompts return the same content as native, matched by index;
- edge sweep: 6 of 6 still identical;
- sampler differential: 8 of 8 still identical.

## What this says about the merge

This is the second port found broken only under a condition the session had not exercised: the first was slot selection with an empty prompt, this
one is the out-ids mapping with more than one sequence. Both were found by testing shapes rather than by reading symbols, and both now fall back to
native behaviour instead of asserting.
