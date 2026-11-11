# EXP362: both batch call sites live in one stage

`split_equal` selection (EXP361) and `ubatch_add` (EXP359) running together in one candidate.

## Verdict

**LIVE, EXACT** — both routes execute on every request and the answer is byte-identical.

## Stage

- 44 flint objects with the combined `llama-batch.cpp.o` and two bridges linked: 5 292 512 bytes.
- ABI complete: every `llama_` symbol `libllama-common.so.0` needs is exported.
- All three library names substituted; loaded-file hash equals the stage hash.

## Run (32K/128, one run, no parallel work)

| check | result |
|---|---|
| status | RESPONSE_RECEIVED, 128 tokens |
| tokens / probabilities / content hash | equal to the control (`e68df05b…`) |
| `FG350S_SPLIT_SELECT` | live — first call, 100, 200 |
| `FG349S_UBATCH_ADD` | live — first call, 100 |
| prefill / decode | 1589.0 / 25.69 tok/s |

The two blocks do not interfere: the selection fills the sets and cursors, `ubatch_add` consumes the
resulting index list, and the union of the two journals is still the same ubatch.

## What this establishes

Both `llama-batch.cpp` blocks that Apex actually enters are Goose-owned in the same candidate. The
remaining batch items are the containers' ownership and the parts of the splitter this profile never
reaches (keep-tail, split_simple), which cannot produce a witness here.
