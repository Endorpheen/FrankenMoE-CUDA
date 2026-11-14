# Decision core (P8-07 S4)

The offline half of `tools/server/server-decision.cpp`: request parsing, value
transforms, the KEV/H2O/D1-omni texts, the option/variant tables, the confidence
formulas, `format_answer` and `group_tasks`. Base commit `e6e4f469`.

The native file is 1618 lines. The model-dependent parts stay native and are S5:
`init` from the GGUF metadata (36), the image and audio loaders (338, 349), the
jinja render (984), `d1_labels` (854), the `fill_task` family (1056, 1120, 1177,
1308) and `fill_task_laya`, which is a prompt layout over a tokenizer.

## What this slice covers

`source/decision-core.goose` (prefix `dc_`) reads JSON with the accepted parser
(`goose/json/json-parser.goose`, copied here as `jsonparse.goose`), which keeps
the document key order like the native `nlohmann::ordered_json`.

| export | native anchor | what it returns |
|---|---|---|
| `dc_parse_questions` | `parse_questions` 220 | the normalised questions as text: `[{"id","type","instructions","options":[{"key","description"}]}]`, or the native error message |
| `dc_parse_state` | `parse_state` 359 | `files=<n> kinds=<i\|a>` and the state with the media parts of its messages taken out |
| `dc_replace_text` | `decision_replace_text` 420 | the value with every string replaced, Python layout |
| `dc_sort_keys` | `decision_sort_keys` 444 | the value with every object sorted by key |
| `dc_kev_render` / `dc_kev_text` | `decision_kev_render` 467, `decision_kev_text` 526 | the flattened text, optionally with the special tokens escaped |
| `dc_escape_tokens` | `escape_special_tokens` 499 | `<\|name\|>` becomes `<¦name¦>` (U+00A6) |
| `dc_d1omni_escape` | `decision_d1omni_escape` 531 | the value with the special tokens escaped in strings and keys |
| `dc_py_float_of` / `dc_py_str_of` / `dc_py_space` / `dc_py_dumps` | 556-722 | `repr(float)`, `str` escapes, `lstrip`/`rstrip`, `json.dumps` with the indent, separator and `ensure_ascii` rules |
| `dc_h2o_cut` / `dc_h2o_quote` / `dc_prepare_state` / `dc_render_h2o` | 725, 748, 774, 782 | the head/tail cut, the quote of the special tokens, the prepared state, the template input object |
| `dc_n_variants` / `dc_n_outputs` | 838, 846 | the prompt and output counts |
| `dc_confidence` | 1442-1494 | the choice, score and entropy formulas, and the noul error |
| `dc_format_answer` | `format_answer` 1496 | the answer object: type, choice/noul/score, probabilities, legend, confidence |
| `dc_group_tasks` | `server_decision_group_tasks` 1588 | one line per resulting task: `G <id> <shared> <children>` or `T <id>` |
| `dc_markers` | 827-836, 1303-1306 | the D1-omni and CLEF marker constants and the token budgets, as data |

Owned decisions:

- the question normalisation: score keys from the level index, the noul
  `false`/`true` order, the `no`/`yes` aliases, the true-first and
  criteria-order swaps, the option limit, the choice sort, and the difference
  between an absent h2o instruction (`""`) and an explicit null;
- the kev flattening: raw strings, `- ` array items with their leading space,
  tab, newline and carriage return stripped, `:\n` before a nested value;
- the Python layouts: `repr` of a float (exponent form below 1e-4 and from
  1e16, `.0` on an integral value), `str` escapes including surrogate pairs,
  `str.isspace`, and `dumps` with `","`/`":"` below indent 0 and `", "`/`": "`
  from 0 (the h2o description dumps use the compact pair with spaces);
- the answer: the softmax with the reversed second variant, the score bias, the
  LEV expected value, the media temperatures, the canonical legend, and the
  confidence formulas;
- `group_tasks`: the parent/child decision, the shared prefix, and the
  `n_slots` walk.

## Gate

```
bash tests/build.sh
```

`tests/fixture.cpp` carries a transcription of the native functions on
`nlohmann::ordered_json` (the library behind `common_json`), with the source
line cited at every function, and links the Goose module. Both sides evaluate
the same corpus in one process and compare byte for byte:

```
FG_DECISION cases=400 failures=0
```

The corpus: 12 fixed JSON documents (nesting, escapes, control bytes, 2-, 3-
and 4-byte UTF-8, floats on both sides of the Python thresholds, leading
whitespace in array items, special tokens) each through 10 operations; 12
floats; 6 raw strings through `str`, `lstrip` and `rstrip`; 8 special-token
texts; 4 cut cases including a cut inside a three-byte character; 3 quote cases;
35 question bodies (choice, score, noul, the aliases, the swaps, the limits, the
h2o and vllm defaults, every error message); 15 state bodies (messages, media
parts, aliases, the counters, every error); 6 prepared states; 6 h2o renders; 45
variant/output rows; 10 confidence rows; 14 answers (two variants, bias, media
temperature, canonical legend, NaN, wrong sizes); 8 group rows; the marker
constants; and 10 deterministic random documents (LCG, depth 2) through kev,
dumps, a pretty dump, sort, d1-omni escape and replace.

