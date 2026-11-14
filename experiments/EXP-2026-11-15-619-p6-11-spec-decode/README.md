# Speculative decoding decisions (P6-11)

The pure policy of the self-speculative decode loop: the n-gram draft proposal, the accept
decision, the MTP draft-state arithmetic, and the per-step budget, rollback and end-of-turn
trim. Base commit `df3ce73a`; the anchors are unchanged through `77ef4c72` (verified with
`git diff df3ce73a..HEAD -- runtime/core/src/engine/session.cpp ...`, empty).

Native counterparts:

| file | lines used |
|---|---|
| `runtime/core/src/engine/ngram_draft.cpp` | 10-98, the whole matcher |
| `runtime/core/src/engine/session.cpp` | 1676-1912, the decode loop |
| `runtime/core/include/bmoe/config.h` | 260-325, `DraftSource` and `SpecConfig` |
| `upstream/llama.cpp/common/speculative.cpp` | 1445-1470, 1700-1800, the MTP head |

The work tree is `/tmp/opencode/sub-626`; the repository itself was not touched, no model
was loaded, no GPU was used and no production path was changed.

## What this slice covers

`source/sdp.goose` (prefix `sdp_`, 466 lines, 11 exports, no imports) is pure integer policy
over token ids, positions and confidences. Three slices:

### A - the draft source and the accept decision

| export | native anchor | what it returns |
|---|---|---|
| `sdp_ngram_draft` | `ngram_draft` `ngram_draft.cpp:10-98` | the prompt-lookup continuation, 0 when no suffix of `min_match` tokens matches |
| `sdp_accept` | `session.cpp:1766-1795` | the accepted prefix, the confirmed group, and the end-of-generation stop |
| `sdp_emit_count` | `session.cpp:1848-1852` | how many confirmed tokens the caller's budget lets through |

Owned decisions: the longest-common-suffix scan with its `max_match` cap and early exit, the
recency tie-break (`>` only, so an equal-length earlier match never wins), the intentional
overlap between match and pattern that makes a run of one token predict itself, the `avail`
bound that stops a draft at the corpus end, the `min_match` floor that makes an abstaining
step cost exactly a plain decode, the argmax-agreement prefix, and the rule that an accepted
end-of-generation token is in the KV and in the count but never in `confirmed`.

### B - the MTP draft state

| export | native anchor | what it returns |
|---|---|---|
| `sdp_mtp_width` | `speculative.cpp:1700-1785` | the adaptive draft width under `p_min`, the `n_min` floor, and the decode-step count |
| `sdp_mtp_head_clamp` | `speculative.cpp:1445-1470` | the chained-head `n_max` clamp (one head on the measured target) |
| `sdp_mtp_rewind` | `session.cpp:1700-1706` | the draft-KV rows the rewind removes, and the hole a missed prefill leaves |
| `sdp_mtp_catchup` | `session.cpp:1810-1830`, `speculative.cpp:1792-1800` | the catch-up width (`1 + n_acc`, accepted prefix only) and the accept seed row |

Owned decisions: a candidate below `p_min` stops the loop *before* the push (the step decoded
and produced nothing), a push that reaches `n_max` stops it *after*, a result below `n_min` is
cleared, the rejected tail is never submitted to the MTP block, and the next draft is seeded
from row `min(n_acc, n_rows - 1)`.

### C - the budget, the rollback and the trim

| export | native anchor | what it returns |
|---|---|---|
| `sdp_step_plan` | `session.cpp:1676-1740` | room, `n_max`, whether the source is called, `wide`, the capped draft count and the verify width |
| `sdp_rollback` | `session.cpp:1818-1824` | whether the rejected tail must be carved, and the keep position `n_past + 1 + n_acc` |
| `sdp_step_advance` | `session.cpp:1884-1890` | the next `n_past` and the logits row (`n_acc` on a wide step, `-1` otherwise) |
| `sdp_tail_trim` | `session.cpp:1899-1912` | the trim point `n_prompt + n_gen` and the rows ahead of it |

Owned decisions: `room = n_predict - n_gen - 1` goes in *before* drafting so no source is
asked for tokens with nowhere to go; a step that drafts nothing takes the plain path (one
token, one logits row, no rollback); the rollback is emitted only when the draft was not fully
accepted; and the KV is trimmed back to what the caller actually received.

## Gate

```
bash tests/build.sh
```

