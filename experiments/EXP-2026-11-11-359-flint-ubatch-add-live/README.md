# EXP359: ubatch_add on the accepted line, live and exact

The first stage run that both uses the accepted runtime's line and shows a Goose module executing.

## Verdict

**LIVE, EXACT** — `ubatch_add`'s decisions are taken by Goose on the Apex path, and the answer is
byte-identical to the accepted control. Not yet an acceptance of the splitter as a whole.

## What runs

- **Stage**: 44 objects of `llama.cpp-flint-cuda/build-cuda`, with `llama-batch.cpp.o` replaced by the
  integrated copy and the EXP356 bridge linked in. 5 283 864 bytes.
- **ABI**: every `llama_` symbol `libllama-common.so.0` needs is exported.
- **Substitution**: all three library names replaced, so `/proc/maps` and the file hash agree.

## Run (32K/128, one run, no parallel work)

| check | result |
|---|---|
| status | RESPONSE_RECEIVED, 128 tokens |
| tokens / probabilities / content hash | equal to the accepted control (`e68df05b…`) |
| `FG349S_UBATCH_ADD` | **live** — first call, then `calls=100` within the request |
| prefill / decode | 1547.4 / 24.09 tok/s |

The throughput is below the accepted floors because this stage carries only one Goose module; the
floors describe the full accepted runtime, not an intermediate stage. What matters here is that the
route executes and that the output is unchanged.

## What this closes and what it does not

Closes the question "does the port ever run on Apex" for this block: yes, and without changing the
answer. It does not close the batch sub-system, and it says nothing about the KV state paths — those
still need a request that actually saves or restores state.

## Files

- `build_stage.py` — the stage link, with the ABI assertion.
- `run_apex.py` — the run protocol (borrowed unchanged from EXP358).
