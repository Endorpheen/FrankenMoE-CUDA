# Which remaining work is gated on which decision

Written after seven decisions accumulated, to answer one question plainly: is the loop blocked on choices, or is there work left that a
decision cannot unblock?

## Gated on the base-library rebuild (decision 1)

Four items live in `libggml-base`, whose deployed bytes cannot be reproduced from source:

- **P6-02** itself -- the scheduler replacement and the reproducibility problem.
- **P2-13**, encoder/decoder *wiring*. The sixteen decoders and five artifact-gated encoders exist and are gated, but installing them in
  the type-trait table means relinking the base.
- **P1-06**, "zero calls to replaced native arithmetic". Same table, same relink.
- **P4-03**, the extra block-alignment check and the error-message parity. Same module, same relink.

## Gated on other decisions

- **P6-03** needs a reference graph trace (decision 5).
- **P6-06** needs a scope definition or a generator that emits a second valid model (decision 6).
- **P2-16** needs the definition of "owned" (decision 7).
- **make_qkx3 / IQ4_XS** (decisions 2 and 3) are numerical questions in the encoder direction; they do not need a relink to investigate,
  only a decision about whether `-0` versus `+0` and a sign/magnitude discrepancy are acceptable.

## Not gated, but large

Everything else open -- the P3 CUDA families, P1-07 pack/trait round-trips, the P2 operator matrix items -- lives in `libggml-cuda`,
`libggml-cpu` or plain C++ that can be rebuilt and relinked without touching the frozen base. Those are multi-tick ports, not decisions.

## The plain conclusion

The loop is not out of work: it is out of *closeable* work. Seven items -- the ones this session actually reached -- are each one decision
away from moving, and the remaining open items are ports that need multiple ticks and, for CUDA, a GPU that is available but shared.
Starting another port while four items sit one answer away would not be efficient, so the next tick begins one only if a decision is still
pending.
