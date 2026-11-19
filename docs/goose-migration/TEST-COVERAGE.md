# Test coverage of every module in the shipped unit, and what is not covered

The unit is 24 modules. This is the map the last request asked for: module, what it decides, what proves it, and what
remains untested. It is generated from the repository by `tools/audit-door-evidence.py --modules`, so it cannot drift from
the sources without the command saying so.

```
python3 tools/audit-door-evidence.py --modules   -> ok, every module in the unit is traceable to evidence
```

## Modules with direct evidence

| module | decision | evidence | not covered |
|---|---|---|---|
| `sampling/sampler-chain-full` | top-p, typical, XTC, DRY, mirostat, penalties, temp_ext, adaptive_p | per-stage doors compared on selected tokens; the DRY order defect and the XTC sort defect found this way; AWT now drives them through an agent session | adaptive_p off-by-one ULP off the acceptance profile |
| `speculative/spec-doors` | synthetic and rejection acceptance, prefix agreement, draft width, compatibility | 880 rejection combinations, 180 compatibility, 630 ngram-accept, synthetic live on the model | nothing requires a draft model; the live rejection route does |
| `model/bpe-tokenize` | the encoder | 248 320 token texts equal; 23 prompts; a 32 000-token text prompt with the same answer hash | pre-tokenizer families other than qwen35 (guarded, never served) |
| `model/bpe-detokenize` | piece cache, buffer form, end-of-generation, sequence form | all 248 320 pieces equal, then served; short buffers equal; streaming edges and abandonment | none known |
| `model/gguf-reader` | the metadata reader | every key index and type, 34 values bit for bit, 13 strings, 744 231 array elements, refused on disagreement | float arrays (Apex has none), multi-file shards |
| `model/gguf-tensors` | tensor records | writer cases relinked against the shipped library, 41 cases / 307 checks | - |
| `model/arch-registry` | architecture names and recipes | an 89-recipe sweep with 0 disagreements at load | - |
| `model/slru-policy-goose` | the expert-cache replacement | 61 441 decisions equal on a real run | **not served**: needs the per-layer maps inside the door |
| `misc/pretokenizer-qwen35` | the segmenter | inside the encoder's equality | other families |
| `cli/fit-params-line` | the fit line | 20 cases equal to the native format, `-ts` and `-ot` included | no call site in this tree |
| `cli/llama-bench-report` | report formatting | 5 hand-checked cases (means, CSV escaping) | no call site; no end-to-end report comparison |
| `cli/config-validate` | config rules | 4000 cases, 0 uncovered | - |
| `server/hf-cache-naming` | HF cache naming | 6 hand-checked cases, `models--org--name` | no call site |
| `cuda/cuda-arch-policy` | compute capabilities | 5 hand-checked cases | wiring lives in CMake |
| `cuda/cuda-target-flags` | nvcc flags | `-lineinfo` in debug, absent otherwise | wiring lives in CMake |
| `cuda/cuda-install-layout` | install destination | 2/1/0 for dynamic/shared/neither | no call site |
| `misc/spec-decode-decisions` | ngram draft, accept, emit | 11 exports named; reached through the spec doors | live draft routes need a draft model |
| `metrics/metrics-traces` | CSV/trace field rules | 21 exports named; the `%.6g` provider exists because the module needed one | the sinks live in another tree |

## Modules exercised through their callers

`misc/unicode-text` and `misc/gguf-headers` have no card of their own. The audit shows why they are still accounted for: the
first is called by `pretokenizer-qwen35` and `bpe-detokenize`, the second by `gguf-reader`. Their correctness is the
correctness of those callers, which the table above proves on 248 320 pieces and 744 231 metadata elements.

`speculative/spec-decisions` and `model/tokenizer-decisions` export nothing: they are bodies called by the doors above.

## The gap this map makes visible

Every module has evidence, and that was **already true when the server was dying**. The crash came from
`common_token_to_piece` in the C++ host, not from a Goose module - and no per-module test could have found it, because the
defect was in how the host used the port's answer. That is why the agent-level test is now part of acceptance: it exercises
host and port together, which is the boundary the module tests do not cross.

## The rule this establishes

A module is not DONE because its exports are named in a card. It is DONE when a test drives the shipped call path that
reaches it and checks the result, and the untested column above is the honest list of what remains.

## Defects found in the shipped runtime and the test that now holds each of them

The rule is that a defect gets a permanent test, not a note. This is the ledger; every row names the command that fails if
the defect returns.

