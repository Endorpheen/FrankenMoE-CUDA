# What is in the link line (P3-20 measurement)

P3-20 asks that no replaced hand-written CUDA logic remain in the candidate link. That is not yet
true, and this records the exact composition so the gap is a number rather than an impression.

From `runtime/exp185/build/commands.json`, the active link line for `libggml-cuda`:

    174 object files
    119 the rest of the CUDA backend, untouched (acc, argsort, allreduce, the 24 fattn-mma
        template instances, conv*, cross-entropy*, dsv4, ...)
     43 Goose objects (module bodies, host bridges, experiment-local core copies)
     12 privatised native objects, one per replaced module:

        concat, convert, cpy, fattn-host, fwht, getrows, mm-ids, mmvf, mmvq,
        moe-weighted, mmq-q8-1, set-rows

## Reading

- The **12 privatised objects are the rollback path**, not leftovers. Each is the original body
  of a module that Goose now replaces, renamed to a private symbol and reachable only through
  that module's switch. Deleting them would delete the rollback the rules require.
- The **119 untouched objects are the un-ported surface**, and they are the same families the
  census quantifies: `mul_mat_q` at 40% of kernel time is inside `mmq.cu.o`, `quantize_mmq_q8_1`
  in `quantize.cu.o`, and the `fattn-mma-*` instances are the prefill attention variants this
  profile never runs.
- Goose's own share is 43 objects against 12 replaced — the port is one module per file, not a
  rewritten tree, and that ratio is what P3-20 would have to change to call this closed.

## What closing P3-20 would require

Either the blocked families get ported — which needs the emitter to type addresses, the standing
blocker — or the link is allowed to drop the 12 privatised objects, which removes the rollback the
rules mandate and is not a decision for this work to take.

So P3-20 cannot be closed from here, and the reason is the same one that keeps `mul_mat_q` native.
Recorded so the claim "P3-20 verified by truncation" is never made on the strength of the 12
replaced objects alone.
