# P10-09 — test runners, fixtures and diagnostics for native-free builds

Base commit: `70de805c`. Work done in `/tmp/opencode/sub-652/`; the repository was
not modified (`git status` shows no tracked change). HEAD has since moved to
`9ac3eab0`; every native file used here is byte-identical at both commits
(`git diff 70de805c HEAD -- <path>` is empty for all of them).

## Inventory

The ledger entry (`docs/goose-migration/tasks.json`, P10-09) names four source
groups. What they actually contain, and what can be verified without a model:

| source | content | offline? | decision content |
|---|---|---|---|
| `runtime/tests/CMakeLists.txt` | 8 C++ test binaries and a CTest registry with per-architecture fixtures, a split-shard case and a python3-present skip | the registry is offline; the binaries are not (they link `bmoe_core`) | selection, fixture order, skip rules, exit codes |
| `upstream/llama.cpp/tools/server/tests/` | pytest tree, `tests.sh` (`SLOW_TESTS`, `-m "not slow"`), `pytest.ini` markers, `conftest.py` fixtures | the harness is; the tests start a server | marker/skip policy |
| `benchmarks/`, `scripts/bench-analyze.py`, `scripts/trace_io.py` | benchmark JSON/CSV, the analyzer that turns a run CSV into tables | yes — plain text in, numbers out | CSV parse rules, statistics, fallbacks, report rows |
| `upstream/BigMoeOnEdge/scripts/make-tiny-moe.py` | the fixture generator both gates depend on | yes with stub `gguf`/`numpy`; no with written weights | tensor plan, shapes, seeds, metadata, mode guards |
| `tools/check-goose-tree.py`, `tools/check-generated-files.py` | already ported in P10-05 S3 (`misc/generated-artifact-policy.goose`) | yes | — no slice here, no overlap |
| `tools/check-composition.sh` | composes module sets and runs `goose check` | needs no model | only the set list; no decision worth a module |
| `common/` diagnostics | `frankenmoe-logging.goose` and `metrics/metrics-traces.goose` already own logging and trace text | yes | — no slice here, no overlap |

Three slices were chosen because each owns a decision that a native-free build
cannot run without, and each has a real oracle that is not a transcription:

1. **tr_** — the runner policy, oracled by CTest 4.2.3 itself.
2. **fx_** — the fixture plan, oracled by the real generator running with
   recording stand-ins.
3. **dg_** — the benchmark diagnostics, oracled by the real `analyze()` executed
   from the native file.

## Results

| slice | module | oracle | cases | gate | mutations |
|---|---|---|---:|---|---|
| tr_ | `tr-testplan.goose` 667 lines, 11 exports | CTest 4.2.3 (`--output-junit` + progress lines) | 23 scenarios | **23/23 identical, 0 disagreements** | **12/12** |
| fx_ | `fx-fixture-plan.goose` 619 lines, 9 exports | the real `make-tiny-moe.py` with stub `gguf`/`numpy` | 2 architectures + split + guard | **195/195 rows identical** | **12/12** |
| dg_ | `dg-diagnostics.goose` 692 lines, 7 exports | the real `bench-analyze.py analyze()` + `trace_io.percentile` | 16 CSV shapes | **66/66 rows identical** | **12/12** |

Every gate runs at `-O1` and `-O0` and requires the two transcripts to be
identical. Every mutation run is split 6/6.

## What each slice decides

**tr_** — POSIX-ERE-subset filters; `DISABLED` is not a filter; a required
fixture pulls in its setup tests and appends its cleanup tests; a cleanup runs
after the last dependent and at its own definition position if that is later;
dependencies are emitted before dependents in a depth-first walk; a failed or
never-run setup blocks, a skipped setup does not; `SKIP_RETURN_CODE` is a skip,
not a failure; the denominator excludes disabled tests; the percentage rounds
half up; exit code 8 on any failure.

**fx_** — 51 qwen3moe tensors and 76 gemma4 tensors with exact numpy shapes and
per-layer seeds; the metadata order including the three list-valued tokenizer
calls; the tool's vocabulary rule (`<unk>`, `<s>`, `</s>`, then `<0xNN>`
upper-case); `small_first_shard` on every split; a split gemma4 refused with the
native message; the `wrote …` banner.

**dg_** — the header-driven column map with `wall_ms` at position 2; rows with
an unparsable wall dropped whole, unparsable optional fields dropped alone; the
last `# summary` trailer read by name; mean over all wall rows while rates use
only positive walls; median and interpolated p5/p95; MiB-per-token and TTFT;
`-1.0`/absent cache hit rendered as a dash; the cache-sizing table's membership
rule; row layouts with dash fallbacks.

## Integration map

| slice | target | ledger | notes |
|---|---|---|---|
| tr_ | `goose/misc/test-runner.goose` | P10-09, 1/3 | not in `check-composition.sh`: slice-parameter exports specialise only when a driver calls them |
| fx_ | `goose/misc/fixture-plan.goose` | P10-09, 2/3 | keep the pinned native copy as the oracle |
| dg_ | `goose/metrics/bench-diagnostics.goose` | P10-09, 3/3 | reads what `metrics-traces.goose` writes |

Each slice README carries the exact five steps (canonical copy, `CANONICAL_SOURCES`
row, `MANIFEST.json` row with SHA-256, `tasks.json` update, oracle copies).

## Compatibility with the single Goose runtime