| defect | how it showed | regression test |
|---|---|---|
| piece buffer overlap in `common_token_to_piece`: a 21-byte piece written into the string's 15-byte cache, damaging the caller's token field | `tok=-779955792` (bytes `b0 d1 82 d1`) at generation step 643 of a real agent session | `experiments/EXP-2026-11-19-1441-piece-buffer-regression/verify.sh` - 248 077 pieces, 7 508 of them longer than 15 bytes, zero overwrites, and a control proving the old implementation damages the caller |
| DRY sampler order | door comparison | `experiments/EXP-2026-11-19-1381-sampler-full-sequence/` |
| XTC sort | door comparison | same |
| device sampled token returned unvalidated | two runs with a value outside the vocabulary | `vendor/flint/common/sampling.cpp` now falls back to the CPU chain; covered by the AWT (`tools/awt/run.sh REPRO`) |
| invalid token accepted into the batch | `get_sub_batch: failed to add token` | the server guard `FG_SERVER_INVALID_TOKEN` plus the AWT liveness check |

The first row is the one this section exists for: the per-module tests all passed while it happened, because the damage was
on the boundary between the port and its host.

## The encoder on the shipped path: default and equal

`experiments/EXP-2026-11-19-1442-encoder-served-default/verify.sh` runs the shipped `llama_tokenize` twice - once with
`FG_TK_BPE_SERVE=0` and once with nothing set - over the 23 prompts and requires the two answers to be identical, that
`FG_TK_BPE_SERVED` appears (so the port really answered) and that the comparison reports zero disagreements:

```
encoder: served answer equals native on 23 prompts
FG_TK_BPE_SERVED lines: 2
FG_TK_BPE_COMPARE calls=1 disagreements=0
FG_TK_BPE_REFUSED: none, the port answered every prompt
PASS
```

Two defects came out of writing it. Serving had been nested inside `FG_TK_BPE_DIAG`, so the accepted improvement was off
unless a diagnostic was enabled, and the check that the Goose path answered was only true under that flag. And the comparison
block sat after the `return` that serving uses, so a diagnostic run reported no comparison at all.

## The piece crash: the root cause is the served detokeniser's length contract

The first fix (a real buffer inside `common_token_to_piece`) stopped the crash on that call path. The root cause was one
layer deeper: the served detokeniser is given a 4096-byte window of its own, and its answer was copied into the caller's
buffer **without comparing it with the caller's length**. Any caller with a short buffer was overflowed by any piece longer
than it - and the caller that crashed passed 15 bytes for a 21-byte piece.

It now returns the native `-needed` when the piece does not fit, so no caller can be overflowed whatever length it passes.
`experiments/EXP-2026-11-19-1443-detok-length-contract/verify.sh` calls it with deliberately short buffers surrounded by
canaries, over every piece longer than 15 bytes:

```
detok length contract: long_pieces=7508 short_buffer_cases=25123 canary_hits=0 contract_violations=0
control: an unchecked copy of 35 bytes into a 8-byte buffer is caught
PASS
```

This is the check that should have existed before the port was served: the equality tests compared the bytes the port
produced, and none of them asked what happens when the caller's buffer is smaller than the answer.

## The buffer contract, written down

`docs/goose-migration/PORT-BUFFER-CONTRACT.md` lists every place the shipped runtime lets the port write into memory it does
not own, the capacity rule of each, and which one was wrong. The rule it states is the one the crash taught: a door given a
window of its own knows nothing about the caller's limit, so the host must compare the answer with that limit before copying.

## The sampler doors, on the CPU, against the native stages

`experiments/EXP-2026-11-19-1447-sampler-door-differential/verify.sh` runs a real chain (temperature, top-p, dist) over
fabricated token arrays - nine thousand cases from five distribution shapes, five sizes, three temperatures, three top-p
values and forty seeds - twice: once with the doors and once with `GGML_GOOSE_SAMPLER_TOP_P=0`.

```
compare: cases=9000 selection_mismatches=0 kept_size_mismatches=0 boundary_token_differences=40
FG171_TOP_P lines with the doors on: 41
FG171_TOP_P lines with the rollback:  0
PASS
```

What the test learned, and what it deliberately does not claim:

- the **selection** is identical on every case, which is the decision;
- the **kept set** differs by one element on forty near-uniform cases: two correct summations of the same probabilities can
  cross the 0.95 threshold on different sides. The comparison allows that boundary token and counts it, instead of hiding it
  behind an order-insensitive hash or failing every run for it;
- the rollback run proves the door was actually exercised: forty-one marker lines with it, zero without.

Two defects were in the verifier itself and were fixed: the exit code came from a `grep -c` that finds nothing on a passing
run, so a pass was reported as a failure - the second time this exact mistake appeared in this project's own checks.

## What a client actually sees: pieces joined equal the whole answer

The server streams pieces; the client's answer is their concatenation. `experiments/EXP-2026-11-19-1448-piece-sequence-consistency`
measures that relation on the shipped path:

```
piece/sequence: text_cases=10 text_diffs=0 sequence_cases=400 sequence_diffs=0
PASS
```

Ten texts - code, JSON, unicode with emoji, tabs, SQL, empty - survive encode->decode byte for byte, and four hundred random
token sequences give the same bytes when their pieces are joined as the sequence form gives whole.

