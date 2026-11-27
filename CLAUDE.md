# AGENTS.md — Rules for AI agents working in GooseInfer

Single source of truth for every AI agent (Claude or other) working in this
repository. Read this file before building, measuring, or committing anything.
If a rule here disagrees with a habit carried over from a previous session,
this file wins.

## Hard rules (from Igor)

Non-negotiable. They are also carried verbatim into every handoff report.

1. Before starting the server/model, tell Igor the goal, the duration, and the
   number of runs. Start only after an explicit "yes".
2. Before every commit, show the results in plain words: outcome, memory
   impact, correctness, and the list of files. Commit only after approval.
   A commit that touches `goose/` runs `python3 tools/check-goose-tree.py` first: it compares
   every manifest entry with its file and its integration commit in 0.3 seconds, and on
   2026-11-19 a push without it failed CI on a hash mismatch for three modules that were edited
   correctly and recorded incorrectly.
3. Comments, commit messages, and all repository documentation are in English.
   Documentation means every tracked text artifact: experiment cards
   (`experiments/*.md`), `experiments/EXPERIMENTS.md`, `ROADMAP.md`,
   `docs/*.md`, patch READMEs, and any new notes or summaries. A Russian
   version of any document is produced only when Igor explicitly asks for it.
4. One experiment = one atomic commit.
5. Never touch user changes in `work/llama.cpp-integration`.
6. No destructive Git.
7. Each C++ deletion batch removes at least 6,500 net physical C++ and CUDA lines from `main`. Run `tools/cpp-line-count.py` before and after the batch. Report the measured total before, net lines removed, and total after. Use the same scope on both sides and the baseline in `docs/goose-migration/CPP-LINES.md`. This is the complete C++ migration progress report and supersedes the older handoff format for this work.
8. EHS stays off by default.
9. Never present single-response throughput as aggregate throughput.
10. Accepted improvements must be enabled by default in new builds. A new
    feature's env/CLI flag exists only to disable the feature (like
    `GGML_EXPERT_PINNED_RING=0`), never to opt in.
11. When this file changes, update `CLAUDE.md` synchronously in the same
    commit: `cp AGENTS.md CLAUDE.md`.
12. Every check must support selective re-run. If one of N checks fails, fix the
    cause and re-run only that check and the checks that genuinely depend on the
    change. Keep the passing results and reuse them while their inputs are
    unchanged; do not re-run a whole suite for reassurance. This applies to every
    test, gate and build, present and future. Correctness is not reduced by this
    rule: the full set still runs at a final acceptance, on a control point, or
    when the inputs of the skipped checks actually changed.
13. Preparing the staged tree is expensive: minimise how often it happens.
    Accumulate the largest safe independent batch of removals (20-50+ units),
    then prepare the tree once, run one incremental build and the required
    checks. Do not recreate the tree after every edit or patch error. Validate a
    patch on its own before the tree is prepared
    (`bash tools/stage-m1-tree.sh --check-patches` dry-runs every carrier against
    `upstream/llama.cpp` and touches nothing), fix it there, and reuse an
    existing tree and its build results whenever that is correct. A full rebuild
    happens only when it is genuinely required.
14. Every build, test and check writes its full log from the start: the exact
    command, the output, the exit code and the result, to a file (use `tee` or
    an equivalent so the output is both shown and kept). Reuse an existing log
    and result whenever the inputs are unchanged. Re-running a build or a test
    solely to produce evidence, a README or a report is forbidden: the log the
    run already wrote is the evidence. On a failure, repeat only the affected
    checks.
15. No C++ is deleted before the new Goose runtime is fully accepted. The order
    is fixed and is not reordered: complete integration (GGUF, tensors, graph,
    CPU/GPU, KV cache, server, prefill/decode), then proven behaviour with the
    checks that are actually needed (clean build, link, ABI, functional and
    numerical tests, comparison against the existing Apex, regressions, required
    execution paths), then Igor's final manual acceptance, and only then one
    largest possible pass that removes the replaced C++ runtime, the carrier
    patches and the now-unneeded dependencies. Until the new runtime has proven
    full working order, removing the old C++ or any of its working fallbacks is
    forbidden. Do not delete on assumption: every removal needs the evidence
    that the replacement already carries the behaviour. Do not over-verify
    either: reuse the evidence already produced and repeat a successful check
    only when its inputs changed. After the removal, verify the clean build and
    that nothing still references the deleted code; if that fails, fix the cause
    rather than weakening the check. A real model or GPU run is never part of
    this preparation without Igor's permission: prepare everything and hand the
    run over for his acceptance.
16. A full build of the composed runtime runs only when it is genuinely needed:
    after a large integration batch (tens of units, not a handful of lines) or at
    a mandatory acceptance point. A few dozen changed lines are not a batch: keep
    grouping them and verify locally. The local gate, in increasing cost, is
    `goose check <module>`, `tools/compose-goose.py` over the module list, and
    `goose check` on the composed file; those catch a broken module, a conflict
    and a type error without a C compiler. A full build is justified only for a
    risk they cannot see: generated-C compilation, the link, the symbol table, the
    ABI, or the binary's behaviour. Name that risk before starting. Do not run
    `bash tools/build-main-runtime.sh --repro` after every edit. Reuse a
    successful result while the sources and dependencies are unchanged, and never
    rebuild for a report, for confirmation or for reassurance.
    Integrate modules in large batches. Collect the errors a batch produces and
    fix them together; between builds use only the cheap local gate. A build per
    module is the failure mode this rule exists to prevent.
17. Apex is not the only architecture. Before any C++ is deleted, prove that the
    support for the other architectures and the ability to port them to Goose are
    intact: the shared loaders, graphs, backends and APIs must stay
    multi-architecture and must not acquire Apex-specific shortcuts. A working
    Apex obtained at the cost of the rest of GooseInfer is a failed migration.
    Keep an explicit dependency map of what is shared and what is
    per-architecture, and keep every piece an unported architecture still needs.
    The removal order is unchanged and stricter: proven Apex readiness, then a
    compatibility check of the other architectures, then Igor's manual
    acceptance, then removal. The scope of "other architectures" is the current
    ones: the modern 2026 models, the 2025 architectures selectively, and older
    legacy only for a stated reason. Porting all 151 substrate architectures is
    not the goal and must not become an endless migration of dead code. What must
    be preserved is the universal runtime and the ability to plug in the
    architectures that matter.