`tests/fixture.cpp` carries a transcription of the native functions with the source line cited
at each, and links the Goose module. Both sides evaluate the same synthetic corpus in one
process, so every comparison is exact equality:

```
FG626_SPC ngram    cases=112 failures=0
FG626_SPC accept   cases=316 failures=0
FG626_SPC mtp      cases=356 failures=0
FG626_SPC budget   cases=5028 failures=0
FG626_SPC loop     cases=4016 failures=0
FG626_SPC cases=9828 failures=0
FG626_SPC loop runs=33 emitted=567 drafted=368 accepted=353 steps=150 decodes=223 trims=9 eog_runs=9 floor_runs=3
```

The corpus:

| section | what it holds |
|---|---|
| ngram | 19 fixed corpora (empty, shorter than the floor, no repetition, exact `min_match`, a run of one token, a copied segment, an equal-length tie, the `max_match` cap, a 12-token block repeated) each through 1-3 parameter sets, plus 36 deterministic random corpora over alphabets of 2, 3 and 5 tokens |
| accept | 11 fixed patterns (all accepted, a mismatch first, middle and last, none, empty, an accepted EOG at each position, an EOG past a mismatch) plus 40 random patterns with agreement near two thirds, and the emit bound grid |
| mtp | 13 width cases (`p_min` 0, 0.5 at the boundary, 1.0, the `n_min` floor, an exhausted confidence array), 24 random confidence arrays, 8 head-clamp rows, 7 rewind rows including an empty draft context, and 5x7 catch-up rows |
| budget | the full `n_predict` x `n_gen` x `draft_max` x `spec_on` x `n_draft` grid (600 steps x 6 outputs), the rollback grid at three `n_past` values, the advance grid, and the trim grid across three contexts |
| loop | a whole speculative run over a scripted model `T(pos)`: 18 runs on a repeating pattern (three prompts x three `draft_max` x two `min_match`), 9 runs where the source proposes the end-of-generation token, 3 runs cut by the caller's budget, and 3 runs on a non-repeating stream |

The loop section compares the composed port against the native loop step by step: the draft
width, the accepted count, the verify width, the keep position, the logits row, the emitted
tokens, `n_past`, the KV end, and the trim. It also checks that the speculative answer is
byte-identical to plain greedy decoding (no approximation enters the loop) and that a run
which drafts nothing takes exactly the plain decode count - the floor the source rests on.

Two consecutive runs of the final build print identical output. `tests/gate.log` is that
output.

## Mutations

```
python3 tests/mutate.py 0 6     # half 1: the draft source and the accept decision, 6/6
python3 tests/mutate.py 6 12    # half 2: the MTP draft state and the budget, 6/6
```

Twelve semantic mutations, two halves of six, module rebuilt at `-O0`; a build failure counts
as caught and the source is restored after every mutation:

| # | mutation | first case that caught it |
|---|---|---|
| 0 | an equal-length earlier match beats the most recent one | `ngram_tie_recent` |
| 1 | a run no longer predicts its own last token (`avail - 1`) | `ngram_run` |
| 2 | the gate needs strictly more than `min_match` | `ngram_tie_recent` |
| 3 | an accepted EOG is emitted like any other token | `acc_eog_first` |
| 4 | a mismatching draft is confirmed | `acc_first_mismatch` |
| 5 | the emit bound ignores the caller's budget | `emit_grid` |
| 6 | a token exactly at `p_min` is rejected (`<=`) | `mtp_pmin_boundary` |
| 7 | one draft past `n_max` is collected | `mtp_pmin0_nmax3` |
| 8 | the accept seed reads one row past the prefix | `mtp_catchup_seed` |
| 9 | the rewind keeps the last drafted row | `mtp_rewind_removed` |
| 10 | the room forgets the confirmed token | `plan_room` |
| 11 | the rollback keep forgets the confirmed token | `rb_keep` |

## Interfaces the port relies on

Canon side (all present at `df3ce73a` and unchanged at `77ef4c72`):

| interface | use |
|---|---|
| `bmoe::ngram_draft(ctx, last, n_max, min_match, max_match, out)` in `core/include/bmoe/ngram_draft.h` | the signature the module replaces; tokens are `int32_t`, which is `llama_token` |
| `bmoe::SpecConfig` (`source`, `draft_max`, `draft_p_min`, `ngram_min_match`, `ngram_max_match`, `draft_max_limit`, `ngram_match_limit`) | every input the module takes as a parameter; no new flag |
| `bmoe::DraftSource` (`none`, `mtp`, `ngram`) | decides whether a step speculates at all |
| `Session::Impl` loop locals: `n_past` (`llama_pos`), `n_gen`, `req.n_predict`, `draft_buf` (`std::vector<llama_token>`), `mtp_ctx`, `verify_toks`, `confirmed`, `im.mtp_*`, `im.drafted_steps` | the state the exports read and return |

