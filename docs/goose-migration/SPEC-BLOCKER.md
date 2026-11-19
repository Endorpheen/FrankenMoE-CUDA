# Blocker report: the speculative path is unreachable from the server

For the senior helper, per the project rule about a repeated blocker.

## Expected

A server started with `--spec-type ngram-simple --spec-ngram-simple-size-n 3 --spec-ngram-simple-size-m 8`
enters the speculative verification block in `tools/server/server-context.cpp`, reaches the accept
dispatch, and with `temp = 0` calls `common_sampler_sample_and_accept_n`; instrumentation there prints
per drafted token.

## Actual

No instrumentation prints at all, including one placed **in the init path**:

```
FG_SEQ_RM type=%d       (placed right after ctx_tgt_seq_rm_type = common_context_can_seq_rm(ctx_tgt))
FG_ACCEPT_BRANCH ...    (placed at the top of the accept dispatch)
FG_SP_SYNTH ...         (inside server_sample_and_accept_synth)
```

All three are silent, and the server serves requests normally (`"speculative.types":"none,ngram-simple"`,
about 80 tok/s with a repetitive prompt).

## What was verified

- the library is fresh and loaded from this build: `libllama-server-impl.so`, timestamp 04:01,
  resolved by `ldd` to `builds/flint-acc/bin`;
- the same library prints other diagnostics (`FG171_TOP_P goose calls=1`, `FG268P_CTX_CHECKPOINTS`,
  `GOOSE_ALLOC`) on the same run, so the build is not stale and stderr is captured;
- `FG_ACCEPT_DIAG=1` is exported in the server's environment;
- four configurations tried, including f16 KV and `--ctx-checkpoints 32`; none print;
- one earlier run did print `FG_SP_PREFIX goose calls=1`, from the same binary family, which is why the
  path was believed reachable.

## Hypotheses, unproven

1. The init function containing `ctx_tgt_seq_rm_type = common_context_can_seq_rm(ctx_tgt)` is not the
   one executed at startup, so my model of the construction order is wrong.
2. `spec` is created but the slot never reaches the verify block because `slot.spec_draft` stays empty,
   and the branch I instrumented is inside a condition that returns earlier.
3. The server binary found on `PATH` is not the one started (it is started by absolute path, so this is
   unlikely but not excluded).

## What I ask

A pointer to where in `server-context.cpp` the speculative draft is filled and verified for a
non-draft-model spec type, or to the log line that would show it. With that, the remaining P6-11 live
numbers become a matter of one run each.

## What is not blocked

Everything gated synthetically: thirteen decisions with their own tests, `n_max` verified live on four
configurations. This blocker affects only the live acceptance numbers.
