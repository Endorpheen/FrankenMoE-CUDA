# GooseInfer upstream synchronization and maintenance plan

**Historical reference.** This is a preserved proposal for an earlier upstream synchronization workflow. See the [current build guide](BUILD.md).

Status: PLANNED. Nothing in this document is implemented. It describes the
pipeline that starts only after the runtime is fully migrated to Goose and the
repository is ready for publication. Until then the migration plan and the
current experiment queue have priority.

## 1. Purpose

GooseInfer tracks `llama.cpp` for upstream compatibility and potential ports. Upstream keeps adding kernels, model
architectures, quantizers, server features, fixes and performance work. The
fork must absorb the useful parts without ever returning hand-written C/C++ to
the working runtime. This plan defines how upstream changes are discovered,
triaged, ported to Goose, tested, accepted and recorded.

The three non-negotiables:

1. The working runtime is Goose. A port is not "done" because a symbol exists;
   it is done when the Goose module is on the executed path, gated, and can be
   rolled back.
2. No accepted change may accumulate a regression. The golden rule is the
   combined balance of prefill, decode, RAM and VRAM on the fixed Apex profile,
   not one number in isolation.
3. Every decision is traceable to an upstream SHA and a GooseInfer experiment
   or task id. No change is accepted from memory or from a chat message.

## 2. What is tracked

### 2.1 Upstream repositories and refs

- `ggml-org/llama.cpp`: `master`, release tags, and the `ggml` submodule history.
- Relevant long-lived branches and PRs that touch the paths below.
- Upstream CI results and issue labels (`bug`, `perf`, `CUDA`, `server`,
  `quantization`, `model`, `security`).

### 2.2 Paths and subsystems

| Upstream area | GooseInfer area | Owner ledger |
|---|---|---|
| `ggml/src/ggml-cuda/**` | CUDA compute and host | P3-* |
| `ggml/src/ggml-cpu/**`, `ggml/src/ggml-cpu/arch/**` | CPU math and control | P1-*, P2-* |
| `ggml/src/ggml-alloc.c`, backend/scheduler | allocator, buffers, scheduler | P4-* |
| `src/llama-*.cpp`, `src/llama-*.h` | model, graph, KV, memory, sampling | P5-*, P6-* |
| `common/**` | GGUF, tokenizer, chat, CLI, logging | P7-*, P9-* |
| `tools/server/**` | HTTP, API, queues, slots | P8-* |
| `vendor/**`, build files | dependencies and packaging | P10-* |

Every upstream change is mapped to one of these areas and to the current task
ledger id. A change that maps to no area is either out of scope or reveals a
missing ledger row; both outcomes are recorded.

### 2.3 What is not tracked

- Upstream changes for architectures, backends and tools that GooseInfer has
  explicitly retired. The retirement list is a versioned document; a retired
  item is not silently resurrected by a sync.
- Style-only churn, comment edits and documentation that does not change a
  contract.
- Experimental branches that upstream itself has not merged.

## 3. Ingestion pipeline

### 3.1 Fetch

A scheduled job (daily, and on release tags) fetches:

- commit metadata: SHA, parents, author, date, message, changed files, diff;
- PR metadata: number, title, body, labels, review state, linked issues;
- CI status for the commit and for the PR head;
- release notes and tag annotations.

The raw objects are stored append-only with their SHA and fetch time. The
pipeline never rewrites history; a corrected analysis is a new record.

### 3.2 Normalize

Each change becomes one record:

```json
{
  "upstream_sha": "…",
  "kind": "commit|pr|release",
  "area": "cuda|cpu|alloc|model|common|server|vendor|build",
  "files": ["…"],
  "summary": "…",
  "risk": "low|medium|high",
  "status": "new|triage|port|not_required|deferred|accepted|rejected",
  "franken_task": "P3-07",
  "franken_experiment": "EXP-…",
  "evidence": ["…"],
  "owner": "…",
  "updated_at": "…"
}
```

The registry is append-only for status transitions: every transition keeps the
previous value, the author, the time and the reason. A status is never edited
in place without a transition record.

### 3.3 Classify

An automated first pass proposes:

- **not_required** -- the change touches a retired area, a test-only path, a
  style-only edit, or a backend GooseInfer does not ship;
- **port** -- the change fixes correctness, safety, a numerical contract, or a
  measured performance loss on a supported path;
- **deferred** -- the change is useful but depends on a subsystem that is not
  yet ported, or on hardware that is not available;
- **accepted** -- already present in the Goose implementation (the diff is
  checked against the ported module, not assumed).

The automated proposal is a hint, not an acceptance. A human reviewer confirms
every `port` and every `accepted`. The classifier records why it proposed the
status, including the matching file, symbol or keyword, so a wrong proposal is
auditable.

### 3.4 Risk score

Risk is a small, explainable score, not a magic number:

- touches allocation, lifetime, synchronization or error paths: +2;
- changes numerical order, precision or rounding: +2;
- touches CUDA kernels or launch geometry: +1;
- changes a public ABI, API or file format: +2;
- has an upstream test or a small reproducer: -1;
- is a one-line fix with a clear contract: -1.