Native llama.cpp side, kept where it is and only read by the port's caller:

| interface | use |
|---|---|
| `llama_get_logits_ith(ctx, i)` + `argmax` | supplies `target[i]` for `sdp_accept` |
| `llama_vocab_is_eog(vocab, tok)` | supplies the `eog` mask |
| `llama_memory_seq_rm(llama_get_memory(ctx), seq, p0, -1)` | executes the rewind, the rollback and the trim the port decides |
| `common_speculative_draft` / `_process` / `_accept` | the MTP block itself; `sdp_mtp_catchup` sizes the batch it gets and names the seed row |
| `common_params_speculative_draft{n_max, n_min, p_min}` | the width inputs, `n_min` being 0 as the session passes it |
| `llama_model_n_layer_nextn`, `llama_get_ctx_other` | the head count and the shared-memory bit behind `sdp_mtp_head_clamp` |
| `llama_decode`, the sampling chain | untouched: the module decides nothing the backend executes on its own |

## Integration step

1. Copy `source/sdp.goose` to `goose/misc/spec-decode-decisions.goose` (the group that holds
   other `llama_context`-level decisions, e.g. `goose-sched-decisions.goose`). The file has no
   `import`, so it composes with any set and collides with none.
2. `python3 tools/sync-goose-tree.py` (rebuilt `MANIFEST.json` with the new file) and
   `python3 tools/check-goose-tree.py`.
3. `python3 tools/check-goose-symbols.py` - it must report the 11 new `sdp_` exports against
   the recorded set; accept them with `--update` in the same change and re-run to verify.
4. Add the module to `tools/check-composition.sh` (the `core` set, or a new `engine` set with
   the future engine modules) and run it: the composer now checks that the composed unit
   exposes exactly the union of the input exports.
5. `runtime/core/src/engine/ngram_draft.cpp`: keep the header and the signature, replace the
   body with `sdp_ngram_draft({ctx.data(), (int64_t) ctx.size()}, last, n_max, min_match,
   max_match, {out.data(), (int64_t) out.capacity_or_n_max})` - `out` is the caller's vector,
   so reserve `n_max` first and resize to the returned count. `runtime/tests/ngram_test.cpp`
   keeps passing unchanged, which is the cheapest proof of behavioural equality.
6. `runtime/core/src/engine/session.cpp`, in this order inside one decode step:
   1. `sdp_step_plan(req.n_predict, n_gen, im.cfg.spec.draft_max, spec_on, 0, plan)` - `plan[1]`
      is the `n_max` handed to the source; after the source returns, call it again with
      `n_draft` for the defensive cap (`plan[4]`) and the verify width (`plan[5]`);
   2. build `target` and `eog` from `llama_get_logits_ith` and `llama_vocab_is_eog`, then
      `sdp_accept(...)` replaces the loop at 1778-1790;
   3. `sdp_mtp_rewind(seq_pos_max, n_past, out)` before the draft context is caught up, and
      `sdp_mtp_catchup(n_acc, n_rows, out)` for the batch width and the seed row;
   4. `sdp_rollback(n_past, n_draft, n_acc, out)` - when `out[0]` is 1, call
      `llama_memory_seq_rm(ctx, 0, out[1], -1)`;
   5. `sdp_step_advance(n_past, n_acc, wide, out)` for `n_past` and the logits row;
   6. after the loop, `sdp_tail_trim(n_prompt, n_gen, kv_end, out)` for the two `seq_rm` calls.
7. No `SpecConfig` field and no env/CLI flag is added: the module is on whenever
   `spec.source != none`, and `GGML_*`-style switches stay absent (rule 10). `validate()`
   keeps owning `draft_max_limit` and `ngram_match_limit`; the module assumes them.

## Compatibility checks

