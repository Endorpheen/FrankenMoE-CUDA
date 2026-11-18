# EXP-2026-11-18-997: Count the decisions no unit contains

Status: the defect class that produced the last two cycles' findings is now a list with
a ratchet. 89 modules, all classified, 0 unclassified.

## Why

EXP995 found `shutdown-policy.goose` and EXP996 found `limits.goose`: each owned a
decision, each was in no composed unit, and each had been replaced in the host by a
weaker local rule. Both were found by accident, one per cycle.

An accident twice is a class. A decision that no unit contains is invisible to every
gate - the suite composes its own unit and can only test what is in it - and invisible
to every capability line in the report, which is written from the artifact.

## What was done

`tools/check-unwired-modules.py` walks `goose/`, takes the module list out of
`tools/build-main-runtime.sh`, and separates three cases:

| case | meaning |
| --- | --- |
| in unit | composed, so reachable |
| excluded by path | the path names it as not-a-runtime-decision: archive, a rejected variant, a staged copy, a fixture, a corpus, a diagnostic draft, a mutation workspace |
| unwired | exports decisions, and no rule excludes it |

40 modules are excluded by path - by rule, not by judgement, so nobody maintains 300
entries.

The 89 unwired ones are recorded in `docs/goose-migration/UNWIRED-MODULES.json` with a
status and a reason. **Every one is classified, none is guessed:**

- 19 architecture ports (gemma, glm, kimi, mistral, deepseek, qwen2/3/4) - rule 17 keeps
  the ability to port them; the unit carries Apex (`qwen35`/`gdn`) only;
- 16 jinja modules - the chat-template engine, and the standalone has no chat path;
- ~30 json modules - grammar and schema for constrained generation, not wired;
- streaming, multipart, prerouting and the other HTTP decisions - most need a parsed
  request body, which the listener does not keep;
- Unicode table variants - the unit carries the two the tokenizer uses.

Six needed reading rather than pattern matching, and two of those are a different
problem:

    goose/cpu/dequant.goose         same two export names as goose/model/quant-dequant.goose,
                                    which IS in the unit; bodies differ (143 vs 158 lines):
                                    two versions of one decision, one must be retired
    goose/cpu/goose-moe-cache-slru  slru_plan is also exported by
                                    goose/model/slru-policy-goose.goose, and neither is in
                                    the unit: two versions of the cache policy

Neither is wired, deliberately: wiring a second version of a decision is worse than
leaving it visibly unwired, and deleting either needs the audit that rule 28 describes.

The one entry that matters most is `goose/sampling/sampler-chain-full.goose`: it holds
the whole chain, and the unit carries `sampler-chain` plus the three filters but not the
draw. That is the P6-10 remainder, and it is now a line in a file instead of a memory.

## The ratchet, and proof that it works

The check fails only on a module absent from the baseline. A check that cannot fail is
not a check, so it was made to fail:

    $ cat > goose/server/zz-ratchet-probe.goose   # one exported function
    $ python3 tools/check-unwired-modules.py
    unwired modules: 90 | excluded by path: 40 | unclassified in baseline: 0
    FAIL goose/server/zz-ratchet-probe.goose exports 1 decisions and is in no unit and no baseline
    exit=1

    $ rm goose/server/zz-ratchet-probe.goose
    unwired modules: 89 | excluded by path: 40 | unclassified in baseline: 0
    PASS no unwired module outside the recorded baseline

`migration-status.sh` prints the count, so the report carries it without anyone
retyping it:

    unwired decisions   unwired modules: 89 | excluded by path: 40 | unclassified in baseline: 0

## Result

| check | result |
| --- | --- |
| unwired modules | 89, all classified, 0 unclassified |
| excluded by path | 40 |
| ratchet | fails on a new module, passes when removed |
| report | prints the count |
| modules changed | none - this cycle added a check, not a port |

The gate suite is not re-run: no file under `goose/` changed, and the unit is unchanged.
38 295 checks stand on unchanged inputs.

## Not claimed

The baseline is a ratchet, not a verdict. "Classified" means the reason was read and
recorded; it does not mean the entry is correct to leave unwired. Several entries name
work that is genuinely outstanding - the sampler draw above all - and the number 89 is a
backlog, not a badge.

Duplicate pairs are recorded, not resolved: deciding which of `dequant.goose` and
`quant-dequant.goose` is authoritative, and which of the two SLRU modules to keep, needs
a body-by-body comparison rather than an export-name match.

No model was started, no benchmark was run.