* All three are ordinary modules: `import std`, plus `import math` in `dg_` for
  `floor`. No externs, no file I/O, no clocks, no allocation. Buffers are
  supplied by the caller.
* Name prefixes are disjoint (`tr_`, `fx_`, `dg_`), so the three compose into one
  unit; each gate driver already composes its module with a driver and runs it.
* Fixed bounds are explicit and refused, not truncated: 64 tests / 30 fixtures
  (tr_), 64 rows / 32 columns (dg_).
* Existing behaviour is untouched: no file in the repository is read at build
  time, nothing shadows an accepted export, and no accepted module is modified.

## Risks

* CTest semantics are the golden rows. A future CTest that changes
  `UpdateForFixtures` or the summary arithmetic would fail the gate loudly,
  which is the intended behaviour; the port would then be rebased against the
  new oracle, not silently kept.
* The regex subset is a boundary. A filter using groups would select nothing
  rather than the wrong tests; this is documented and mutation-tested for the
  implemented constructs.
* `dg_` decimal parsing is exact for the gated corpus but is not `strtod`; a
  value long enough to round differently would appear as a field mismatch.
* `goose check` on a module alone does not specialise slice-parameter exports.
  The unit that typechecks is the module plus its driver, as in every gate in
  this tree. Do not add these three to `check-composition.sh` without a driver.

## How to reproduce

```
cd slices/tr/tests && python3 gen_tr.py --rebuild && python3 mutate_tr.py 0 12
cd slices/fx/tests && python3 gen_fx.py --rebuild && python3 mutate_fx.py 0 12
cd slices/dg/tests && python3 make_cases.py && python3 native_rows.py \
    && python3 gen_dg.py && python3 mutate_dg.py 0 12
```

No model, no GPU, no CUDA, no build of the engine. `cmake`, `ctest`, `python3`
and the Goose toolchain (`goose-2026.3-string`) are the only tools used.

## SHA-256 of the deliverables

```
2f42df0fe0d69ea70913667d5f22b2695070e6c96b3f8f3c659f87d4d53a7465  slices/tr/source/tr-testplan.goose
3a6ed27760f9842768431c1aa984a01acff0cee700aa0e5d433040fa4786893d  slices/fx/source/fx-fixture-plan.goose
c79f6718fff6d671b80603716f6052bb3b3cfc6712dc04f77bb160da0f07d00f  slices/dg/source/dg-diagnostics.goose

18138cec09ce52c2d5981cc23454729dde053714ad32278d6c99bc762f48a334  slices/tr/tests/gen_tr.py
46319743ab1a1f2496dac398d2ac0bbaecf4090420d78dc6f20e4301dc2d79f3  slices/tr/tests/mutate_tr.py
b51686e681763078de5a785cd26a435eee75b2a432cc70a3c5ba527b456e5b68  slices/tr/build/native.json

754099ce7ae03a400dd0b4bf6efee6dbb741eeb866173513926eb45919ac53d3  slices/fx/tests/capture_fx.py
6a9aaae3b4878f371e7ac0abdd6cb6f28a452a32c968544e972b8eb08d26ac32  slices/fx/tests/gen_fx.py
b304b2115d17c2d765d9cbfda3a619ff6e57fab7c56654b88c90243ee6f0a118  slices/fx/tests/mutate_fx.py
67f420a93191b7e93d358d8f0b7bcab34102a74e837de14c38682b40635290b2  slices/fx/tests/native/make_tiny_moe.py
cb4deaca2df98351694b0c7d0376616777c4330c6080ea1c63007828b825d4f1  slices/fx/build/native_fx.tsv
8ba587c4099e816332d625365e103a11590de3978db6ab6c02023a41f0cba2d7  slices/fx/build/goose_fx.tsv

91eeadbcb47b283ce96119f65300f87b8bef435db07a239626ace5bd89d5903b  slices/dg/tests/make_cases.py
3c859f45e8d72e580e05336651cc63215169861009b8bf5c6c3250fbcab5016c  slices/dg/tests/native_rows.py
59430a8d64810b6ceb052cc05022bf46480cd9107f508f19b71e0a4213136fea  slices/dg/tests/gen_dg.py
df0b735cd260de4c314e293fe33b08a22b8c1f06e7a6918489c8730e31e413fe  slices/dg/tests/mutate_dg.py
d78414ae4c4eb84c3bde384b96f49cfbcf6d07504e84bed531946c3873c62e64  slices/dg/tests/native/bench_analyze.py
01b341758b96c31c73dc33582dbc55b832acee2af8f53237bd26d19a25d9f057  slices/dg/tests/native/trace_io.py
ca72c0f2b37318a18e86ac59020eedc5d5f29f87446296a6332c7ff5838b5e1f  slices/dg/build/native_dg.tsv
5cababd48168a970964f905b5265632145c0e77c9803e46617104175ecf7ff76  slices/dg/build/goose_dg.tsv
```

## What to commit when the lead accepts

* the three `source/*.goose` files and the three slice READMEs,
* the generators, the mutation tables and the pinned native oracle copies,
* the recorded transcripts (`build/native_*.tsv`, `build/goose_*.tsv`,
  `build/native.json`) as evidence,
* **not** the generated drivers (`build/gate_*.goose`) nor the per-case CMake
  projects — both are reproducible from the source and the tables.

One experiment = one atomic commit, so the three slices can land as three
commits or as one P10-09 commit; the transport is otherwise identical.
