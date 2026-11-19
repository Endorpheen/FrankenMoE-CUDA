# EXP-2026-11-18-990: test-runner.goose does generate standalone C

Status: the ledger's recorded blocker was **false**. 22 new checks, whole suite
38 252.

## The claim that was recorded

`docs/goose-migration/tasks.json`, P10-09, remaining:

    test-runner.goose cannot generate standalone C: tr_match/tr_selected export
    bool/slice signatures; decide i64 return or drop export, then re-gate

That is a diagnosis and an instruction. Nobody had run the command.

## What the command says

    goose --standalone -o t.c --header t.h goose/misc/test-runner.goose
    wrote t.c (232912 bytes)
    wrote t.h (2468 bytes)

It generates. The C compiles with no errors, defines 12 exported symbols, and the
header carries the slice parameters as `sl_u8` - the pass-by-value form this project
already requires. `tr_match` returns `uint8_t`; a `bool` export is not a problem for
the generator.

## What was actually missing

A caller. Generating C proves the generator finished, not that the result works. Two
things only showed up when the exports were called from C:

1. `goose runtime error: goose_init must be called before exported functions`. The
   generated runtime refuses exported calls until `goose_init` has run. A gate that
   skipped it would look like a failure of the module.
2. `sl_u8.data` is `uint8_t *`, not `const uint8_t *`, so a string literal needs the
   cast through `uintptr_t`.

## The gate

`tests/gate.c` calls the decisions and asserts their contracts, 22 checks:

| function | cases |
| --- | --- |
| `tr_match` | literal match, non-match, empty pattern, unanchored search, `^` anchoring, `\|` alternation |
| `tr_selected` | empty filter, filter mismatch, exclude match, both filters |
| `tr_classify` | no return code, blocked dependency, pass, matching skip code, any other code |
| `tr_status` | disabled decided before the command |
| `tr_percent` | 100, zero reported, half, all disabled |

It is now section 3 of `tools/verify-unit-gates.sh`, because it needs a different
generation than the canonical unit and would otherwise rot.

## A placeholder I removed

The first draft of the gate contained

    check(tr_match(S("abc|xyz"), S("xyz"), 0) == 0 || 1, "placeholder");

which does not compile (three arguments) and, had it compiled, would always pass.
Deleted rather than repaired: a check that cannot fail is not a check.

## Result

| gate | checks | result |
| --- | --- | --- |
| test-runner (generated on its own) | 22 | PASS |
| whole suite | **38 252** | **0 failures** (9 gates) |

## What this changes in the ledger

P10-09's first remaining item is withdrawn: it was a stale diagnosis, not work. The
requirement stays PARTIAL for its other item (`bench-diagnostics.goose` attaches only
when its scripts exist).

## Not claimed

The gate covers the decision functions, not the whole runner: `tr_plan`, `tr_row` and
`tr_summary` take many parameters and are exercised by the unit-based gates elsewhere.
No model was started.

## Reproduction

    bash tools/verify-unit-gates.sh
    # the standalone half alone:
    goose --standalone -o t.c --header t.h goose/misc/test-runner.goose
    cc -O1 -I. gate.c t.c goose_host.o -lm -o gate && ./gate
