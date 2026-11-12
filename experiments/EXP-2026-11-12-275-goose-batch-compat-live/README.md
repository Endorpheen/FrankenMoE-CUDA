# EXP275: batch-compatibility predicate on the live path

Integration half of EXP-274. The body of `server_slot::can_batch_with()` is switched to the Goose module behind
`fg274b_enabled()` (default on; `GGML_GOOSE_BATCH_COMPAT=0` restores the native rule).

The two LoRA lists are flattened into pointer and f32-scale arrays on each call, so the module compares exactly what the
native code compares: same length, same adapter pointer, same scale as floats (paths are not compared, as in native).

## Build and ABI

`server-context.cpp` compiled with the flint command; only `fg274b_enabled` and `fg274b_can_batch` are added to its
undefined set. The object replaced the archive member and `libllama-server-impl.so` was relinked with the archive and the
bridge.

## Live A/B with concurrency

Same harness as EXP-271/273 (A: accepted 32K request; B: the same request two seconds later, which defers; C: streaming
client that disconnects and posts a cancel):

| arm | A content | A tokens/probs | B content | B tokens/probs | witness |
|---|---|---|---|---|---|
| `GGML_GOOSE_BATCH_COMPAT=1` | `e68df05b...` | equal | `e68df05b...` | equal | `FG274B_BATCH_COMPAT calls=257 yes=257` |
| `GGML_GOOSE_BATCH_COMPAT=0` | `e68df05b...` | equal | `e68df05b...` | equal | `calls=0 yes=0` |

Every comparison during batch assembly went through the module and returned true -- the two requests are identical, so
they batch together, exactly as in the native arm. Both clients received byte-identical answers with all saved
probabilities equal.

## Not claimed

No timing verdict: three clients on one slot, one pass per arm.
