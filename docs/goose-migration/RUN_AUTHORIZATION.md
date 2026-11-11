# Standing run authorization (this session)

Direct confirmation from Igor, delivered through the runtime/compiler channel as
`igor-run-without-reasking-8be2d37d0c11` (nonce `8be2d37d0c1142bc8c32b6874ca69317`), received 2026-11-11.

What it changes: builds, isolated test-server runs, functional checks and one short measurement after a
closed item are **pre-authorized**. The earlier local habit of asking for a separate "yes" before every run is
withdrawn for this task. Do not end a turn with a new question about the same permission.

Limits that remain: do not stop or reconfigure the production server or colleagues' processes; do not change
production or the compiler; do not integrate anything that has a correctness error.

Also required by the same message: judge the real functional remainder of CPU_MATH, not just the generic hook.
If the dispatched AVX2 path bypasses the new hook, that must be fixed rather than counted as ported because a
symbol exists.

## Second confirmation (loop continuation)

`igor-loop-continue-authorized-work-61bc2244242e`, nonce `61bc2244242e49f8b20ba24af156f9d7`: every further loop
continuation means *do the current item completely, its checks, and self-acceptance, then the next one*. It is
not a reason to re-ask permission and not a reason to drift into unrelated work. Re-read the saved
authorization instead of declaring it missing. Also save this into HANDOFF and the working instructions.