`tests/gate.log` is the recorded output of the last run (400 `OK` lines).

## Mutations

```
python3 tests/mutate.py 0 6     # 6/6 caught
python3 tests/mutate.py 6 12    # 6/6 caught
```

Twelve semantic mutations, two halves of six:

| # | mutation | caught by |
|---|---|---|
| 0 | a score question accepts 11 levels | `q_score_eleven` |
| 1 | noul options are no longer swapped | `q_noul_true_first` |
| 2 | the omni/h2o `no`/`yes` aliases are swapped | `q_noul_alias_loose`, `q_noul_alias_h2o` |
| 3 | an h2o question defaults to null instructions | `q_no_instr_h2o_loose` |
| 4 | the choice sort compares the wrong neighbour | `q_choice_sorted` |
| 5 | score level keys start at 1 | `q_score`, `q_two`, the seven `ans_*` rows |
| 6 | a kev array item keeps its leading tab | `kev_10`, `kevtext_10`, `kev_11` |
| 7 | the escape uses U+00A7 as its second break | `escape_0`, `escape_1`, `kevtext_9`, `d1omni_9` |
| 8 | the float exponent form starts from 1e17 | `py_float_1e+16`, `dump_compact_5` |
| 9 | an integral float prints `.1` | `py_float_0`, `py_float_-0`, `py_float_1e+15`, `conf_1` |
| 10 | the cut marker loses a dot | `cut_ascii`, `cut_utf8` |
| 11 | `>` is quoted in upper case | `quote_1`, `quote_2`, `h2o_4` |

`mutate.py` restores the source after every mutation and rebuilds once at the
end; a mutation that breaks the build counts as caught.

## Recorded limits

- `init` (the model type, the temperature table, the score bias, the label
  table, the special-token spellings) is S5. Here the type is an argument, the
  label texts are a `A,B,C` list, the spellings are newline separated and
  already longest first, the temperature and the bias are arguments, and
  `get_temperature` (1421) is not ported: the caller resolves the bucket.
- `h2o_cut_state` takes the token budget as an argument and splits on bytes,
  one token per byte. The head/tail composition, the `...` line and the U+FFFD
  at a cut inside a character are compared with `dump_safe`; the real token
  boundaries stay native with the tokenizer (D6), so this is a surrogate for
  the tokenizer branch, not for the cut rule.
- `parse_state` checks and counts the media, it does not decode the base64; the
  `data:` and audio prefix checks are the native ones, the content-type
  detection of `handle_media` and the 8-file vector stay in S5. It assumes
  `parse_questions` ran first, as in the server: a missing `state` is reported
  by the question parser, not by a library exception.
- `render_h2o` builds the template input, not the prompt: the jinja template
  (the `systemone` chat template) and the tokenizer are S5.
- `format_answer` takes the scores as the native `float`, so every score,
  temperature and bias is rounded to f32 before use; the probabilities and the
  answer fields are double, as natively.
- `group_tasks` models `server_task` as an id and a token list. A task with an
  empty token list is not in the corpus: the native `size() - 1` underflows
  there and the behaviour is not meaningful to reproduce.
- Fixed limits: 16 questions, 64 options, 1024 JSON nodes, 16384 bytes of JSON
  strings, 32768-byte scratch strings, 4096-byte quoted keys. Larger documents
  truncate; none of the corpus documents comes close.
- Duplicate JSON keys are not in the corpus: `ordered_json` keeps both and the
  choice sort is then not stable, while the port sorts stably.
- Integers above `i64` are dumped from the parser's low word only; none of the
  corpus documents has one.
- `canonical_legend` dumps the description with a compact nlohmann dumper
  (section "dumps" of this module). Strings below U+0020, floats, booleans and
  integers are covered; invalid UTF-8 inside a legend is not (the native
  `dump()` throws there).

## Files

| path | what |
|---|---|
| `source/decision-core.goose` | the port |
| `source/jsonparse.goose` | byte-identical copy of `goose/json/json-parser.goose` |
| `tests/fixture.cpp` | the native transcription and the corpus |
| `tests/build.sh` | builds the module at `-O0` and the fixture at `-O1`, runs the gate |
| `tests/mutate.py` | the twelve mutations, `mutate.py [lo hi]` |
| `tests/gate.log` | the recorded gate output |
| `REPORT.md` | result, table, hashes |

`build/` is generated and not part of the experiment.