The first run of this test reported seven differences out of four hundred, and they were **my** error: the piece form was
called with `special=false`, which returns nothing for a special token, while the sequence form was called with
`unparse_special=true`, which renders it. Two different questions, compared as one - the same mistake this project keeps
finding, and the reason the note stays here.

## The edges, where the equality tests never looked

`experiments/EXP-2026-11-19-1449-tokenizer-boundaries/verify.sh` asks what happens when something is wrong:

```
tokenize short buffer: got=-10 smashed=0
detokenize empty: got=0 smashed=0
token_to_piece out of range: -1 -> 0, n+1000 -> 0 (no abort)
parse_special: off=7 ids, on=4 ids
detokenize with a bad id inside: got=2 (process alive)
tokenizer boundaries: failures=0
```

The third line is the defect this test found: an id outside the vocabulary reached `token_get_attr`, which is
`id_to_token.at(id)`, which throws `std::out_of_range`, which nothing catches - the process terminates. Measured on `-1` and
on `n_vocab+1000`, both aborting. A server cannot afford that, because token ids arrive from requests, so the public piece
call now refuses with an empty answer and one line in the log (`FG_TOKEN_TO_PIECE_RANGE`), and a bad id inside a sequence no
longer ends the process either.

## A request the server must refuse without dying

`awt.py` now sends one request with a token id outside the vocabulary (`"prompt": [1, 2, 99999999]`) at the end of CHECK,
FULL and REPRO, and requires three things: a request error rather than 200, not a 5xx, and the server still answering `/health`
afterwards.

The check is written because the same failure shape reached the batch builder from the sampler
(`failed to add token -2100184624`), and the batch builder aborts on it. The client path turned out to be already safe:
`server_tokens::validate` refuses `t < 0 || t >= n_vocab` and the handler answers `ERROR_TYPE_INVALID_REQUEST`. That is now
held by a test rather than by reading the code once.

## Long context is measured, and a refusal must leave the server serving

Two claims were previously implied rather than checked.

The padding in CHECK and FULL is only a means; the test now reads the `prompt_tokens` the server reports and fails when it
is below the size the mode exists for (18 000 for CHECK, 25 000 for FULL). "We padded the conversation" was not the same
claim as "the request carried a long context".

A request for 400 000 tokens cannot be served, so the server must refuse it - not answer, not crash - and the next request
must succeed. That is the recovery property, and it holds whatever the reason a request fails. The refusal is recorded with
its status and the reason for the following step is measured, not assumed.

## The same request twice must give the same answer

Every mode that runs a session now ends with two identical greedy requests: temperature zero, the same seed, the same text.
With no sampling variance left, a difference can only come from the context, the cache or state the sampler carried between
requests - which is exactly the class of defect that a single-request test cannot see.

If it ever fails, it is a finding rather than a flake: the step records both lengths and the first eighty characters of each
answer.

## The architecture doors, without a model, without a GPUk

`experiments/EXP-2026-11-19-1456-arch-doors-cpu/verify.sh` opens the shipped library for its exported doors and asks the
questions the loader asks on every start - but in a second, and without 18 GiB of weights:

```
arch doors: registry rows=9
arch doors: rows_checked=9 round_trip_failures=0 duplicate_rows=0 unknown_refused=6/6 ver_checked=7 ver_failures=0
PASS
```

The round trip is the contract: the name a row gives must map back to that same row, or the loader reads the wrong layout for
a model it recognises. Unknown names are refused rather than given a row, and the version comparison is checked as a truth
table rather than by one example.

Two mistakes were in the harness itself, and both are the kind this project keeps finding. The first called a door as
`(pointer, length)` in separate registers instead of the two-field struct the generated C declares, and segfaulted. The second
tested a **guessed** list of architecture names - twenty-one of twenty-five were architectures this build deliberately
dropped, so the test failed on its own guesses. The names now come from the registry through `ar_count`/`ar_arch`, which is
the only list that means anything.

## The joint QG split, held against the native line

The registry recorded one gap by name: `fg_q35s_qg_split` was not compared by any gate. It is now.

`experiments/EXP-2026-11-19-1467-qg-split-gate` compiles `goose/model/qgate-split.goose` on its own against a capture host,
so the two view records the module produces are recorded instead of becoming tensors, and compares them with the native
expression transcribed from `vendor/flint/src/models/qwen35moe.cpp:293-300`:

```
qg split: shapes=7 refusals=4 failures=0
PASS
```

Seven shapes cover the head widths and element sizes an Apex-class model produces. A head stride of one head width instead of
two - the failure that would run and produce nonsense - is caught by the stride comparison, and the gate view's offset must be
exactly one head width inside each head. All four zero dimensions refuse with codes 60..63 rather than returning a plausible
handle.

What the gate does not prove is that the native builder still carries that expression; it is compared against the line as
transcribed, and the transcription is named so a future reader can check it.
