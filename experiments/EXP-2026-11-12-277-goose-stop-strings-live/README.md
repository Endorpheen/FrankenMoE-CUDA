# EXP277: stop-string decision on the live path

Integration half of EXP-276. The full-stop branch of `server_slot::find_stopping_strings()` is switched to the Goose
module behind `fg276t_enabled()` (default on; `GGML_GOOSE_STOP_STRINGS=0` restores the native loop). Anti-prompts are
concatenated into one byte buffer with offset/length arrays, the module returns the earliest position and the word index,
and the caller sets `STOP_TYPE_WORD`, `stopping_word` and `has_next_token` exactly as before.

The partial-stop branch stays native: the profile reaches only the full-stop path, and the partial rule is a per-word
walk whose loop is the caller's. Recorded rather than silently implied.

## Build and ABI

`server-context.cpp` compiled with the flint command; only `fg276t_enabled` and `fg276t_find_full` are added to its
undefined set. The object replaced the archive member and `libllama-server-impl.so` was relinked with the archive and the
bridge.

## Live A/B with an anti-prompt

The accepted request does not stop on a sequence, so the A/B request is the accepted one plus `"stop": [" "]`; the
answer therefore differs from `e68df05b` by construction, and the gate is equality between arms on the same DSO:

| arm | A content | A tokens/probs | witness |
|---|---|---|---|
| `GGML_GOOSE_STOP_STRINGS=1` | `93c660bb...` | equal | `FG276T_STOP_STRINGS full=16 hits=2` |
| `GGML_GOOSE_STOP_STRINGS=0` | `93c660bb...` | equal | `full=0 hits=0` |

The full-stop search ran sixteen times and found a stop twice, with byte-identical content, token ids and all saved
probabilities between arms. That is a live route, not a linked one.

## Not claimed

Timing, and nothing about the partial-stop branch, which is still native.
