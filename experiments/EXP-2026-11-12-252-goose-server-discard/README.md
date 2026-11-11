# EXP252: context-shift policy in Goose (APEX.REQUEST/SESSION)

When the context is full the server keeps `n_keep` tokens and discards a window:

    n_discard = params.n_discard ? params.n_discard : n_left / 2
    n_discard = clamp(n_discard, 0, max(0, n_left - 1))

A wrong port here silently corrupts prompt history, so it gets the same treatment as a kernel.

## Gate

    FG252S_GATE cases=11720 failures=0

`n_keep` in {0,1,64,4096}, `n_left` 0..4096 step 7, `params.n_discard` in {0,1,7,1024,100000}, against a
`std::clamp` transcription.

## Wiring and its honest limit

The decision is wired into `server_context.cpp` in place of the native expression, and the server built from the
flint tree completes the accepted request with `e68df05b`, equal probabilities and restore ok.

A defect probe (forcing `d = 0`) did **not** change the completion: the accepted request generates 32768 tokens
against a 262144-token context, so the shift branch is never entered. The port is therefore gated and wired but not
exercised by this workload - and the same is true of the native code it replaces.
