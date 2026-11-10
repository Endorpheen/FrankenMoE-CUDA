# EXP226: MMQ Apex series under the compiler/runtime resource lock

First measurement run under the coordination protocol Igor established: the Rust/Goose compiler work is
now owned by DeepSeek in OpenCode, `resources.lock` is the exclusive-resource token, and I held an
exclusive `flock` for this entire series.

## Protocol actually used

    exec 9>".../runtime-compiler-link/resources.lock"
    flock -w 300 9        # held for the whole series, inherited by the runner
    ... A1, B1, B2, A2 ...

Alternating schedule: round 1 = A then B, round 2 = B then A, so each arm takes the cold first slot
once. Before starting I verified the lock was genuinely free (`flock -n` succeeded and `lsof` showed no
holder) rather than inferring it from an empty file.

## Result

| run | arm | decode tok/s |
|---|---|---:|
| A1 | native | 23.85 |
| B1 | u64+ne2 | 23.42 |
| B2 | u64+ne2 | 23.17 |
| A2 | native | 25.30 |

    native   mean 24.58   spread 1.45 (5.9%)
    u64+ne2  mean 23.30   spread 0.25 (1.1%)
    delta    -5.2%

Tokens and all 128 saved probabilities exact in all four runs.

## What is credible and what is not

**Credible:** the direction. The Goose route is slower in **both** orders — 1.8% when it ran second and
8.4% when it ran first — so unlike my earlier series this is not explained by the warm-up slot. The
Goose arm is also far tighter (1.1% spread) than native (5.9%), which is what a stable-but-slower arm
looks like next to a jittery control.

**Not credible:** the magnitude. Native read 23.85-25.30 here against 26.0-26.7 in earlier sessions, so
the whole band is depressed by roughly 8% and neither mean belongs to a clean configuration.

**The contention is mine, not the compiler's.** `ps` during the series: opencode 60%, igorek-factory
41%, igorek-desktop's bundled python 40%, chrome 31%, and my own llama-server 131%. None of it was
compiler work. Holding `resources.lock` correctly excluded my collaborator and changed nothing about my
own session's load — which is worth knowing before the next attempt: the lock is necessary but not
sufficient on this host.

## Verdict

Not promotable on this evidence, for two independent reasons: the direction says the Goose route costs
time, and the magnitude is not measurable at this host's stability. MMQ Q8_1 stays native.

That also settles the variant question by default: whether the chain links the typed or the u64 source
does not matter while the route as a whole loses. The two defects I found earlier (chain links the
typed variant EXP208 rejected; switch is opt-in against rule 10) remain real and remain unfixed, but
neither is worth acting on until the route is competitive.

## To get a usable number

The host needs the native control back at 26.0-26.7 and needs my own session's processes quiet, which I
cannot arrange from inside a measurement — opencode and the desktop app are the session itself. The
realistic options are a second machine, or accepting the kernel-level harness (spread under 1%) as the
decision instrument for this question, which is what EXP208 already used.
