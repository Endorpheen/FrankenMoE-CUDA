# P4-03: public symbol parity of the built line against flint

The criterion requires the public tensor/context surface to be Goose-owned while the ABI is preserved. The cheapest honest check
first: does every public symbol the flint libraries export still exist in ours?

| library | flint | ours | difference |
|---|---|---|---|
| `libggml-base` (`ggml_*`) | 550 | 551 | one additive: `ggml_backend_sched_set_expert_pipeline` |
| `libllama` (`llama_*`) | 255 | 255 | none |
| `libggml-cpu` (`ggml_backend_cpu*`) | 6 | 6 | none |

So no public symbol is missing anywhere, and the single difference is an addition, not a removal. That is the symbol-level half of
"ABI preserved"; signature-level compatibility is not proven by this table and is not claimed.

## Integrity finding made while measuring

The CPU comparison first reported 0 against 6. The cause was not a missing API but a **dangling symlink**: the promotion step that
replaced `libggml-cpu.so.0.27.0.p502-test` with `libggml-cpu.so.0.27.0` left the stage directory pointing at the removed name. `nm`
silently failed and the script reported zero.

The stage was repaired (all three names now point at the promoted file), audited for further dangling links (none), and verified by
loading the server: `ldd` reports no missing libraries and the completion endpoint answers correctly.

That matters for the record: any measurement taken through a stage whose libraries were dangling would have been resolved by the
loader's RPATH rather than by the stage, so those runs would silently use a different path than the one intended. Nothing here proves
a past run was wrong, but the failure mode is now documented and the stage is healthy again.
