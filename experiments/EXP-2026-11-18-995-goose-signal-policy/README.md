# EXP-2026-11-18-995: Count the adapters honestly, and put the signal policy where it belongs

Status: the debt figure was understated by two files; one of them held a second
implementation of a Goose decision. Both fixed. 143 modules, 38 281 checks.

## 1. The debt number was wrong, in my favour

The report said 6 files / 1818 lines. The enumeration was:

    ls integration/goose-standalone/*.c integration/goose-standalone/*.cpp \
       integration/m1/host/*.c | grep -v '/tests/'

`cli/` is a subdirectory, so the glob never reached it. `serve-main.c` (133 lines) and
`tokenize-main.c` (213) were not counted - not because they are tests, but because the
pattern stopped one level short. The comment above the code said only files under
`tests/` are excluded; the code silently excluded two more.

Enumerated with `find` now, so a new directory is counted the day it appears:

    files: 8   lines: 2192      (was 6 / 1818)

The report also names how many files are excluded and why, so the exclusion can be
audited instead of trusted: `excluded: 26 file(s) under tests/`.

**The correct number is worse than the one I was reporting.** Adapter debt is supposed
to shrink, and a figure that drifts down because of an enumeration gap is the worst kind
of progress.

## 2. A parallel implementation of a decision, found by checking

`tools/check-adapter-parallelism.py` turns a comment into a check: no adapter may
define a function sharing a name with a Goose export. Rule: an adapter is translation
only, so a second definition with the same name is either duplication or a plan to
bypass the first.

It found one:

    FAIL cli/serve-main.c defines on_signal, which is exported by
         goose/server/shutdown-policy.goose

and the hit was real. `goose/server/shutdown-policy.goose` owns the policy - a first
interrupt runs the shutdown, a second forces `exit(1)` with a message - and **the module
was in no composed unit**. Nothing could reach it. The host meanwhile had its own
simplified rule: any signal means stop.

Two separate defects in one: dead Goose code, and a host decision standing in for it.

## What was done

- `shutdown-policy.goose` renamed its private helpers to `sp_put` / `sp_st_i64` - it
  conflicted with other modules' `put` and `st_i64`, which is why it could not simply
  be added - and joined the composed unit (142 -> **143 modules**).
- `fg448_st_i64` added to the host: a three-word store and nothing else. The action,
  the exit code and the message flag are chosen in Goose.
- `serve-main.c` keeps only an async-signal-safe counter (`signals_seen++`); formatting,
  buffered I/O and any Goose call stay out of a handler, which is what signal safety
  requires. The loop asks `on_signal` and acts on its answer.
- The new `signal-policy` gate pins the contract: first interrupt -> action 1, no code,
  no message; second -> action 2, code 1, message 1; no handler -> action 0.

## Evidence

| check | result |
| --- | --- |
| `check-adapter-parallelism.py` | PASS, 1614 exports against 8 adapter files |
| `signal-policy` gate | 7 checks, 0 failures |
| unit | 142 -> 143 modules, 1166 globals, 0 duplicates, 147 extern |
| whole suite | **38 281 checks, 0 failures** across 10 tests |
| live server | serves a request, then exits cleanly on SIGTERM |
| adapter debt | **8 files / 2192 lines**, honestly enumerated |

## Not claimed

The shutdown *plan* (`shutdown_plan`, seven ordered steps) is still not wired: the
standalone server has no sessions, models or backend to tear down in that order, so
wiring it now would be a plan with nothing to plan. It joins the unit and is available
when the runtime has state to release.

A second interrupt forcing exit is implemented and gated but not exercised on the live
server, because that needs two signals inside one request and the loop is what would be
tested, not the decision.

No model was started, no benchmark was run.
