# AGENTS.md — Rules for AI agents working in FrankenMoE-CUDA

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
3. Comments, commit messages, and all repository documentation are in English.
   Documentation means every tracked text artifact: experiment cards
   (`experiments/*.md`), `experiments/EXPERIMENTS.md`, `ROADMAP.md`,
   `docs/*.md`, patch READMEs, and any new notes or summaries. A Russian
   version of any document is produced only when Igor explicitly asks for it.
4. One experiment = one atomic commit.
5. Never touch user changes in `work/llama.cpp-integration`.
6. No destructive Git.
7. After every commit, output the updated handoff block (format below).
8. EHS stays off by default.
9. Never present single-response throughput as aggregate throughput.
10. Accepted improvements must be enabled by default in new builds. A new
    feature's env/CLI flag exists only to disable the feature (like
    `GGML_EXPERT_PINNED_RING=0`), never to opt in.
11. When this file changes, update `CLAUDE.md` synchronously in the same
    commit: `cp AGENTS.md CLAUDE.md`.

Measurements are manual and strictly one at a time: no scripts, no parallel
runs, no background work alongside benchmarks.

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

    ПЕРЕДАЧА РАБОТЫ — FrankenMoE-CUDA
    Дата: <YYYY-MM-DD>
    Ветка: <branch>
    Текущий commit: <full hash>
    Рабочее дерево: чистое | <state>
    Текущая фаза: <P# and short goal>

    ОБЯЗАТЕЛЬНЫЕ ПРАВИЛА           — the 11 rules above, current wording
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