The migration target is full functional coverage, not a module count: for every
own C++ file, the shipped binary's call path must reach the Goose export, the
Goose export must answer, and the native body must be dead code. A file is
"owned" when the Goose module holds its decisions; it is "replaced" only when
the wiring is proven and the native body is removed. Legacy, test, duplicate and
alternative modules are never counted toward coverage.

`bash tools/migration-status.sh` is the single accounting command. It reports the
composed unit's module count, the own C++ classified by role (substrate that must
stay native, superseded by an accepted Goose module, still needed), and the
standalone runtime's capability list - and never blends them. A call into Goose
from the old C++ is never counted as progress: the migration's goal is a
standalone runtime that answers the capability without that C++ at all, so adding
Goose calls to the old runtime is forbidden as a way to improve a number.
Substrate lines (OS and memory primitives) are never counted as debt.
    Every new C/C++ adapter written to connect Goose to a substrate is DEBT, not
    progress: it is added to the ledger as a removal target and is never counted
    as completed migration. This project does not build a new C++ application with
    Goose inside it; it replaces the C++ runtime with Goose. A host adapter is
    acceptable only while the substrate has no Goose-callable surface, and it must
    shrink as that surface appears, never grow into a parallel implementation.
    Reuse an existing binding instead of writing a second one for the same symbol:
    a duplicate definition is debt with no purpose, and it fails the link the
    moment two adapters meet. A new adapter is created only when no existing one
    covers the symbol, and it carries no runtime logic - translation only. Every
    temporary adapter is removed in one pass after the Goose runtime passes full
    acceptance; none of them survives that pass. `docs/goose-migration/BUILD-SET.md`
records the build set by name and every exclusion with its reason. Both are
derived from the repository; neither is typed by hand, and neither requires a
build.

18. Waiting has no fixed duration, and a heavy build has a threshold.

    Never wait for work with a fixed sleep. A command that takes minutes is waited
    on through its own completion - `wait`, `shell_output` with `wait: "exit"`, or a
    status poll that returns immediately - never through a fixed several-minute
    pause. A fixed pause either overshoots (idle time) or undershoots (a second check
    of the same thing), and both waste the same resource. Polling is a status read,
    not a delay.

    Never start a build that is already running, and never re-run one to confirm a
    result it already produced. If a build is in flight, wait for that process.

    A heavy build (arch unit, composed runtime, full build) is justified by a large
    batch of necessary changes - tens of units - or by a risk the cheap local gate
    genuinely cannot see: generated-C compilation, the link, the symbol table, the
    ABI, or runtime behaviour. Adding one or two exports is not a batch: collect the
    next ones and build once. The local gate (`goose check <module>`,
    `tools/compose-goose.py`, `goose check` on the composed file) catches everything
    else, and its cost is seconds.

    Every heavy run is started as a tracked background process with its output going
    to a named log, and its pid recorded:

        PIDFILE=builds/.<name>.pid bash tools/<script>.sh [args] \
            > builds/.<name>.log 2>&1 &
        echo $! > builds/.<name>.pid

    While it runs, progress is reported with `bash tools/progress.sh
    builds/.<name>.log builds/.<name>.pid`: the stage, the elapsed time, the last log
    lines and the verdict. Never leave a long run as an opaque silence and never ask
    the human to watch an empty screen. Identify the process by its recorded pid -
    `pgrep -f` on the command line matches the pgrep itself and reports a finished
    build as running.

19. Optimisation is balanced across three axes: prefill, decode, and memory
    (VRAM and RAM). Never chase one number at the cost of the others without
    stating the trade.

    Compare RELATIVE changes, not absolute figures alone. Example: decode rising
    from 35 to 40 tok/s is +14.3%, and prefill falling from 2200 to 2100 tok/s is
    -4.5%; that exchange can be worth taking because the decode gain outweighs the
    prefill loss. Report all four numbers - prefill %, decode %, VRAM %, RAM % -
    together, every time.

    Rules that follow from this:

    - A compromise is allowed when the practical total improves; it must be
      stated as a compromise, with the numbers.
    - Choose libraries, dependencies, execution routes and build settings by
      measurement across several configurations, not by maximising one test.
    - No OOM, no correctness change, no unjustified memory growth. A configuration
      that breaks any of those is rejected regardless of its speed.
    - Record each best configuration with its numbers and a stated way to roll
      back.
    - On equal effectiveness prefer the option with fewer C/C++ dependencies and a
      larger Goose share.
    - Temporary library combinations are acceptable while searching for the
      optimum, but must not harden a C++ dependency into the final runtime.

    The architectural goal is unchanged: a complete, high-performance Goose
    runtime. The balancing rule governs how speed is pursued, not what the end
    state must be.

20. Every A/B test reports prefill, decode, VRAM, and RAM.

    A configuration is never accepted or rejected on one number. Each comparison
    is one table: prefill tok/s and %, decode tok/s and %, VRAM MiB and %, RAM
    MiB and %, measured under identical conditions, ideally as paired repeats,
    comparing medians and spread. A small decode gain may come with a prefill gain
    or a memory saving, and a small decode loss may be offset the same way - the
    table is what settles it, not the decode column alone.

    Any positive result from a comparable, warmed A/B with preserved correctness
    earns a place in the experimental cumulative checkpoint. There is no
    minimum throughput gain or time saving. A small gain, including 1–5 tok/s,
    remains useful. When two comparable pairs both improve the target metric,
    keep the change even when its gain is small or the absolute historical best
    remains higher.

    Record experimental acceptance, a fresh paired control, and the historical
    best as distinct results. Preserve each valid absolute best, including
    1480 → 1485 → 1490 tok/s on the same profile. Combine accepted changes and
    verify their joint effect when they share buffers, execution paths, or
    scheduling. Roll back a change only when a regression, correctness failure,
    or incompatibility is established. Preserve the previous best executable,
    exact runtime profile, and a way to switch back.

    Screen each optimization with a short warmed A/B that exercises its real
    path. Compare elapsed time in seconds on matched inputs. Use Apex 4096/2
    when it covers the path, and a targeted kernel fixture when a larger shape
    is required. A positive signal qualifies one matched full Apex 32,768/512
    pair. Save inconclusive or slower screens and move to another hypothesis.
    Warm up each binary before the full measurement. Keep measurements at
    each generation length separate in benchmarks/experiment-history.jsonl.
    Absent measurements stay null. When diagnostic profiling is planned for an
    experiment, include its results in that experiment's push.

