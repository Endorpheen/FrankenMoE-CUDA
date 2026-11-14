# EXP-2026-11-15-626 — speculative decoding decisions (P6-11)

Base commit: `df3ce73a`. Native counterparts: `runtime/core/src/engine/session.cpp` (decode
loop, 1676-1912), `runtime/core/src/engine/ngram_draft.cpp` (10-98),
`upstream/llama.cpp/common/speculative.cpp` (1445-1470, 1700-1800). Work tree:
`/tmp/opencode/sub-626`; the repository was not touched.

## Conclusion

Speculative decoding is three separable decision surfaces, and all three are pure policy over
token ids, positions and confidences: none of them needs a model, a tokenizer, a KV or a
thread. The port covers them in one module, 11 exports and 466 lines, and gates exactly
against a transcription of the native functions on a synthetic corpus.

- gate: `FG626_SPC cases=9828 failures=0` (two runs, identical output)
- mutations: 12 of 12 caught, 6 in each half
- slices: 3 (draft source and accept, MTP draft state, budget and rollback)
- port: `source/sdp.goose`, prefix `sdp_`, no imports, no native dependency

## Gate

| section | comparisons | what it holds |
|---|---:|---|
| ngram | 112 | 19 fixed corpora x 1-3 parameter sets, 36 random corpora over alphabets 2, 3, 5 |
| accept | 316 | 11 fixed patterns, 40 random patterns, the emit bound |
| mtp | 356 | 13 width cases, 24 random confidence arrays, 8 clamps, 7 rewinds, 35 catch-up rows |
| budget | 5028 | the step grid (600 steps x 6 outputs), 45 rollback rows, 30 advances, 189 trims |
| loop | 4016 | 33 whole runs, every step decision plus the final state |

Loop evidence from the same run:

```
FG626_SPC loop runs=33 emitted=567 drafted=368 accepted=353 steps=150 decodes=223 trims=9 eog_runs=9 floor_runs=3
```

- 368 drafts proposed, 353 accepted (95.9%) - the source is exercised, the corpus is not
  rejecting everything;
- 9 runs where the source proposes the end-of-generation token, so the accepted-EOG trim is
  exercised and not just the plain stop;
- 3 runs on a non-repeating stream where the source abstains: the decode count equals the
  plain path, which is the floor the feature rests on;
- every run also compares the speculative answer with plain greedy decoding: identical, which
  is what "no approximation enters the loop" means operationally.

Command: `bash tests/build.sh`.

## Mutations

| half | command | caught |
|---|---|---|
| 1 | `python3 tests/mutate.py 0 6` | 6/6 |
| 2 | `python3 tests/mutate.py 6 12` | 6/6 |

Each mutation changes one semantic decision: the recency tie-break, the overlap that lets a
run predict itself, the `min_match` gate, the accepted-EOG emission, the mismatch break, the
emit bound, the `p_min` boundary, the `n_max` off-by-one, the accept seed row, the rewind row
count, the room, and the rollback keep position. Each was caught by a named case; the source
is restored after every mutation and the baseline is re-run at the end.

## Integration

The module goes to `goose/misc/spec-decode-decisions.goose` (the group that already holds
`llama_context`-level decisions such as `goose-sched-decisions.goose`), then
`tools/sync-goose-tree.py`, `tools/check-goose-tree.py`, `tools/check-goose-symbols.py
--update` and `tools/check-composition.sh`. In the runtime,
`runtime/core/src/engine/ngram_draft.cpp` keeps its header and calls `sdp_ngram_draft`, and
`runtime/core/src/engine/session.cpp` replaces the inline arithmetic with
`sdp_step_plan` -> `sdp_accept` -> `sdp_mtp_rewind` / `sdp_mtp_catchup` -> `sdp_rollback` ->
`sdp_step_advance` -> `sdp_tail_trim` in that order. The full step, with the arguments and the
preconditions, is in `README.md`.

No `SpecConfig` field and no flag is added: the module is on whenever `spec.source != none`,
and the existing knobs (`draft_max`, `draft_p_min`, `ngram_min_match`, `ngram_max_match`) are
its only inputs.

## Boundaries

Not covered, and deliberately: the batched verify pass (not bit-identical to single-token
decoding, so a near tie can flip an argmax - `docs/mtp.md`), the MTP head that produces the
confidences, the draft context's KV and `n_layer_nextn`, the tokenizer, and every performance
number. The gate checks decisions; nothing here claims throughput, bytes or acceptance on real
text.

## Hashes

SHA-256 of the tracked artefacts:

| file | lines | SHA-256 |
|---|---:|---|
| `source/sdp.goose` | 466 | `e9f93b67252381b78e3df63934cda589efe55c57aa913ea496a4b6bc70f033e6` |
| `tests/fixture.cpp` | 935 | `306fbba37bc7b191c48cd3fbacff94097194746732f20b30f693007a4569cb84` |
| `tests/build.sh` | 34 | `ca17f00c098b9b83e65c488110e6833a8b3df774680420b3a422a4a25eb47751` |
| `tests/mutate.py` | 168 | `2857219a65b9b881d12882c08138a636be21af50a679049c1004cd306febb41e` |
| `tests/gate.log` | 7 | `ba5a7a4142f2fed4407110e90ac1b658fc97087c9bc1e0e34554cc22bf11535c` |
| `README.md` | 262 | `a62ed9ecaf6d3c7dc9859ab8009e28ba1bb8fc19c1c47e364e2c806e32fcdeea` |

`build/` (the generated C, the objects and the gate binary) is not part of the experiment; the
generated translation unit is 182,065 bytes.

## Environment

| | |
|---|---|
| Goose | `goose-2026.3-string`, runtime `1e714e6146bc` |
| compiler | `gcc-13` / `g++-13` (Ubuntu 13.4.0-10ubuntu1) |
| port build | `-O1` final, `-O0` under mutation |
| fixture build | `-O1 -std=c++17`, `-lm` |
| network, model, tokenizer, GPU | none |
| repository state | untouched; the canon had moved from `df3ce73a` to `77ef4c72`, and the cited native anchors are unchanged between them |
