# EXP267: shutdown during an in-flight request, then restart clean

P5-09's last lifecycle clause: no dangling cache after unload. The clean-shutdown case was already measured; this is
the harder one -- SIGINT to the server while a streaming completion is being served.

## Phase 1: interrupt mid-stream

A streaming completion was started on the accepted runtime and the server was sent SIGINT after 8 SSE events:

    events_before_interrupt = 8
    exit_code               = 0          (clean, not killed)
    log                     "cleaning up before exit..."
    port_free_after         = true
    process_gone            = true
    FG195V_CUDA_VMM_POOL    alloc=9526 free=9526 grow=5 native=0
    FG196P_CUDA_PINNED      host_plan=3 host_fail=0 fallback=0 reg_plan=120 unreg=240 native=0

So the pools balanced exactly despite the request being cut off mid-stream, with zero native fallback. After the
process exits there is no `llama-server`, port 8081 is free, and the GPU holds only the desktop applications.

## Phase 2: the same runtime starts again

    ready = true
    sha   = e68df05b4829865dcf8cd2424febfaded15b055f56fec36fefc002650ef8e7c9
    tokens = 128
    exit_code = 0

The accepted answer comes back unchanged after the interrupted session, so the interruption left nothing on disk or in
the cache that a later request can pick up.

## What this closes

The unload clause of P5-09 now has both cases: clean shutdown (balanced counters, EXP-266's log) and shutdown under
load (this run). Together with the cancellation measurement (EXP-379: drop a streaming client, next request normal at
`e68df05b`), the lifecycle clauses have counters rather than assertions.

## What it does not close

Shift and sequence-cleanup decisions are still gated modules without a model witness of their own (EXP-291/295/327-333),
and the near-capacity run is still missing; both are P5-09/P5-07 work, not lifecycle questions.