High-risk changes require a differential gate and a rollback before any model
run. Low-risk changes still require the normal gate; risk only changes the
amount of evidence, never whether evidence exists.

## 4. Triage workflow

1. The daily digest groups new records by area and risk.
2. The reviewer marks each record `port`, `not_required`, `deferred` or
   `accepted`, with a one-line reason.
3. `port` records become an experiment card before any code is written. The
   card states the hypothesis, the upstream SHA, the affected Goose module, the
   correctness gate and the acceptance criteria.
4. `deferred` records name the unblocking condition (subsystem, hardware,
   compiler feature or owner decision).
5. A weekly review closes stale `triage` records; nothing stays unclassified
   for more than one release cycle.

The reviewer never ports directly from a diff without a card. A card may cover
several small commits if they share one mechanism and one gate; it may not
cover unrelated subsystems.

## 5. Porting workflow

1. Create an isolated candidate directory and keep the working runtime
   untouched.
2. Locate the upstream change in the Goose source. If the mechanism already
   exists, the status becomes `accepted` with the evidence; do not re-port it.
3. If it does not exist, write the Goose implementation. Native code may be
   used only as an oracle in a fixture, never as a fallback in the shipped
   path.
4. Build with the pinned compiler and record the compiler SHA and the generated
   source hashes.
5. Run the correctness gate before any performance claim:
   - differential against a verbatim native oracle, bit-exact where the
     contract is arithmetic;
   - mutation gate, with mutations that target every decision the port owns;
   - sanitizers appropriate to the code: memcheck/racecheck/initcheck/
     synccheck for CUDA, ASan/UBSan for host code, TSan for shared state.
6. Run the model gate on the fixed Apex profile when the change is on the
   executed path. Compare tokens, probabilities and content with the accepted
   checkpoint.
7. Run the performance gate according to section 8.
8. Accept, reject or defer. A rejection keeps the card and the numbers; it does
   not delete the experiment.
9. Commit one experiment per change, with the upstream SHA in the message and
   the ledger transition in the same commit.

## 6. Test system

The test system is layered so a cheap gate stops a bad change before an
expensive one runs.

| layer | what it proves | when |
|---|---|---|
| unit / fixture | one function or kernel matches its contract | every port |
| differential | Goose output equals a native or independently derived oracle | every port |
| mutation | the gate actually detects a broken decision | every port |
| sanitizer | memory, race, init and sync safety | every port; full matrix on high risk |
| API/ABI | symbols, signatures and wire formats remain compatible | every public-surface change |
| concurrency | slots, streams, cancellation, shutdown | server/scheduler changes |
| model corpus | real Apex request, tokens/probabilities/content | on-path changes |
| quality corpus | task/tool-use correctness where probabilities may legitimately move | precision/quantization changes |
| performance | golden-rule balance on the fixed profile | accepted candidates |

### 6.1 Differential oracles

- Arithmetic kernels: a verbatim copy of the upstream kernel compiled into the
  fixture, compared bit for bit where the contract allows it.
- Host policy: a transcription of the upstream decision, with generated cases
  that reach every branch, including refusal and overflow paths.
- Format/API: golden bytes produced by the upstream implementation, stored with
  its SHA.

A tolerance is fixed before the candidate run and recorded in the card. A
failed tolerance is never relaxed after the fact to turn a rejection into an
acceptance.

### 6.2 Mutation policy

Every port names the decisions it owns and supplies at least one mutation per
decision. A mutation that cannot be observed is replaced, not counted as a
pass. A mutation that hangs the driver is not a detection; the object is
restored and the mutation is redesigned.

### 6.3 Fuzzing

- GGUF, tokenizer, JSON, grammar and chat-template parsers: structure-aware
  fuzzing with a corpus derived from real models and malformed inputs.
- KV/state save and restore: truncated, oversized and cross-version files.
- Server: malformed HTTP, oversized bodies, disconnects, cancellation and
  concurrent slots.
- CUDA: out-of-range shapes, tails, zero sizes and misaligned strides under
  compute-sanitizer.

## 7. CI design

### 7.1 Stages

1. **fast** (per push): format/build check, unit fixtures, static analysis.
2. **correctness** (per push/PR): differential, mutation smoke, host
   sanitizers.
3. **CUDA** (per push on a GPU runner): kernel fixtures, memcheck/racecheck.
4. **model** (nightly or on demand): fixed Apex profile, tokens/probabilities/
   content, save/restore/rewind/cancel.
5. **performance** (nightly on a quiet host): paired A/B/B/A, golden-rule
   table, regression budget.
6. **release** (on tag): full matrix, reproducible build, license/attribution
   audit, artifact hashes.

A failed stage blocks the merge. A flaky test is quarantined with an owner and
a deadline; it is not silently retried until green.

### 7.2 Runners and reproducibility

- CPU runner for host fixtures and policy tests.
- GPU runner with the same compute capability as the reference host; a
  different GPU is recorded as a different environment, not mixed into the
  same baseline.
