# EXP219: cumulative Unicode routes — repairs a silent chain loss

## What went wrong

EXP215, EXP216 and EXP217 each port a different function in `unicode.cpp`. All three live in the
**same translation unit**, so only one version of that file is compiled per stage. Every one of
those EXP directories copied the **pristine native** `unicode.cpp` and added only its own route.
The result: each stage shipped with exactly one Unicode route and silently dropped the previous
one.

Measured, not assumed — the witness sets of consecutive runs, correctly compared:

    exp215 -> exp216   lost: FG215U          new: FG216E
    exp216 -> exp217   lost: FG216E          new: FG217C

EXP216 lost a route that had **2,003,762 calls**, and every gate still passed: tokens exact, all 128
probabilities exact, sha `e68df05b`. This is the EXP204 failure mode again — the one the stage
protocol's witness check exists to catch.

## Why the check did not catch it

The tool is fine; I ran it wrong. `scripts/check_witnesses.py` only reports a loss when given
`--baseline`. I invoked it without one, so the baseline was empty, nothing could be missing, and it
printed `verdict: OK` while listing `new FG204X FG205G …`. Re-run properly:

    check_witnesses.py --run exp217/... --baseline exp216/...
    MISSING vs baseline (a component silently reverted):
      - FG216E
    verdict: REGRESSION

An empty baseline can never fail. Any run of this tool without `--baseline` is a no-op dressed as a
gate.

## The repair

`integration/unicode.cpp` now carries all four routes (215 cpt_from_utf8, 216 cpt_to_utf8,
217 cpts_from_utf8, 218 tolower), each behind its own toggle.

    FG215U_UNICODE_CPT calls=256
    FG216E_UNICODE_ENC calls=268
    FG217C_UNICODE_CPTS calls=255553
    FG218T  (absent: not reached by this profile, as recorded in EXP218)

62 witness modules. `--baseline exp217` now reports **no missing** and `new FG215U`, `new FG216E`.

**EXP215 dropping from 2,003,762 calls to 256 is legitimate supersession, not a fallback.** EXP217's
Goose function decodes inline instead of calling back per codepoint, so `unicode_cpt_from_utf8` is
no longer entered from that loop. Both are Goose; the remaining 256 calls come from other sites.
The distinction matters: a counter drop has two possible meanings and they must be told apart.

## Result — ACCEPTED

    2552.49 / 25.78, tokens and all 128 probabilities exact, sha e68df05b

| pair | prefill | decode |
|---|---:|---:|
| 1 | +0.41% | +1.81% |
| 2 | +0.28% | +1.72% |
| **mean** | **+0.34%** | **+1.77%** |

Decode is up ~1.8% in both pairs, against EXP217. Inside 2% either way, and the point of this stage
is coverage: the same runtime now executes three Unicode modules instead of one.

## Protocol fix

Stage step 5a now requires `--baseline <previous stage>` explicitly. An earlier note in this repo
said witness checks were mandatory; what was missing is that they are mandatory *with a baseline*.