21. Use one fixed endpoint for GooseInfer runtime server runs and benchmarks:
    `127.0.0.1:9101`. Do not change the port between experiments or silently
    choose another port when it is occupied.

    Before launching, identify the listener and verify whether it is a previous
    server owned by this worker. Stop only that worker's own previous server,
    verify that it has exited and released the port, then start the next run.
    Never stop an unrelated service to obtain the port. Report a conflicting
    owner instead of bypassing the conflict with another port.

    Keep at most one runtime benchmark server alive. Record its actual PID,
    terminate it after the run, and verify termination and port release.

22. Preserve runtime binaries before replacing them. Before any rebuild or
    operation can overwrite an existing executable or shared library, save an
    independent byte copy outside the candidate build directory and verify its
    SHA-256. A symlink or a hardlink is not a backup. Record the original path,
    source revision, build settings, dependency paths and hashes, and the
    workload/results associated with that copy; mark unknown provenance as
    unknown rather than calling it an accepted reference.

    Accepted executable/library sets are immutable. Build candidates in separate
    directories and switch to them only after the required checks. Never use an
    accepted library's filename as the compiler or linker's output. If the backup
    cannot be verified, do not overwrite the existing file. Preserve previous
    working sets and their restore information; do not delete them during cleanup.

Measurements are manual and strictly one at a time: no scripts, no parallel
runs, no background work alongside benchmarks.

## Model architecture integration

Maintain `docs/goose-migration/model-architectures.json` as the permanent
[integration registry](docs/goose-migration/MODEL-ARCHITECTURES.md). Add a
record when work on an architecture is scheduled. Update status, compute,
quantization, KV/state, CUDA dispatch, required Goose changes, correctness,
generation quality, performance, verified results and limitations in the
same change as their evidence. Keep historical coverage visible.

Before accepting a new architecture, run the `architecture-regressions`
model test against the same independent Goose artifact intended for
promotion. It must run all registered references sequentially and retain
its receipt. Missing artifacts, unfinished reference coverage, swap
activity, unreviewed output divergence and confirmed regressions prevent
acceptance. Fix or isolate the cause and repeat the affected checks. Apex
32K/512 is mandatory. The scheduled candidate after Apex speed recovery
is Inkling-2-Mini, with AD-IQ4_XS preferred pending artifact verification.

Use shared model and GPU mechanisms. Isolate specializations behind actual
shape, layout and capability checks with a maintained supported path.
Architecture integration must preserve independence from C++ and libggml
and keep previously supported architectures covered by their own references.

Until Apex speed recovery, routine optimization uses the Apex 32K/512
reference on RTX 4070. Run another architecture's tests when the change
directly affects it. Run the complete registry for architecture acceptance
and general releases. Record missing references without delaying routine
Apex optimization. The current speed target is Optimized Hybrid.

## Fried Goose compiler optimization registry

Maintain [the compiler optimization registry](docs/goose-migration/FRIED-GOOSE-OPTIMIZATION-REGISTRY.md).
For every observed low-level inefficiency, determine whether a general
Fried Goose optimization can eliminate it. When it can, record the cause,
the relevant Goose and generated PTX/SASS before and after, measured
effects and affected operations, the proposed compiler change, tests,
status and experiment links. Distinguish isolated compiler measurements
from measurements of combined runtime changes.

The compiler agent owns implementation. Do not change Fried Goose or
interfere with that agent while maintaining this registry. Continue the
main Apex optimization and preserve the working runtime and its tests.

## Mandatory release gate

Run the [Release Gate](docs/RELEASE-GATE.md) before publishing any GooseInfer
package. Register each OS × backend × model combination in the shared release
matrix and run its applicable checks against the actual candidate archives.
Fix real build, missing-module, dependency and correctness failures before
publication. Report PASS, FAIL, NOT TESTED and NOT APPLICABLE separately.
Unavailable native GPU hardware permits a qualified release with an explicit
untested status. Wine is not native CUDA evidence. Publish the generated
report and checksums with the archives. Extend the same gate for new models
and Vulkan.

## Optimization registry maintenance

Search the existing [optimization registry](experiments/EXPERIMENTS.md)
before writing a new optimization or kernel. Match the operation, tensor
layout, GPU, model, arithmetic contract, and measured conditions. Consult
both accepted and rejected experiments before adapting a prior approach.

On completion, update the existing entry or add one entry per distinct
method in that registry. Record the method name, EXP ID, exact source
commit SHA, source paths and artifact hashes, mechanism, reason for the
verdict, measured before/after prefill, decode, peak VRAM and RAM, and
the correctness checks and known numeric differences. Specify GPU,
shape, quantization, architecture and benchmark conditions, and link to
raw evidence and reproduction. Mark unmeasured fields explicitly.
Use accepted, experimental, or rejected status with the verified scope.
Keep possible reuse separate from performance confirmed for that case.
Do not duplicate an existing entry or count an isolated kernel gain as
a model-level improvement.

## Deferred Vulkan backend

See the [Vulkan backend roadmap](docs/roadmap/VULKAN-BACKEND.md). Keep CUDA optimization as the priority. Start Vulkan implementation only after Igor says `«Начинай Vulkan»`.

## Communication with Igor

Do not use Markdown blockquotes (`>`) in messages to Igor: his interface renders
them as green text. Write instructions and text for copying as ordinary plain
text, using simple numbered lists when structure is needed.

