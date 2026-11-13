# EXP269: checkpoint policy on the live server path

Integration half of EXP-268. Three call sites in `tools/server/server-context.cpp`'s `create_checkpoint()` are switched
to the Goose module behind `fg268p_enabled()` (default on; `GGML_GOOSE_CTX_CHECKPOINTS=0` restores the native loops).

## How it is wired

The checkpoints are a `std::list<common_prompt_checkpoint>`. For each decision the integration copies `n_tokens` and
`id_task` into flat arrays, calls the module, and erases by index from the mask it returns -- so the container, its
ownership and its logging stay exactly where they were. The near-eviction loop, the make-room loop and the supersede
loop are each preceded by the Goose pass and skipped when the switch is on.

## Build and ABI

`server-context.cpp` was copied, patched and compiled with the flint build's own command from `compile_commands.json`:
only `fg268p_enabled`, `fg268p_near_evict`, `fg268p_make_room` and `fg268p_supersede` are added to its undefined set. The
object replaced `server-context.cpp.o` inside a copy of `libserver-context.a`, and `libllama-server-impl.so` was relinked
from ninja's own command with the archive and the bridge substituted.

## Live A/B on the accepted profile

Same DSO, only the switch differs:

| arm | content sha256 | tokens | probabilities | witness |
|---|---|---|---|---|
| `GGML_GOOSE_CTX_CHECKPOINTS=1` | `e68df05b...` | equal | equal | `FG268P_CTX_CHECKPOINTS near=2 room=2 supersede=2` |
| `GGML_GOOSE_CTX_CHECKPOINTS=0` | `e68df05b...` | equal | equal | `near=0 room=0 supersede=0` |

The decisions execute during prompt processing (a 32K prompt reaches the checkpoint code), the answer is unchanged, and
the switch really disables them. This is a live route on the accepted profile, unlike the shift work.

## What is not claimed

Speed. Both arms were run once each; comparing decode or prefill times from a single pair would be noise, and this change
is a policy swap with the same number of container operations.
