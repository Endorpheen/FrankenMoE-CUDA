# Tokenizer decisions join the accumulated unit

Integration slice for `goose/model/tokenizer-decisions.goose`, joined at commit
`b80dd8e206a2a8ed87238f3fa817339ac8b5b88c` ("EXP-2026-11-18-1184: tokenizer decisions join the
accumulated unit; TD-S8 records export fn").

## What the module contains

The tokenizer decisions ported from `src/llama-vocab.cpp`, composed from the experiments that
gated them: text normalisation, the two merge orders, UTF-8 accounting, the score-path step, the
WPM match search, the SPM merge order and the final symbol emission.

## Where the evidence is

The module header states that each function keeps the name of its gate and that the prefix `tk_`
marks the area. The per-decision gates are those experiments; this slice records only the
integration into the cumulative unit, and makes no measurement of its own.

The doors of the same vocabulary are in `goose/model/tokenizer-doors.goose`
(EXP-2026-11-18-1229, verified live in EXP-2026-11-19-1233).
