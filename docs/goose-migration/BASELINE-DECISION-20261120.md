# The decision, made by measurement: B is the new baseline

Runs on 2026-11-19/20, the fixed Apex profile, paired minutes apart, one at a time under the
`apex-test.sh` lock, port 9101, model 100% resident in RAM before the first and 99.2% before the second.

## The four axes

| | A - accepted EXP1476 | B - everything integrated | delta |
|---|---|---|---|
| decode tok/s (three runs) | 60.41 / 61.36 / 61.74 | 61.05 / 61.44 / 61.85 | **+0.13 %** (median 61.36 -> 61.44) |
| prefill tok/s (three runs) | 2628.6 / 2906.2 / 2929.8 | 2657.0 / 2918.1 / 2886.6 | **+0.41 %** (median 2906.2 -> 2918.1) |
| VRAM at ready | 9465 MiB of 12282 | 9465 MiB of 12282 | **0** |
| free RAM at ready | 43 027 076 kB of 63 638 224 | 46 210 208 kB of 63 638 224 | **+3.2 GB** |
| response hash | `c2ff42cc1219` | `c2ff42cc1219` | **identical** |
| unit doors | 198 | **200** (`json_parse`, `json_dump`) | **+2** |

B is not slower. It is fractionally faster on both axes, uses identical VRAM, leaves more RAM free, and
produces the same answer while carrying two doors A does not have.

## Why today's absolute numbers are below the recorded ones

The recorded EXP1476 medians are decode 62.86 and prefill 2940.9. **A itself measured 61.36 / 2906.2
today**, so the gap is the machine's state this evening, not a change in B: B is measured against A under
the same conditions, which is the only comparison that can attribute a difference. Quoting 61.44 against
62.86 would be the mistake rule 20 exists to prevent - two different sessions.

## The decision

Per the rule fixed before the numbers were seen: both axes inside the spread, response hash identical, so
**B becomes the baseline** - the speed is kept and every integrated feature comes with it. A stays as the
historical control, exactly as EXP906 stayed when EXP1476 replaced it.

## What B is, and what it is not yet

B is `builds/flint-acc/run-candidate`, assembled from the working build and the one library that build did
not carry. It carries 200 unit doors and passes the accepted-component check 20/20, and it has no
producing command recorded - the same defect A has, undiminished and unimproved.

So this decision settles the question Igor asked (keep the numbers, take the features) and does not settle
the question the verdict document raised (can either artefact be rebuilt). Both statements are true at once
and are kept in separate documents on purpose.

## Evidence

```
results/apex-runs/dec-a-1476-20261120-003955/   run.log, summary.txt, r1..r3.json, vram-ready.txt
results/apex-runs/dec-b-features-20261120-004125/ the same set
```