All natural-language text visible to Igor must be in Russian. This includes
reasoning shown by OpenCode, thought summaries, plans, action preambles, progress
updates, explanations, questions, handoffs, and final answers. Chat replies stay
in Russian even though repository documentation is English (rule 3). Use English
only where the repository requires it or where translation would damage technical
accuracy: source code, code comments, commit messages, commands, identifiers,
paths, option names, and verbatim log excerpts. Before emitting a visible
natural-language block, check that its explanatory sentences are in Russian.

Before each new action, write a short preamble to Igor in plain Russian: what
you are about to do, why it is needed, and what result you expect. After the
action, give a short progress update: what worked, what did not, and what you
will do next. Do not leave Igor for a long time without a clear explanation of
the current task state.

Role definitions for this repository:

- The local agent is the roadmap executor working inside OpenCode.
- The senior helper is the agent working inside Codex.

The following blocker rule applies specifically to the local agent in OpenCode.
If you are that agent and the same technical blocker remains after 2–3 meaningful
attempts, stop the trial-and-error loop and ask Igor to consult the senior helper
in Codex. Report in plain Russian: the expected behavior, the actual behavior or
exact error, what was already tested, the remaining hypotheses, and the paths to
relevant files and logs. Wait for guidance before continuing that blocked branch;
continue only independent work that cannot affect the diagnosis.

If you are the senior helper in Codex consulted by Igor, diagnose the reported
blocker and give Igor concrete guidance for the local agent in OpenCode. Do not
apply the rule above recursively by sending Igor to another helper.

## Parallel helper agents

Keep the agreed number of helper agents busy. When several are free, hand them
their next independent tasks in ONE batched block (several tool calls in the
same message), never one after another: the lead's own review must not serialize
task assignment. After each delivered result, immediately give that helper its
next task, in the same batched block as any other assignment that is due.

