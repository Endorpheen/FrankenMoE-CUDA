# EXP-2026-11-18-1197: what the grammar port costs now that it actually runs

Before today the port declined almost every call, so its cost was measured on a half-idle path and
reported as 3-5x. It now decides the whole call - 1 736 513 candidates with zero disagreements - and
the cost is measured properly, same binary, one switch:

```
g_on_1  decode=4.33  prefill=5.9
g_off_1 decode=50.68 prefill=21.7
g_on_2  decode=4.31  prefill=5.8
g_off_2 decode=49.87 prefill=20.0
```

| axis | ON | OFF | ratio |
|---|---|---|---|
| decode tok/s | 4.32 | 50.3 | **11.7x slower** |
| prefill tok/s | 5.85 | 20.9 | **3.6x slower** |

So the earlier 3-5x was an artefact of the declines, not a property of the port: when the port
decides every candidate, its per-candidate loop costs about twelve times the native pointer walk.

## What this means, stated plainly

- the grammar port is **correct** - zero disagreements on 1.7 million candidates, and constrained
  output identical to the reference library;
- it is **not usable** at 12x, and it will not become usable by rearranging the host: the earlier
  eight host-side alternatives were closed and the remaining lever is the generated per-candidate
  loop;
- therefore it stays **opt-in**, and the item cannot be marked done: "ported and correct" is not
  "replaced".

This is a compiler-facing cost, recorded in TECH-DEBT for the compiler agent with the numbers, not
with an opinion.
