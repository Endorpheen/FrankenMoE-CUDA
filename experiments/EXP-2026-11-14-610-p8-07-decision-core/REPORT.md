# EXP-2026-11-14-610 — decision core (P8-07 S4)

Base commit: `e6e4f469`. Native counterpart: `tools/server/server-decision.cpp`
(1618 lines). Work tree: `/tmp/opencode/sub-614`, the repository itself was not
touched.

## Conclusion

The offline half of the decision core is ported to Goose and gates byte for
byte against a transcription of the native functions on a fixed corpus and
deterministic random documents. The model-dependent remainder is S5 and is
listed below; nothing in this slice needs a model, a tokenizer, a socket or a
thread.

- gate: `FG_DECISION cases=400 failures=0` (two runs, byte-identical output)
- mutations: 12 of 12 caught, 6 in each half
- port: `source/decision-core.goose`, 2987 lines, 22 exports, prefix `dc_`

## Gate

| | result |
|---|---|
| comparisons | 400 |
| failures | 0 |
| fixed documents | 12 (nesting, escapes, control bytes, 2/3/4-byte UTF-8, float thresholds, whitespace, special tokens) |
| operations per document | kev, kev text, dumps (compact, indent 1, compact with spaces), sort, d1-omni escape, replace, marker replace |
| floats | 12 (`0`, `-0`, `1e15`, `1e16`, `1e-5`, `1e300`, `5e-324`, ...) |
| question bodies | 35, every native error message |
| state bodies | 15, media counting and filtering |
| answers | 14 (two variants, bias, media temperature, canonical legend, NaN, wrong sizes) |
| group rows | 8 |
| random documents | 10 (LCG, depth 2) through 6 operations |
| native source | transcription on `nlohmann::ordered_json`, line cited per function |

Command: `bash tests/build.sh`.

## Mutations

| half | range | caught |
|---|---|---|
| 1 | `tests/mutate.py 0 6` | 6/6 |
| 2 | `tests/mutate.py 6 12` | 6/6 |

The mutations change one semantic decision each: the score level limit, the
noul swap, the `no`/`yes` aliases, the h2o instruction default, the choice sort
comparison, the score level keys, the kev item strip set, the escape marker
byte, the float exponent threshold, the integral float suffix, the cut marker
and the quote of `>`. Each was caught by at least one named case; the source is
restored after every mutation.

## What is not covered (S5)

`init` from the GGUF metadata (the type, the temperatures, the score bias, the
label table, the special-token spellings, `get_temperature`), the image and
audio loaders (`handle_media`, the base64 decoding, the mtmd chunk types), the
jinja `systemone` render, `d1_labels`, the `fill_task` family and
`fill_task_laya`. `h2o_cut_state` is ported with a byte-per-token surrogate for
the tokenizer branch; the cut rule, the `...` line and the U+FFFD at a cut
inside a character are compared with `dump_safe`, the token boundaries are not.

## Hashes

SHA-256 of the tracked artefacts:

| file | bytes | SHA-256 |
|---|---|---|
| `source/decision-core.goose` | 2987 lines | `b5d9e60b79a7d842ac3413de7fbf1a67dd1f1c306b940b33efb30a455c8b314f` |
| `source/jsonparse.goose` | 1074 lines | `ee56f4e479dd05b65f57589ca4916767d923d13e73187087a95ab27416bfd70e` |
| `tests/fixture.cpp` | 1534 lines | `1b953a1f2ed4c3db9bc88729e95b2738210667098823079361ef7d080b84aacc` |
| `tests/build.sh` | 18 lines | `e0fe6bfb4b9c2bb04dc01961c712ba595e0a713d6520cda0336bb6636f5b949c` |
| `tests/mutate.py` | 179 lines | `537cb404f311b0de50ad52cee09b01b7c466302b334f096b2a3130526724beb1` |
| `tests/gate.log` | 401 lines | `1ab5bb7864e87c6f721f4a2f7ef27d4dbbd9f08c0e6cc0cb31858995401e6142` |
| `README.md` | | `78b7585aca0cf9b071c45a75a83297de85fdf494cdb2c231de9f8224a1cb29a5` |

`source/jsonparse.goose` is a byte-identical copy of
`goose/json/json-parser.goose`; the hash above is the copy.

## Environment

| | |
|---|---|
| Goose | `goose-2026.3-string`, git `e0501dc7dee4522428a46f813cebbce0ad62736e`, runtime `1e714e6146bc` |
| compiler | `gcc-13` / `g++-13` (Ubuntu 13.4.0-10ubuntu1) |
| port build | `-O0` (`--standalone`, one C file) |
| fixture build | `-O1 -std=c++17`, `-lm` |
| nlohmann | `llama.cpp-flint-cuda/vendor` (the library behind `common_json`) |
| network, model, tokenizer | none |