- Pinned compiler, pinned CUDA toolkit, pinned model hashes and a container or
  sysroot recipe.
- Every artifact records the compiler SHA, source SHA, build flags and input
  hashes. A build that cannot be reproduced is a release blocker.

### 7.3 Performance jobs

Performance jobs run alone on a quiet host, never beside a model server or a
compiler build. The job records host load and GPU occupancy; if the host is
busy, the run is marked preliminary and cannot accept a change.

## 8. Golden-rule performance acceptance

The fixed Apex profile is the reference:

- 32K prompt / 128 decode tokens unless the card names a different shape;
- context, batch/ubatch, threads, KV precision, expert placement and sampling
  recorded with every run;
- prefill tok/s, decode tok/s, peak RSS, peak VRAM and swap;
- one paired A/B/B/A series, medians and spread, not a single response.

Acceptance requires:

1. no correctness regression;
2. no stable regression beyond the agreed budget on any of prefill, decode,
   RAM or VRAM;
3. a plausible mechanism for any claimed improvement;
4. the same result in both orders of the pair, or an explicit statement that
   the effect is inside the noise band.

A single noisy sample may open an investigation; it may not accept or reject a
change. The precise budget is set with the owner before the first optimization
cycle and recorded in the ledger; it is not invented per experiment.

## 9. Checkpoints and regression control

- The best accepted full runtime is a checkpoint with an immutable manifest:
  binary hashes, library hashes, profile, compiler, evidence links.
- A new candidate is compared against the current checkpoint, never against a
  half-remembered number.
- An accepted candidate becomes the next checkpoint; the previous one stays
  available for rollback.
- Rejected experiments stay in the journal with their numbers so the same idea
  is not retried blindly.
- A regression found after acceptance triggers a bisect over the checkpoint
  history and a rollback if the cause is not fixed promptly.

## 10. Architecture and dependency policy

- Retired architectures, backends and tools are listed explicitly. A sync does
  not resurrect them.
- A new architecture is tracked only when a real model and a real use case
  exist. "Upstream supports it" is not a reason to port it.
- Vendored dependencies are pinned and audited. A dependency update follows the
  same card/gate/acceptance cycle as a code change.
- Licenses and attribution are preserved for every imported file and for every
  generated artifact.

## 11. Security and supply chain

- Signed commits and protected branches for the public repository.
- No secrets in logs, fixtures or experiment artifacts; the logger redacts
  tokens and API keys before output.
- Fuzzing and sanitizer results are release evidence, not optional extras.
- A security fix from upstream is `port` by default, even if it is not on the
  current performance path.
- The public repository never contains private model files, credentials or
  local experiment dumps.

## 12. Registry and dashboards

The registry is the single source of truth. It exposes:

- counts by status, area and risk;
- time in `triage`, time to port, acceptance and rejection rates;
- open `deferred` records and their unblocking conditions;
- the current checkpoint and the last accepted experiment per area;
- links to experiment cards, raw evidence and benchmark tables.

A read-only dashboard and a Telegram digest may consume the registry. They do
not become a second source of truth; a status changed only in a chat message is
not a status.

## 13. Roles and cadence

| role | responsibility |
|---|---|
| upstream watcher | fetch, normalize, propose classification, keep the registry current |
| reviewer | confirm classification, write cards, enforce gates |
| Goose implementer | port, gate, benchmark, record evidence |
| release owner | checkpoint, reproducible build, public artifacts, rollback |

Cadence:

- daily: fetch and digest;
- weekly: triage review and stale-record cleanup;
- per release: full matrix, checkpoint promotion, public notes;
- per incident: rollback/bisect and a post-mortem record.

## 14. Metrics

- percentage of new upstream records triaged within one day;
- median time from `port` decision to accepted experiment;
- share of accepted ports with a live witness on the reference profile;
- regression rate after acceptance;
- deferred records older than one release;
- test coverage of owned decisions (ports with a mutation gate / ports);
- reproducibility rate of release artifacts.

Metrics are diagnostic. They do not replace the acceptance gates and they do
not turn a count into a quality claim.

## 15. Rollout after 100 percent Goose migration

1. Freeze the migration ledger and publish the final dependency report.
2. Publish the retired-architecture list and the supported-profile matrix.
3. Stand up the registry, fetcher and dashboard in read-only mode.
4. Run one dry-run sync cycle without porting anything; compare the proposed
   classifications with a manual review.
5. Enable the daily digest and the weekly triage.
6. Port one low-risk upstream fix end to end as the first real cycle.
7. Port one high-risk fix with the full sanitizer/model/performance matrix.
8. Review the cycle, adjust thresholds and only then make the pipeline
   routine.

## 16. Non-goals

- Returning C++/CUDA to the working runtime "just for this one fix".
- Porting every upstream architecture or backend.
- Chasing upstream performance numbers on a different profile.
- Accepting a change from a single benchmark or a single response.
- Using the registry as a substitute for reproducible evidence.
- Publishing private models, credentials or local experiment data.
