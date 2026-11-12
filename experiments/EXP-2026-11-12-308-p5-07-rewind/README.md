# EXP308: rewind witness on the model, and what near-capacity now depends on

P5-07's inventory left three things open: a rewind/branch model run, a near-capacity run, and the scope decision on the server
prompt cache. This pass closes the first.

## Rewind, measured

Same server, one slot, greedy decoding with a fixed seed:

    B alone            -> 1da0f49942ac7858
    A (longer prompt)  -> a244f9347208948d
    A, then B          -> 1da0f49942ac7858   (identical to B alone)

So issuing a shorter prompt after a longer one on the same slot produces exactly the continuation a fresh server produces. The
slot reuse path -- erase of the previous sequence, re-decode of the shorter prefix -- does not leak state from the longer request.

## Branch

Already witnessed by the state round trip: a 131 975 288-byte state written by this line is restored by a fresh process and
continues identically (`1da0f499`), with the file byte-identical to the native line's in EXP293.

## Near-capacity: now dependent on the open scheduler defect

The near-capacity run needs a KV cache filled to the point where the split/defrag path engages. On this line a 371-token prompt
already aborts inside the scheduler's CUDA split (P6-02, blocker written), so a near-capacity run would hit the same defect and
measure nothing. This clause is therefore blocked on P6-02 rather than on missing work, and is recorded that way instead of
being attempted and produced as a crash.

The remaining non-blocked item is the scope decision on the server prompt cache, which is with the owner in
docs/goose-migration/OWNER-DECISIONS-20261112.md.
