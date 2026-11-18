# EXP-2026-11-18-984: every unit gate re-run in one pass

Status: 5 gates, **22 737 checks, 0 failures** on the current 142-module unit.

## Why

By EXP983 this series had accumulated a thousand-plus checks across a dozen
experiments, each verified once in its own cycle. A check that passed yesterday and
has not run since is not evidence about today's unit - and every module was edited
after its own gate (`http-listen` five times, `http-request` twice). "Still passes"
had become memory rather than measurement.

## What was done

`tools/verify-unit-gates.sh` composes the canonical unit from
`tools/build-main-runtime.sh`, generates and compiles it **once**, then links and runs
each gate against that one object. Gates whose oracle lives outside the unit are
listed as SKIPPED with the reason, so the report cannot imply coverage it does not
have.

## Result

| gate | checks | result |
| --- | --- | --- |
| typical | 2 535 | PASS |
| top-n-sigma | 17 112 | PASS |
| dry | 2 440 | PASS |
| http-request | 566 | PASS |
| http-listen (routing, auth, CORS, keep-alive, pipelining) | 84 | PASS |
| **total** | **22 737** | **0 failures** |

Unit: 142 modules, 1158 globals, 0 duplicates.

Skipped, with reasons, because their differential oracle is not in the unit: the
reader adapter (EXP953), the vocabulary wire (EXP954), the BPE engine (EXP955), the
detokenizer (EXP956), the live sampler comparisons (EXP966-968) and the wired-sampler
two-library test (EXP970/971). Those need the accepted `libllama`, and the script says
so instead of quietly omitting them.

## Two failures of my own in the script, both cost a run

1. the gates include `runtime.h`, but the generated header is named after the unit
   (`unit.h`); the script now copies it under the expected name rather than editing a
   dozen tests;
2. the unit was compiled without `-ffunction-sections`, so `--gc-sections` kept every
   host symbol the whole unit references and all five gates failed to link. The flag
   is required, not cosmetic, and the script now says why.

Both were the same shape as the rest of this series: the build succeeded and the
result was wrong.

## Not claimed

This re-runs the gates that need only the unit; it does not re-verify the
libllama-based differentials, which are named above. It says nothing about model
behaviour, and no model was started.

## Reproduction

    bash tools/verify-unit-gates.sh
    cat builds/verify-gates/run-http-listen.log
