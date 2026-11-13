# EXP358: Stage built from the accepted runtime's line (method fix)

The stage that the earlier probes should have used: objects from `llama.cpp-flint-cuda/build-cuda`,
the line that has `llama_batch_ext` and from which the accepted runtime is built.

## Verdict

**LINE CONFIRMED; NO GOOSE CALL SITE YET.** This experiment exists to replace an invalid method, not
to claim a route.

## Why this was needed

EXP347/354/355/357 ran their stage against the accepted EXP244 DSO: `libllama.so.0` stayed a hard
link, so `/proc/maps` showed the expected path while the loaded content was the old library. With the
substitution really in place the server refused to start because `libllama-common.so.0` wants
`llama_batch_ext_init`, absent from the `work/llama.cpp-integration` build.

## What is checked now

| check | result |
|---|---|
| objects from the flint line | 44, linked to 5 275 232 bytes |
| ABI: every `llama_` symbol `libllama-common.so.0` needs | present, none missing |
| all three library names replaced in the run directory | yes |
| run: status | RESPONSE_RECEIVED, 128 tokens |
| run: tokens / probabilities / content hash | equal to the accepted control |
| `FG` witnesses from the accepted runtime | **0** — proof this is a different library |
| prefill / decode | 1586.5 / 24.1 tok/s |

## Reading the numbers

The accepted-control floors (2500 prefill, 25 decode) belong to the accepted runtime, which is built
with the Goose modules. This stage is the plain line without them, so it is slower by construction;
the point of the run is exactness plus the absence of the old witnesses, both of which hold.

## Next step

Add the call sites to this line — starting with `ubatch_add`, whose module (EXP349) matches this
line's `mixed` block — and only then read a witness.
