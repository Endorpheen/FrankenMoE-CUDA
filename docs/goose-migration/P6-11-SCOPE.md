# P6-11 scope: MTP, speculative decoding and draft state

Surveyed 2026-11-18, before any port. Numbers are from the repository, not estimates.

## Surface

| item | size / location |
|---|---|
| speculative runtime | `common/speculative.cpp`, 3113 lines |
| interface | `common/speculative.h`, 125 lines, `common_speculative` opaque handle |
| MTP references | `src/llama-model-loader.cpp`, `src/llama-model.cpp`, `src/llama-quant.cpp` |

The module owns: draft model lifecycle (`common_speculative_are_compatible`,
`common_speculative_n_max`), the ngram cache implementation, type parsing and
configuration selection, and the acceptance/rejection loop.

## What the port has to preserve

1. draft state ownership: shared versus separate draft model, and state borrowing;
2. accept/reject decisions per proposed token, including the RNG consumption order -
   this interacts directly with the sampler chain and with the fixed-seed guarantee
   established in P6-10;
3. rewind and recovery of the draft state after a rejection;
4. the no-speculative mode, which must remain the default path and must not regress.

## Dependencies

- the sampler chain must be the accepted P6-10 build, because acceptance consumes the
  same RNG stream and the same candidate arrays;
- the ngram cache is shared with P9-05, so the two items must agree on one interface
  rather than each carrying its own;
- nothing here needs the grammar stage, so P6-10's grammar decision does not block this.

## Proposed slice order

1. draft state ownership and the compatibility check, gated against the native
   `common_speculative` API;
2. accept/reject with a fixed seed, gated by identical token sequences against native;
3. rewind and recovery, gated by state comparison after a forced rejection;
4. ngram draft, coordinated with P9-05;
5. live Apex generation with MTP after the plain path is accepted.

Nothing is ported yet; this file records the scope so the next iteration starts from the
repository instead of from a new investigation.

## Correction after reading the acceptance path (2026-11-18)

The plan assumed the classic probability-ratio acceptance test inside the speculative
module. In this tree it is not there: `common_speculative` exposes
`accept(seq_id, n_accepted, is_other)`, and `n_accepted` is computed by the **caller**
after the target model verifies the drafted batch (`tools/server/server-context.cpp`
carries the per-position counters). The decision is therefore target-verification, not a
ratio test:

- the draft proposes `k` tokens;
- the target runs them and produces its own sampled token per position;
- the accepted count is the length of the prefix where the draft and the target agree.

Consequences for the port: slice 2 is not "port the ratio test" but "port the prefix
comparison and the state updates it drives". The comparison itself is a few lines; what
matters is that it consumes no RNG of its own, which is good news for the fixed-seed
guarantee established in P6-10.
