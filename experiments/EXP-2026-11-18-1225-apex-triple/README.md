# EXP-2026-11-18-1225: three paired runs on the acceptance profile

The full protocol, through `tools/apex-test.sh` (rule 30 entry point: flock, own port and directory, real
pid checks), same binary, one switch, accepted profile (ctx 262144, `-b/-ub 4096`, `--cpu-moe`,
`--moe-cache-mib 4992`, 512 output tokens, three clients per run per configuration).

```
pair 1  ON  decode [43.26, 43.98, 43.24]  prefill [1866.8, 2173.4, 2179.9]  sha c2ff42cc1219
pair 1  OFF decode [43.23, 43.89, 45.80]  prefill [1892.1, 2197.0, 2190.7]  sha c2ff42cc1219
pair 2  ON  decode [44.37, 45.35, 45.20]  prefill [1923.0, 2232.3, 2246.0]  sha c2ff42cc1219
pair 2  OFF decode [44.66, 45.28, 46.25]  prefill [1953.4, 2236.5, 2256.1]  sha c2ff42cc1219
pair 3  ON  decode [45.63, 46.04, 46.19]  prefill [1979.0, 2262.2, 2282.8]  sha c2ff42cc1219
pair 3  OFF decode [45.39, 46.14, 45.83]  prefill [1949.9, 2272.1, 2276.9]  sha c2ff42cc1219
```

| axis | ON (median of pair medians) | OFF | delta |
|---|---|---|---|
| decode tok/s | 45.20 | 45.28 | **-0.18%** |
| prefill tok/s | 2232.3 | 2236.5 | **-0.19%** |
| sha | `c2ff42cc1219` x3 | `c2ff42cc1219` x3 | identical |

## What this establishes

- **equivalence on the acceptance workload**: all six runs, chain on and off, produce the same hash as
  the accepted baseline - tokens, logprobs and text identical;
- **no measurable cost**: -0.18% and -0.19% are far inside the spread, and the warm-up trend across the
  six runs (43.3 -> 46.2 decode) is larger than the difference between configurations, which is exactly
  what pairing is for.

## The trend worth noting

Decode rises through the sequence regardless of configuration (43.3, 43.9, 45.2, 45.3, 46.0, 46.1...).
A single unpaired measurement at the wrong moment would report a 6% difference between "configurations"
that are the same thing. The pairing and the interleaving are what make the -0.18% a statement rather
than an artefact.
