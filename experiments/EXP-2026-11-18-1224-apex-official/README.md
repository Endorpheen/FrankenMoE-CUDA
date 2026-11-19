# EXP-2026-11-18-1224: the sampler chain on the official Apex profile

First measurement of the chain through `tools/apex-test.sh` - the single mandated entry point (rule 30),
with its flock, its own port and directory, real pid checks and the accepted profile: context 262144,
`-b 4096 -ub 4096 -ngl all -fit off --cpu-moe --moe-cache-mib 4992`, 512 output tokens, three clients.

Same binary (`builds/flint-acc/bin`), one switch.

| run | decode tok/s | prefill tok/s | sha |
|---|---|---|---|
| chain ON | 45.60 (45.48-46.17) | 2273.0 | `c2ff42cc1219` |
| chain OFF | 45.56 (45.25-45.78) | 2264.6 | `c2ff42cc1219` |
| delta | **+0.09%** | **+0.37%** | identical |

## The SHA is the result that matters

`c2ff42cc1219` is the accepted baseline's control hash. Both runs reproduce it **with the Goose chain
enabled**, so on the full profile the ported samplers produce the same tokens, logprobs and text as the
native path - bit for bit, three clients each.

## And the cost is not measurable here

+0.09% decode is inside the run-to-run spread (the two ranges overlap almost completely), and +0.37%
prefill likewise. That is a **different answer** from the earlier -0.88% measured at context 4096 with a
text workload, and both are true: the chain's cost depends on what the sampler actually does on the
workload, and on this profile it is not detectable.

Per the project rule, this pair does not separate the configurations, so neither is declared better; what
it does establish is that the chain does not cost a measurable amount on the acceptance profile.

## Two gate observations, recorded not fixed

1. `tools/apex-test.sh` records `dependencies.txt` but does **not** copy the libraries into the run
   directory, so `check-cumulative-baseline.py` cannot pass on its directories (it reports the three
   libraries missing). The accepted runs on record are in the same position, which means that gate was
   satisfied by habit rather than by command until now.
2. When the libraries **are** present (`results/p610-prov-1222/`, copied by the paired harness), the check
   passes for the three Goose components and fails only on `libmoe_mmid_tiled.so`, which this tree does
   not produce - recorded separately in EXP-2026-11-18-1223.

Neither is fixed here: the first is a change to a rule-30 script and the second to an acceptance gate, and
both are the owner's call.
