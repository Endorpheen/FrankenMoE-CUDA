# EXP-2026-11-19-1306: the component check, extended and proved able to fail

Rule 33 requires that every accepted component stay in the one growing build. A check that only ever reports
success is not evidence, so both directions were measured.

## Positive: the shipped build

```
present  host-registration
present  moe-cache
present  goose-sampler-chain
present  goose-q4-k
present  goose-shim-marker
present  goose-grammar
present  goose-spec-prefix
present  goose-dry-limit
present  goose-dry-apply
present  goose-bulk-bridge
present  expert-cache
check-accepted-components: ok, every accepted component is in the shipped library
```

Eleven entries, each one a piece this migration accepted: the host registration (EXP906/1298), the sampler chain and
its doors, the grammar automaton, the speculative prefix, DRY's limit and apply, the bulk bridge the ggml base
library links against, the expert cache, and the shim's own diagnostic prefix.

## Negative: a build that lost one

```
python3 tools/check-accepted-components.py builds/checkpoint-20261119-chain
check-accepted-components: FAIL, 1 accepted component(s) absent: host-registration
exit status 1
```

The earlier checkpoint is used deliberately as the negative control: it is a real build of this project that really
did lack the registration, and the check names exactly that one component rather than failing vaguely.

## Why this matters more than the eleven lines

The 10 tok/s loss happened because a compile was accepted as proof of presence. The check now reads the artefact,
the benchmark launcher runs it before a single request, and this card shows it failing when it should - which is the
only way to know it is not a green light glued to the wall.