| check | result |
|---|---|
| `goose check source/sdp.goose` | `typechecked ok: 13 specialization(s), 0 struct/0 enum instance(s)` |
| `bash tests/build.sh` | `FG626_SPC cases=9828 failures=0`, two runs identical |
| mutations, halves | `6/6` and `6/6` |
| duplicate exports | `grep -rn "sdp_" goose/ tools/ runtime/` finds nothing at the base commit: all 11 names are new |
| helper names | only `sdp_at`, module-local; `tools/compose-goose.py` needs no `RENAMES` entry |
| generated files | `build/sdp-gen.c` (182 KB), `build/*.o`, the gate binary are in `build/` and are not tracked |
| repository | not modified: `git status` shows no file of this slice |

## Assumptions and conflicts

- `llama_token` is `int32_t` and `llama_pos` is `int32_t` in this tree, which is why the module
  speaks `i32` and `i64`; a token type change would need the slice re-signed.
- Every `out` slice must be sized by the caller: `n_max` entries for `sdp_ngram_draft`, and 2
  to 6 `i64` entries for the rest. A short `out` truncates rather than growing, which the
  native `std::vector` would not; the gate sizes every buffer correctly.
- `sdp_mtp_width` treats a confidence array shorter than the loop needs as "the head would
  decode another step" (`ran_out`). Native has no such bound because the head always produces
  the next probability; the flag is a caller diagnostic, not a decision.
- `sdp_accept`'s `stop` index and `sdp_mtp_rewind`'s `missing` count have no native field: the
  native loop breaks without recording where, and `begin()` only warns about a draft context
  behind the target (`speculative.cpp:1508-1520`). They are diagnostics with zero behavioural
  weight, and the gate compares them against the transcription's own equivalents.
- `n_min` is ported but never fires in the shipped configuration (`sp.draft.n_min = 0`); the
  floor is kept because `common_params_speculative_draft` owns it and a future caller may set
  it.
- The chained-head clamp is ported for the multi-head models (`gemma4`, `step35`) and is inert
  on the measured `qwen35`/`qwen35moe`, which has one head. `is_mem_shared` is an argument, not
  a probe, so the caller passes `llama_get_ctx_other(ctx_dft) == ctx_tgt`.
- A source that honours its cap can never outrun the caller's budget: `room = remaining - 1`
  and `n_acc <= n_draft <= room`, so `1 + n_acc <= remaining` and the group always fits. The
  trim's "cut short by `n_predict`" arm is therefore reachable only through the accepted
  end-of-generation token; the gate exercises that arm (`trims=9`) and the `tt` grid.
- No name conflicts: every export is `sdp_`, the helper is `sdp_at`, and no file in `goose/`
  uses either. If a later slice claims `sdp_`, this one is renamed at composition time, the way
  `RENAMES` already handles `K_STRING` and `fill` collisions.

## What needs the model, the tokenizer or the GPU

- The verify pass itself: the batched decode that produces the target logits is not
  bit-identical to single-token decoding, so a near-tie can flip an argmax and the loop is
  not byte-identical to a non-speculative run. `docs/mtp.md` records this; the gate checks the
  decision, not the arithmetic.
- The MTP head's probabilities (`p_min` input), the draft context's KV, `n_layer_nextn` and the
  shared-memory bit: `sdp_mtp_width` takes the confidences, it does not produce them.
- Token ids: the gate uses synthetic ids in `[0, 5)` plus an EOG sentinel; the tokenizer stays
  native (`D6`), so no vocabulary behaviour is covered.
- Throughput, acceptance on real text and the flash bytes speculation costs: measurements, not
  decisions. This slice claims no performance number.

## Files

| path | what |
|---|---|
| `source/sdp.goose` | the port, 466 lines, 11 exports, prefix `sdp_` |
| `tests/fixture.cpp` | the native transcription and the corpus, 935 lines |
| `tests/build.sh` | builds the module at `-O1` (or `-O0` with `mutate`) and the fixture at `-O1`, runs the gate |
| `tests/mutate.py` | the twelve mutations, `mutate.py [lo hi]` |
| `tests/gate.log` | the recorded gate output |
| `REPORT.md` | result, integration, hashes |

`build/` is generated and not part of the experiment.

## Environment

| | |
|---|---|
| Goose | `goose-2026.3-string`, runtime `1e714e6146bc` |
| compiler | `gcc-13` / `g++-13` (Ubuntu 13.4.0-10ubuntu1) |
| port build | `-O1` final, `-O0` under mutation |
| fixture build | `-O1 -std=c++17`, `-lm` |
| network, model, tokenizer, GPU | none |
