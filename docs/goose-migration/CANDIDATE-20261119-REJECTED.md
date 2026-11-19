# Candidate 20261119 is rejected: a fresh build does not reproduce the accepted set

Built 2026-11-19 with `tools/build-independent-apex.sh` into
`/home/end0/AI/LlamaServer/frankenmoe-cand-20261119-json`, verified with
`tools/verify-candidate.sh`, and rejected on its own evidence.

## The numbers

| measurement | accepted release (EXP1476) | this candidate |
|---|---|---|
| unit doors carried | 198 | **87** |
| duplicate global symbols | none | none (6623 defined) |
| `fg_sp_prefix` (speculative prefix door) | 3 hits | **0** |
| `FG_GGUF_PARSE` (loader call path marker) | 1 hit | **0** |
| accepted components | all present | **4 MISSING**: goose-spec-prefix, goose-dry-limit, goose-dry-apply, gguf-reader-call-path |

`tools/check-accepted-components.py` fails on the candidate and passes on the release. Nothing about
this candidate may be measured, published or called "the runtime".

## What the build actually composed

From its own log: `modules built: 36`, `goose modules compiled from sources: 10`, `tokenizer unit
modules composed: 14`. So the fresh build links 36 bridge objects and 10 Goose modules from source -
not the 155-module composed unit that `tools/build-main-runtime.sh` builds and that
`tools/unit-carry.py` counts against. The two are different artefacts, and comparing the 1256-door unit
with this library is a comparison of different things, which is why the carried share reads 6% here and
15% for the release.

## Why this matters more than the candidate

The accepted release carries components this fresh build does not. Either the build is not
reproducible, or the accepted artefacts were produced by a path that no longer exists in the tree. Both
are serious, and rule 22 is explicit that a set which cannot be reproduced cannot be replaced:

- `tools/verify-clean-build.sh` must be able to build the accepted set from tracked sources and show
  the same component list. It has only ever been run with `--compose-only`, which stops before the
  full build (recorded in P10-06), so the full path had never been compared with the accepted set;
- `fg_sp_prefix` comes from a speculative-decoding bridge; `FG_GGUF_PARSE` marks the loader call path.
  Both were accepted earlier and both are absent here, so whatever produced EXP1476 included bridges
  that today's script does not link.

## What happens next

1. keep EXP1476 as the accepted baseline and the published release; this candidate is deleted from
   consideration, not published;
2. compare the object lists of the accepted build and this one, module by module, and find where the
   two paths diverge (`builds/flint-acc/bin` carries 198 doors and is itself of unknown provenance,
   so it is evidence of what was built, not of how);
3. only then repair the path, rebuild, and require `tools/verify-candidate.sh` to pass before any
   measurement is started.
