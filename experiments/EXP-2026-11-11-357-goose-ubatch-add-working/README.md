# EXP357: ubatch_add on the working line (bridge gated, run invalid)

Bridge for the working candidate's `ubatch_add` decisions and its call site.

## Verdict

**BRIDGE GATED; RUN NOT VALID.** The bridge is exact against its transcription, but the stage run
cannot be evidence: see "Stage identity" below.

## Bridge

The working line's `ubatch_add` is the simple one: payload sizes, `out_ids` in row order, `seq_id_unq`
in increasing id, per-row `n_seq_id` and `output`. The bridge returns those from the module's journal;
the payload copies stay native.

| check | result |
|---|---|
| 5 000 draws | 81 787 checks / 0 failures |
| integrated object's symbol set | 123 = 123 |

## Stage identity — why the run does not count

Two faults were found while this was wired:

1. `libllama.so.0` in the run directory was still a hard link to the accepted EXP244 DSO. Every
   earlier "stage" run therefore loaded EXP244 under that name: `/proc/maps` showed the path I
   expected while the content was the old library, and the witness lines came from it.
2. With the substitution really in place the server refuses to start: `libllama-common.so.0` needs
   `llama_batch_ext_init`, which the objects of `build/expert-tier-franken-cuda` do not provide. That
   build comes from `work/llama.cpp-integration` (native line); the accepted runtime comes from the
   line that has `llama_batch_ext` (EXP238/240/243 integrations).

So the runs of EXP347, EXP354, EXP355 and this one are marked **not validated**, and no scoped item
moves on them. What stands: the gates, the bridges and the call-site compilation.

## Next step

Rebuild the stage from the line the accepted runtime uses, then re-check that the loaded file's hash
equals the stage hash before any witness is believed.