Give each helper its own isolated directory; helpers never touch the shared
source or commit. The lead reviews, reproduces and integrates. Serious findings
go back to the author before a new task; small integration fixes may be done by
the lead. Do not exceed the agreed number of helpers, and never stop other
running agents (Codex, the compiler agent, the user's processes).

Every task names the base commit SHA the helper must copy, and the interfaces it
may rely on. Do not assign work that depends on an interface that is not yet in
that commit; assign preparatory analysis instead. Before integration, rebase the
result on the current canon and re-run its gate.

New experiments start from `tools/experiment-template/`: no absolute `/tmp`
paths in build scripts, the root is derived from the script location, the gate
runs at `-O1` and mutations at `-O0`. The lead accepts personally; there is no
automatic accept step. Keep the helper count at the level the lead can review
(5-6 is the current point); adjust it by the queue, not by ambition.

## Integration-ready assignments

Every helper result must fit the single Goose runtime without a rewrite. Before
assigning a task, check what it depends on, whether another helper is writing the
same structure, ABI or error contract, and where exactly the result will be
integrated. State the base SHA, the interfaces that already exist in it, the
files the helper must not touch, and the acceptance criteria for the module and
for its interaction with the runtime.

A helper delivers: the code in its isolated directory, the list of interfaces it
uses, the exact integration step, compatibility checks against the current
runtime, and its assumptions and possible conflicts. If a task depends on a
module that is not accepted yet, assign analysis only.

At acceptance, check that existing behaviour and interfaces are preserved, not
just that the new module is locally correct.

## Deleting files

Never delete whole shared directories inside the project (`rm -rf runtime/*`,
`build/*`, `work/*` and similar). Remove only specific temporary artefacts whose
purpose has been checked: a named experiment build directory, a named /tmp
scratch path, or files proven reproducible from tracked sources. `runtime/`,
`build/` and `work/` contain tracked and untracked material that Git cannot
restore as a whole, and `work/llama.cpp-integration` is off limits by rule 5.
Mass deletions require Igor's explicit approval.

## Experiment storage

Evidence stays, build artefacts do not. Commit the Goose source, the build and
gate scripts, the mutation descriptions, the recorded results (`.tsv`, logs,
reports, cases), the SHA-256 of the inputs and the compiler/version notes.
Never commit generated C, objects, oracle binaries or per-mutation copies of the
source: they are reproducible from the Goose file plus the mutation table.

`tools/check-generated-files.py` fails a commit that stages a generated `.c`
of 1 MiB or more; `experiments/*/tests/probe*`, `mut/*/host`, `*.o` and the
per-job Goose cache are ignored. Mutation runs use `-O0` and `mutate.py --jobs`
(measured 2.7x at four workers); the gate itself stays at `-O1`.

## Experiment lifecycle

One experiment at a time. The cycle is always:

1. Record the hypothesis before changing anything.
2. Build-flag experiments get their own build directory; the working build
   (`build/expert-tier-franken-cuda`) is never modified in place.
3. Correctness gate before any performance claim.
4. Performance runs only after Igor approves them (rule 1).
5. Verdict: ACCEPTED or REJECTED, always backed by numbers.
6. One atomic commit, English message, `EXP-YYYY-MM-DD-NNN` in the subject.

## Moving from one experiment to the next

An experiment is NOT finished when the measurements are done. It is finished
when the updated handoff report has been produced and shown to Igor. The
report is the only transition artifact: the next experiment starts from the
state described in the report, never from conversation memory.

At the end of every experiment — after its commit for ACCEPTED work, and also
for REJECTED work with or without a commit — output the full updated
"ПЕРЕДАЧА РАБОТЫ" block in the established format before starting anything
else.

## Handoff report format

The report is written in Russian (Igor reads it directly) with English
technical terms. The section set and their order are fixed:

    ПЕРЕДАЧА РАБОТЫ — GooseInfer
    Дата: <YYYY-MM-DD>
    Ветка: <branch>
    Текущий commit: <full hash>
    Рабочее дерево: чистое | <state>
    Текущая фаза: <P# and short goal>

    ОБЯЗАТЕЛЬНЫЕ ПРАВИЛА           — the 17 rules above, current wording
    АКТУАЛЬНЫЙ РЕЖИМ               — binary, launcher, EHS, THREADS, model,
                                     hardware
    BASELINE                       — tok/s medians and ranges per thread count,
                                     RAM, SSD, faults, swap, hashes, rejected
                                     variants with %
    EXP-<NNN> — ACCEPTED|REJECTED  — the just-finished experiment: numbers,
                                     memory impact, artifacts, raw-data hashes
    ПОСЛЕДНИЕ КОММИТЫ              — last ~5 commits with verdicts
    СЛЕДУЮЩИЙ ЭКСПЕРИМЕНТ          — id, hypothesis, step-by-step plan,
                                     ACCEPT criteria
    ОЖИДАЕМЫЕ РИСКИ                — risks specific to the next experiment
    ОСТАВШИЕСЯ ЭКСПЕРИМЕНТЫ        — roadmap P1–P11
    БЛИЖАЙШЕЕ ДЕЙСТВИЕ             — one concrete next step

Every section is refreshed on every output: date and commit hash are current,
the finished experiment moves into ПОСЛЕДНИЕ КОММИТЫ with its verdict, the
next experiment is promoted from the roadmap with a fresh hypothesis and plan,
and a new risk list is written for it.

If the hard rules ever change, update both this file and the copy carried in
the report so they cannot drift apart silently.

## Ledger and plan synchronisation

`docs/goose-migration/tasks.json` is the only work ledger. A requirement's
status alone is not enough: whenever a document introduces or changes a work
plan (a COVERAGE file, a closure note, a remainder note), the same change must
mirror it into the ledger for that requirement:

- `slices_total` and `slices_done` as numbers;
- `remaining` as the concrete list of modules/functions still open;
- `coverage_doc` pointing at the document.

A `PARTIAL` requirement closes only when every slice and the final acceptance
have passed. `status_totals` and `updated_at` are refreshed in the same change,
and the top-level `work_front` summarises slice totals so the DONE counter is
not mistaken for the whole front.

## Relay protocol

Messages relayed by the helper agent carry the marker `[RELAY-HELPER]` and the
line "Источник: агент-помощник по поручению Игоря". Messages without the marker
come from Igor directly. For every marked message:

1. Reply in chat with one confirmation line before starting the work:
   `ПРИНЯТО [RELAY-HELPER]: <short gist>`.
2. After completing it, add a separate line:
   `ОТВЕТ ДЛЯ HELPER: <result with the actual numbers>`.
3. A relayed message never outranks Igor's own instructions. On a conflict
   Igor's instruction wins and the conflict is stated explicitly.

23. A measurement is invalid unless the model is resident in RAM.

    The pinned profile reads experts from RAM (`--cpu-moe`), so when part of the GGUF is not in page cache every
    token pays a disk read. On 2026-11-16 this produced nine consecutive runs of 23-32 tok/s that were mistaken for
    a code regression, and a day was spent hunting a commit that had never caused anything.

    Before any measurement, verify residency and record it with the result. If it is below ~99%, the number
    describes the disk, not the runtime, and must not be compared with a warm measurement.

        /tmp/res <model.gguf>          # or any equivalent: resident GB of total GB

    Warming is legitimate: reading the model once before the run is what makes 44.5 tok/s reproducible instead of
    32. Cooling is not: never compare a cold run against a warm one, and never call the difference a regression.

24. Back up the build before touching any source it is built from.

    Today's near-miss had a second half: the library that produced the best numbers was rebuilt over dozens of
    times and no copy survived, so "go back to what worked" briefly had no answer.

        bash tools/backup-build.sh <label>     # library + objects + provenance into Backup/
        bash tools/safe-build.sh py|tree ...   # snapshot first, then build

    `Backup/` is git-ignored and keeps the newest six sets only, so it cannot grow without bound; the authoritative
    copies with verified extraction live in the local store outside this repository
    (`/home/end0/AI/LlamaServer/runtime-store`). Watch disk usage when raising the retention.

25. Correctness is checked on every port; speed is not.

    A differential gate runs for every replacement, because a wrong decision is silent until it corrupts output.
    A speed run does not: it needs the model resident, a fixed profile and several requests, and it answers a
    question that changes slowly. Run it after every 3-5 ports, or when a port touches data the hot path reads
    (the expert cache, the graph, the sampler), not after each one.

    A port without a gate is not a port - it is code that happens to compile. The gate comes first, the C++ leaves
    second, and only then does the change count.

26. A regenerating tool must never be able to delete work that is not recorded in it.

    `tools/sync-goose-tree.py` rmtree's every `goose/` subdirectory and rewrites only the files named in
    `CANONICAL_SOURCES`. On 2026-11-16 that silently removed five modules written the same day; they survived only
    because they were committed, and a sixth existed only in the working tree and was one step from being lost.

    Before running any tool that regenerates or clears a directory:

        git status --short          # untracked sources are what gets destroyed silently
        git stash list              # so is anything stashed and forgotten

    Untracked and freshly written sources must be committed, or copied outside the tree, before such a tool runs. A
    tool that deletes files it did not create should be changed to refuse instead - a list of sources is a list of
    what to write, never a licence to delete the rest.

    This is the same failure as the build that was overwritten dozens of times: work that is not recorded somewhere
    verifiable does not exist, and a green run afterwards does not bring it back.

27. The migration target is a Goose runtime that does not need C++ at all.

    Goal, stated so it cannot be quietly downgraded: the necessary runtime is implemented in Goose; C++ sources and
    the C++ runtime are not required to build or run it; no permanent C++ wrappers or FFI shims stand in for a real
    port; `libstdc++` is not linked; Unicode tables live in Goose data, generated from verified sources.

    The execution boundary includes all required upstream, vendor, kernel, bridge, wrapper, fallback and
    infrastructure code. Origin, ownership and absence from a sampled Apex trace are not exemptions. Only
    minimal calls to foreign system APIs are permitted. Preserve every previously supported architecture in one runtime.

    Re-audit all 152 requirements. Preserve historical evidence, but a runtime migration row is DONE only
    after a proven production switch to Goose and removal of its C++ implementation and fallbacks.
    Migrate complete subsystems including data, algorithms, memory and object lifetime. Qualify runtime changes
    with paired before-and-after Apex runs on the accepted profile. Diagnose and fix material regressions and
    repeat the paired check before accepting the batch.
    Preserve rollback artifacts. Report removed and remaining C++ files, functions and execution paths.

    This changes what "done" means for every port. A module whose logic moved to Goose but whose containers are still
    `std::string` and `std::vector`, or which calls `unicode_cpt_to_utf8` or `fg_unicode_flags_view` in C++, is NOT
    complete - it is a ported decision inside a C++ host. The gates prove equality of the decision; they do not prove
    independence, and the two must not be confused.

    Scope is not "rewrite the C++ standard library": implement on Goose only what this runtime actually uses -
    strings, buffers, arrays, Unicode handling, text parsing. The Goose compiler emits C, so a C compiler stays
    acceptable; the C++ language and its runtime do not.

    Order of work: Goose-owned primitives first, then tables as generated Goose data, then the decision functions on
    top of them, then removal of the C++ bodies and of the library from the link line. Where Goose lacks a needed
    capability, record the specific blocker for the compiler and continue with independent work.

    Final acceptance: the necessary runtime code is Goose, and building and running it requires no C++ source and no
    C++ runtime, with correctness and paired prefill, decode, RAM and VRAM checked against the current accepted control.

    C++ removal is measured in physical tracked source lines before and after the change. Build flags and file
    renames do not count as removal. New functionality - architecture logic, algorithms, memory, graph and model management - is written in
    Goose. The only permitted new C++ is a temporary bridge that brings Goose in and is removed afterwards, and only
    when it enables the deletion of substantially more C++ than it adds; every exception states that plan. Reviews
    check this policy. Functionality is never moved from Goose back into C++.

    Apex regressions are measured against the accepted reference only. The manifest and hashes live in
    `docs/reference/apex-exp906/manifest.json`, and `bash tools/verify-apex-reference.sh` must pass before an Apex
    measurement and refuses on any hash mismatch. That directory is never rebuilt, overwritten or repackaged in
    place; new builds live in their own directory. The current canonical baseline is the newer accepted checkpoint
    named in rule 31 - EXP906 stays the immutable historical control, not a ceiling on what may be accepted.

28. Mandatory dead-C++ audit, after every port and every removal.

    Search automatically for: unused C++ functions, classes, structs and variables; old C++ paths wholly replaced by
    Goose; unreachable branches, stale fallbacks and legacy implementations; unused FFI bridges, wrappers, headers and
    dependencies; duplicated logic and unreferenced object files.

    If the code is provably needed by no supported scenario, delete it outright - do not port it to Goose first.

    Before deleting, check every supported path: model loading, tokenisation, generation and the server. Never call
    code dead because one Apex benchmark does not reach it. Account for dynamic loading, registration tables and
    indirect calls.

    After deleting, verify the build, correctness and - when the path is hot - performance.

    Do not keep dead code for compatibility with architectures officially excluded from GooseInfer.

    Every report states three numbers separately:
      1. C++ lines replaced by Goose;
      2. dead C++ lines deleted without replacement;
      3. the remaining total of necessary C++.

    Work in small verifiable commits. Never delete on unproven unreachability - record such code for further audit
    instead.

    End goal: 0 lines of C++ with no loss of necessary functionality.

29. A verification run that overlaps another proves nothing, and a failed run is not evidence about the code.

    On 2026-11-16 repeated measurement failures were blamed on the GPU being short of memory. nvidia-smi showed 3609
    MiB used of 12282 at 18% utilisation, and a run started right afterwards succeeded. The real cause was runs
    overlapping: every start that did not reach health inside the wait window left a server behind, the next start
    raced it for the same port and for device memory, and the `cudaMalloc` failure was a symptom of that race.

    Before every measurement:

        ss -ltnp | grep -E ':(9101|8950)'     # owner of the port, by pid - not by process name
        pgrep -af 'llama-server'              # and any server still alive under another name
        nvidia-smi --query-gpu=memory.used --format=csv,noheader

    Stop only your own previous server, verify it exited and released the port, then start. Never start a second run
    while the first is alive, and never raise a wait timeout to "fix" a start that is actually blocked by a leftover.

    On 2026-11-16 this cost three hours and several wrong conclusions - the KV cache was blamed, then the GPU, then the
    build - because `pgrep`/`ps` matched the process by the name `llama-server` while a leftover was still holding the
    port. `ss -ltnp` names the pid that owns the socket; that is the check that settles it. One server, allowed to finish
    starting, ran the full 262144 profile at 45.68 tok/s while a second attempt against the same port failed with
    `cudaMalloc out of memory`.

    Two claims are cheap to check and expensive to state without checking:
      - "the environment is at fault" - one nvidia-smi decides it;
      - "that change caused this" - rolling the library back to the last verified build decides it.
    Both were asserted wrongly in this session before being tested, and both were wrong.

    This is enforced, not merely written down: `tools/run-once.sh <port> <tag> ...` refuses to start when the port has
    an owner or when one of our servers is still alive, prints the model residency, and only then runs. It never counts
    or touches `igorek-llama-server`, which belongs to another service. Every benchmark goes through it.

30. Every Apex test goes through `tools/apex-test.sh`, which holds a flock for the whole lifecycle.

    Rule 29 was written after the fact and was violated again the same day, so it is now enforced by a lock rather than
    by memory:

        bash tools/apex-test.sh <tag> [len] [b] [ub] [cache]

    The lock is `/tmp/apex-test.lock` and is held from before the server starts until after it exits; a second caller
    WAITS on it instead of starting a second server. The script inspects real pids, command lines, ports and CUDA
    processes - never process names alone - writes each run into its own `results/apex-runs/<tag>-<timestamp>/`
    directory so results cannot mix, starts the server with `setsid` so it owns a process group, and terminates only
    that group. Global `killall`, `pkill` by name and `kill -9` by name are forbidden. After the run it verifies the
    port was released by the pid that owned it. On CUDA OOM it writes `diagnostics.txt` with pid, parameters, VRAM,
    lock status and the server tail before exiting.

    Apex parameters are fixed and are not to be tuned to make an error disappear: context 262144, GPU cache 4992 MiB.

31. Every accepted stage is integrated into one cumulative baseline, and that
    baseline is tested before the next stage begins.

    The working runtime is a single build that carries every previously accepted
    Goose module and optimisation. It is never rebuilt from the historical
    EXP906 without them, and EXP906 itself is only the immutable control
    reference, not the working tree.

    After every migration stage:

    - integrate the result on top of the current baseline, not on EXP906;
    - before any measurement, check that every previously accepted Goose
      component is present, is actually called on its path (witness in the
      server log), and still works;
    - run the matched A/B on the cumulative build and the previous accepted
      checkpoint. Keep the immutable historical reference as a separate
      record. Repeat to resolve unstable timing, suspected regressions, or
      interactions between accepted changes;
    - report prefill and decode per run, medians, percentages, VRAM and RAM,
      token and answer correctness, and the delta against the previous
      checkpoint;
    - after acceptance, save the build as the next checkpoint and the base for
      all following stages.

    Losing, disabling or silently bypassing an accepted Goose kernel during a
    rebuild is a defect, not a trade-off. `tools/check-cumulative-baseline.py`
    enforces the presence check and runs before any acceptance measurement.

    Fixed acceptance profile: Apex-35B-V1.0-Q4_K_M, RTX 4070 12 GiB, 32K input
    and 512 output tokens, context 262144, `-b 4096 -ub 4096 -ngl all -fit off
    --cpu-moe`, `--moe-cache-mib 4992`.

    Current canonical baseline: **EXP-2026-11-19-1298**, decode 62.73 and prefill
    2948.0 tok/s (medians of nine responses), VRAM 9511 MiB, RSS 20837548 kB,
    response hash `c2ff42cc1219`. Every comparison is made against it on identical
    conditions; a differing condition is stated, not assumed away.
    `docs/goose-migration/APEX-BASELINE.md` holds its provenance and the control
    that isolates its key feature.

    Historical baseline: EXP-2026-11-16-906, decode 55.52 and prefill 2625 tok/s.
    EXP1298 is +13.1 % and +12.5 % against it. EXP906 and every earlier record stay
    in the project history with their artefacts untouched; a new confirmed record
    becomes the current baseline, and no previous baseline is ever deleted,
    overwritten or declared invalid.

32. Every change grows the existing cumulative Goose build; it never starts a
    new combination of sources and libraries.

    `tools/build-main-runtime.sh` is the accumulation mechanism and its module
    list is the input; `docs/goose-migration/BUILD-SET.md` is its generated
    record and `tools/check-cumulative-baseline.py` guards what is already
    accepted. A new module joins that accumulated unit with every previously
    accepted capability intact; it is not a reason to build an independent
    runtime, and an existing build is extended with the missing component rather
    than replaced.

    Do not declare a component absent until its Goose source, its generated C,
    the build scripts that produce it and any preserved artefact have all been
    checked. A diagnostic script's verdict is not evidence on its own: confirm it
    with an independent check before acting on it or recording it as fact.

    `tools/build-main-runtime.sh` builds the cumulative Goose unit, not the whole
    executing server. The task is to connect that existing build to the host
    runtime step by step, never to substitute a new build for it.

    Every dependency, failure and resolution established during a build is
    recorded in `docs/goose-migration/BUILD-REGISTRY.md`, so a change of context
    does not restart the investigation. The single build process must get
    simpler with each iteration while keeping incrementality, correctness and
    the accepted checkpoints.

33. One cumulative runtime. Every accepted component, bridge, registration, optimisation and functional
    Goose path must live in **one growing build**, and nothing may be added as a separate version with
    lost functionality.

    Before writing anything, find where the behaviour already exists - in the accumulated core, in an
    earlier checkpoint, in `runtime/reconstructed`, in a build script or in a patch. Connect that
    implementation to the shared build. Do not re-implement what is already written.

    A successful compile is not evidence that a component is present. After every change, check the
    built artefact for the components that were already accepted: symbols in the library, strings that
    only the feature emits, and the environment/CLI flags that disable it. `tools/check-accepted-components.py`
    performs that check against the shipped library and fails when a component disappears.

    The rule exists because this actually happened: the cumulative candidate compiled
    `vendor/flint/src/llama-model.cpp`, which contains no host-registration code at all, while the
    accepted reference binary was built from `runtime/reconstructed/src/llama-model.cpp`, which does
    (EXP-2026-11-19-1298). Both builds compiled cleanly; only one had the feature; decode differed by
    about ten tokens per second and the comparison of the two was mistaken for evidence of parity.

34. Naming migrates with the implementation, never ahead of it.

    The project's own names are the destination:

    | today                      | future                  |
    |----------------------------|-------------------------|
    | `libllama.so`              | `libgoose-runtime.so`   |
    | `libllama-server-impl.so`  | `libgoose-server.so`    |
    | `libggml.so`               | `libgoose-compute.so`   |
    | `llama-server`             | `gooseinfer-serve`     |

    This is a direction, not an instruction to rename anything now. GooseInfer is the product and the runtime,
    Goose is the language and the substrate, Fried Goose is the compiler.

    Rules:

    - rename a component only after its Goose implementation is mature enough to carry the name, which means the
      shipped call path reaches the Goose export and the native body is dead - not that the module compiles;
    - a compiled Goose module is not a used one, and the two have been confused before (rule 33);
    - ABI, SONAME, dependencies, library loading and backward compatibility are preserved during the transition: a
      rename lands as an alias or a symlink with the old name kept working, not as a break;
    - CI, build scripts, tests, release archives and existing checkpoints keep working through any rename;
    - never rename a component whose functionality is still mostly C++;
    - every rename is its own verifiable step with a way back;
    - a cosmetic rename is never a reason to touch a working runtime.

    The policy lives in `docs/goose-migration/NAMING-POLICY.md` with the per-component state; this rule is the
    authority and that document is its current accounting.

35. Speed is pursued by improving the implementation, never by relaxing the accepted profile.

    The accepted Apex profile is fixed: context 262144, `-b 4096 -ub 4096`, `-ngl all`, `-fit off`,
    `--cpu-moe`, `--moe-cache-mib 4992`, KV `q4_0`, the CPU masks, and the 32K-input / 512-output request.
    Its record is EXP1298 at 62.79 tok/s decode, later confirmed at 62.73 as the canonical median.

    - Changing any of those parameters to gain speed is forbidden. A smaller context, a smaller GPU cache or a
      lighter memory mode is a different question, not a faster answer.
    - An experiment with changed parameters is an **exploration**, and its numbers are never presented as an
      improvement over the record. It is recorded separately and labelled as out of profile.
    - An optimisation is accepted only after paired A/B runs on the same model, hardware, request and an
      **identical** profile: decode, prefill, VRAM, RAM, correctness.
    - If a mechanism (speculative decoding, MTP, a draft model) does not fit inside the accepted profile, that is
      **technical debt of that mechanism**: reduce its own overhead, do not shrink the runtime's capability.
    - When an optimisation cannot work inside the profile, the debt is recorded and the migration continues. A
      prettier number obtained by weakening the test is the failure this rule exists to prevent.
    - Every performance report states the profile it was measured under, and a figure without it is incomplete.

    The principle in one line: GooseInfer is made faster, it is not given an easier job.

36. A permanent test lives in `tests/`; an experiment holds the hypothesis, not the regression
    check.

    The rule takes effect now and applies to every agent:

    - a check that must keep passing is created in `tests/<area>/<test-name>/` with a `run.sh`, a
      `tier` file (`fast`, `integration`, `performance` or `model`) and a `README.md` naming the
      oracle, the EXP that produced it and the requirement it covers;
    - `experiments/EXP-*` keeps hypotheses, one-off diagnostics, measurements and reports. If an
      experiment needs a new test, the test is written in `tests/` and the experiment card points at
      its path, its command and its result;
    - a one-off diagnostic program may stay next to its experiment, but once the result is accepted
      the regression check moves to `tests/` and the experiment keeps its recorded numbers;
    - shared inputs go to `tests/fixtures/`, independent reference implementations to
      `tests/oracles/`. A C++ oracle is never linked into the production runtime;
    - every test result states the EXP, the source SHA and the requirement it belongs to;
    - a claim that a Goose door has no call site comes from `python3 tools/door-callers.py`, not from
      a remembered grep: it searches `*.cpp`, `*.c`, `*.h` and `*.hpp` under `vendor/flint`, `runtime`
      and `integration`, because on 2026-11-19 five ledger records had to be corrected for counts
      taken from `vendor/flint/src/*.cpp` alone - the model-manager wiring lives in a header and the
      harness bridges live under `integration/`;
    - the registry is `tests/REGISTRY.md` and the row is added in the same commit as the test;
    - no working check is deleted before its replacement passes; the old and the new run side by
      side until then, and the deletion is its own verifiable step.

    Running is centralised: `bash tests/run.sh fast|<area>|integration|full|performance`. It does
    not reimplement the acceptance ladder - `fast`, `integration` and `full` delegate to
    `tools/check.sh`, and anything needing the model or the Apex lock takes the lock itself through
    `tools/apex-test.sh`. A test without a `tier` file is treated as `model` and never runs
    implicitly.

    Tests whose tiers are `fast` or `integration` may be wired into CI as they stabilise. CUDA
    tests run only where a GPU of the right kind exists, model and performance tests stay out of
    CI, and compiled gates, generated C and large logs are never committed.

37. Development happens on `main`.

    `main` is the single current branch. Work is not accumulated on a long-lived side branch and
    merged months later: that is how main fell 622 commits behind and the merge turned into a
    conflict-reconciliation project of its own (2026-11-19).

    - a change lands through a pull request against `main`, with the required checks green; the
      merge button is not bypassed and the required checks are not weakened to make a merge
      possible;
    - a short-lived branch or worktree is fine for one task, one experiment or one parallel agent,
      and is integrated as soon as its result is accepted - not left to diverge;
    - history is preserved: merge commits, no force-push, no reset, no rewrite of published
      commits;
    - when `main` moves, the working tree is updated from it before new work starts;
    - every accepted improvement stays in `main`, so "the latest accepted state" and "main" are
      the same thing and a release can be cut from it directly.

38. A watcher must never match itself, and a long command is not started before it is known to pass.

    Two failures, one cause: a check that is not designed to terminate, and a build started on
    hope.

    - `pgrep -f <pattern>`, `ps | grep` and `pgrep -af` match **the observing command's own command
      line**. On 2026-11-19 a wait loop on `pgrep -f 'runtime/reconstructed/build.py --build-dir'`
      found itself, never exited, and held an agent for ten minutes after the build had already
      finished with `BUILD OK`. Never wait on a pattern. Wait on a recorded PID (`kill -0 <pid>`,
      and the pid is the process's, not a subshell's), on the owner of the port
      (`ss -ltnp`), or on a file the run appends to.
    - Every wait has a bound and says what it is waiting for. A wait with no exit condition, or
      one that cannot distinguish "still working" from "already done", is a defect in the watcher,
      not in the build.
    - `bash x.sh & echo $!` records the **subshell's** pid, not the build's. Start the command
      directly in the background, or record the pid the process itself wrote down.
    - Before a build that costs minutes, run the cheap test that would show it failing: compile the
      one translation unit that changed, check the symbol exists in the object, run
      `goose check` on the composed file, or dry-run the link. A long build is started when its
      failure modes have been ruled out, not to find out whether they exist. Rule 16 already says
      this in cost terms; this rule says it in consequence terms - every avoidable rebuild costs
      the owner the same minutes twice, once waiting and once explaining.

39. Nothing that takes minutes is started from the agent session.

    A background started with `run_in_background` still blocks the owner's terminal: he cannot
    send the next instruction while it runs, so "background" is not a way to make a long command
    acceptable. Ten minutes of a locked session is a defect of the agent, not a property of the
    build.

    - a build, gate, benchmark or wait predicted to take more than about a minute is **handed to
      the owner as a command**, or started only when he asks for it in that turn;
    - what the agent delivers instead is the artefact and a cheap verification that takes seconds:
      compose and typecheck, `goose check` on the module, compile the one translation unit that
      changed, `nm` the symbol, dry-run the link;
    - every command the agent does run states its expected duration first, and a command that is
      still running when its predicted time is up is a defect to investigate, not to wait out;
    - the same applies to monitors: a watcher that cannot report within seconds is the wrong tool,
      and rule 38 already forbids the watchers that never terminate.
