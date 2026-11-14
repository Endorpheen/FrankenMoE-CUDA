# Build cache: measurements and options (research only)

No cache change is implemented. This records what a cache would buy and what it
would risk, so the decision can be made with numbers.

## Measured stages (EXP563, 10 MB generated C, 32-core host)

| stage | time | note |
|---|---|---|
| goose --standalone | 15.0 s | always needed when the Goose source changes |
| gcc -O1 generated C | 153 s | gate build |
| gcc -O0 generated C | 7.7 s | mutation build |
| link + fixture compile | 0.3–3.7 s | cached separately |
| gate run | 0.2–3 s | |

A single gate costs ~170 s; a mutation costs ~25 s at -O0.

## What a cache can reuse

1. **gen.o keyed by (source SHA, optimisation, compiler version).** The template
   build script already does this. Re-running a gate with an unchanged runtime
   drops from ~170 s to ~4 s. This is the case during documentation, fixture
   edits and review reruns, roughly 2–3 of the 10–15 minutes per slice.
2. **fixture.o keyed by fixture mtime.** Already used; saves 2–4 s.
3. **Mutation results keyed by the mutated source SHA.** Only helps when the same
   mutation is re-run (halves re-run, baseline after restore); a fresh table gets
   no benefit.
4. **ccache for the generated C.** The file differs on every Goose change
   (embedded source path and line numbers), so hit rate is near zero for
   different slices; within one slice, repeated links of the same object are
   already covered by 1.

## Estimated gain

- Per accepted slice: ~2–2.5 minutes saved by 1 (gate reruns after fixture-only
  changes), out of 12–15 minutes of integration. About 15–18%.
- Mutation runs: no gain for new mutations; re-running a half saves its full
  cost (~3.5 min) with 3.
- Across the session: 4 slices × ~2.5 min ≈ 10 minutes saved, which is one
  extra slice of review time.

## Risks

1. Stale gen.o when the compiler changes without a version in the key; the
   template includes it, older scripts do not.
2. Stale object when a header changes but the source SHA does not (the generated
   C depends only on the Goose source, so this is limited to fixture headers).
3. Disk growth: each slice keeps one 10–25 MB object; cleanup is manual.
4. False confidence: a cached gate run proves the object, not the procedure;
   the first run after any source change must rebuild.

## Recommendation

Keep the keyed gen.o cache that the template already provides, and standardise it
in older experiments only when they are touched for another reason. Do not add a
mutation-result cache until the mutation table stops changing, and do not rely on
ccache for generated C.
