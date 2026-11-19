# Centralised tests

`tests/` is the permanent test tree. `experiments/` keeps hypotheses, measurements and one-off
diagnostics. The rule and its reasoning live in `AGENTS.md` (rule 36) and `CLAUDE.md`; the layout,
the conventions for writing a test and the run commands live in `tests/README.md`.

## Why it exists

Before 2026-11-19 a check was born inside the experiment that needed it, ran once in its own
cycle, and was never run again. `tools/verify-unit-gates.sh` already records the symptom: a
thousand checks across a dozen experiments, every module edited after its own gate, so "still
passes" was remembered rather than measured. A permanent test is the fix: it lives where it will
be found, it is listed in one registry, and its runner is one command.

## The three pieces

| piece | responsibility |
|-------|----------------|
| `tests/<area>/<test>/run.sh` + `tier` + `README.md` | one test, self-contained, no absolute paths |
| `tests/REGISTRY.md` | the single list: area, test, path, EXP, requirement, tier, status |
| `tests/run.sh` | the single entry: `list`, `fast`, `<area>`, `<area> <test>`, `integration`, `full`, `performance` |

## What delegates, and to what

- `fast`, `integration`, `full` for the migration build go to `tools/check.sh`, which owns the
  stamps, the staged tree and the acceptance ladder. `tests/run.sh` adds the permanent tests
  around those levels, it does not redefine them.
- Anything needing the model or the Apex lock goes through `tools/apex-test.sh`, which holds
  `/tmp/apex-test.lock` for the whole lifecycle and owns the `127.0.0.1:9101` endpoint.
- A test without a `tier` file is treated as `model`: it never runs implicitly.

## Migration of the existing checks

The migration is incremental and each batch is its own commit:

1. the check is copied into `tests/<area>/<test>/`, keeping its oracle and its cases;
2. the copy is run against the live source (not a pinned experiment copy) and must produce the
   same verdict as the experiment recorded;
3. the registry row is added in that same commit;
4. the experiment card gains the path, the command and the result; its card and numbers stay;
5. the old copy is deleted only after the permanent one passes, as a separate verifiable step.

First batch (2026-11-19): `tests/grammar/reject-alternates`, from EXP-2026-11-19-1491. Seven
cases, `gate: cases=7 fails=0`, identical to the experiment's recorded run, now built against the
live `goose/sampling/sampler-chain-full.goose`.
