# EXP-1700 — Attribute CPU boundaries in default Apex decode

This diagnostic keeps all original CUDA and Goose archive members.
GNU linker wrappers time the boundaries of generation and the
nested GGML graph plan calls. Measure one warmed Apex 32K/512
request on the working no-probability profile at 4,096/4,096.
The request returns the identical 512 token IDs and text.

| Timing over 511 steps | Seconds |
|---|---:|
| Between model evaluator return and next decode call | 1.158 |
| Inside all decode evaluator calls | 12.044 |
| Graph execution inside evaluator | 11.205 |
| Graph plan allocation inside evaluator | 0.247 |
| Graph expansion inside evaluator | 0.085 |

The first two rows reconcile against the server's 13.202 seconds.
The nested rows belong inside the evaluator. The intervals are
host wall time and do not sum with CUDA kernel times. The 1.158 s
gap includes sampling and token preparation. It does not account
for the entire 1.809-second outside-CUDA interval in the earlier
trace, which also sees prologue/epilogue work inside the evaluator.

The warm request independently measured 1.079 s between calls.
This is a diagnostic run, not a performance checkpoint. Measured
prefill and decode are 3,014.43 and 38.71 tok/s. Only the wrapper
object is new. Reuse the frozen fast archive with the ordinary CLI
and pass one linker `--wrap=symbol` for each `__wrap_` definition
in the included source. The compile inputs and both full
responses are under `build/exp1700-decode-boundaries`.
