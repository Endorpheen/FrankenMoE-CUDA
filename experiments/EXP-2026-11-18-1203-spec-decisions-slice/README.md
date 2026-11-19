# Speculative decisions join the accumulated unit

Integration slice for `goose/speculative/spec-decisions.goose`, joined at commit
`94540aae2764225691742d712d56578533149f1d` ("EXP-2026-11-18-1203: speculative decisions join the
accumulated unit").

## What the module contains

Speculative-decoding decisions ported from `common/speculative.cpp`, `ngram-cache.cpp`,
`ngram-map.cpp` and `common.h`, with the prefix `sp_`. Each function keeps the behaviour its own
gate measured.

## Where the evidence is

The per-decision gates are the experiments named by those functions. The rejection-acceptance arm
was later gated on 5006 cases in EXP-2026-11-18-1215, which also touched the doors in
`goose/speculative/spec-doors.goose`. This slice records the integration only.

P6-11 stays PARTIAL: the routes are wired and the decisions are gated, while a real MTP run
against a compatible draft GGUF has not happened.
