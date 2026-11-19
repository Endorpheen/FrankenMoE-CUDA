# Speculative doors exported from the same build

Integration slice for `goose/speculative/spec-doors.goose`, joined at commit
`6804b859d045cb2e2018b8b813d224d824eba523` ("EXP-2026-11-18-1206: speculative doors exported from
the same build").

## What the module contains

The exported doors for the speculative decisions. Each door forwards to the ported decision in
`goose/speculative/spec-decisions.goose` and adds nothing: the point of a door is a stable name,
not a second implementation.

## Where the evidence is

The decisions themselves are gated by the experiments named in their module header, and the
rejection-acceptance arm by EXP-2026-11-18-1215 on 5006 cases. This slice records that the doors
are exported from the same build, and makes no measurement of its own.
