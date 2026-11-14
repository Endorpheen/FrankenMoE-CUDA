# P8-07 S1 — request schema fields (`tools/server/server-schema.cpp`)

Base: repository commit `5ab757e0`. Native tree
`/home/end0/AI/LlamaServer/llama.cpp-flint-cuda` at `1b44e307` (clean).

Hypothesis: every decision in `server-schema.cpp` that does not need a
`llama_vocab` and does not start the server is expressible as data plus small
pure functions in Goose, and can be gated offline against the native
translation unit with line-for-line agreement. Expected outcome: identical
output, 12/12 mutations caught.

Nothing here is committed, built into the server or measured for speed. The
gate is one run of one binary and one run of the Goose driver.

## What the slice owns

`source/server-schema.goose` (1436 lines) is the port.  Everything below
`sc_`/`SC_`/`TTS_` is private; the `schema_*` exports at the end of the file are
the integration surface (see Public API).

| port part | native counterpart | notes |
|---|---|---|
| `S_NAMES`, `S_DESC`, `S_KIND`, `S_TYPE`, `S_ILIM`, `S_FLIM`, `S_HARD`, `S_HAND` | `make_llama_cmpl_schema` | 62 live top-level fields in native order, plus the `stream_options/include_usage` subfield as row 62; aliases in `add_alias()` order; doc strings carried verbatim |
| `sc_base_std`, `sc_base_set_*`, `sc_base_cfg` | `common_params` defaults | sampling as a whole, `n_keep`, `n_predict`, `n_cache_reuse`, `cache_prompt`, `antiprompt`, `sse_ping_interval`, `reasoning_format`, `verbosity`, `n_parallel` |
| `sc_defaults` | the body of `eval_llama_cmpl_schema` | `task_params` defaults, then the values it copies from the base, then `verbose = verbosity > 9` |
| `sc_apply_num` | `field_num<T>::eval` | clamp for soft limits, throw for hard limits, `i32`/`i64`/`u32`/`f32`, `dry_base` handler |
| `sc_apply_bool` | `field_bool::eval` | type check, `ignore_eos` EOG insertion |
| `sc_apply_handler` | `field_str`/`field_json`/`field_nested::eval` | response_fields, lora id set, dry_sequence_breakers, json_schema/grammar selection, reasoning_format names, continue_final_message, echo, stop with the CLI fallback, samplers by name or char, nested object rule |
| `sc_alias_pos` + the `KEYN` table | `has_value` loop in `field::eval` | primary name wins over aliases; later aliases are never evaluated |
| `sc_range_*`, `sc_type_err`, `sc_msg_*` | `handle_with_catch` texts | error strings are part of the decision |
| `T_*`, `sc_tts_*` | `make_tts_schema`, `eval_tts_schema` | 6 rows, `repeat_penalty` 1.05, `n_predict -1`, `top_k 0`, `top_p 0`, `seed` from the base |
| `sc_tline`, `sc_vline`, `sc_tts_*line` | — | the gate's text form only; no server use |

## Explicitly not owned

1. **JSON parsing and nlohmann conversions.** The gate driver tags a value
   (`kind`, int, float, text) and the port decides what the schema does with
   it. The driver stands in for the JSON layer exactly as `data.at(...)` does.
2. **`json_schema_to_grammar`.** D1 owns the conversion. The gate compares the
   grammar *type* (`NONE`/`USER`/`OUTPUT_FORMAT`/`TOOL_CALLS`) and the selection
   rule, not the grammar text.
3. **Token-level handlers** (`preserved_tokens`, `grammar_triggers`,
   `reasoning_budget_start_tag`, `reasoning_budget_end_tags`,
   `reasoning_budget_message`, `logit_bias`). All six call `GGML_ASSERT(vocab)`
   before any branch, so they abort without a vocab. Their rows, aliases, limits
   and defaults are ported; the handlers are not run. D6 supplies the vocab.
4. **`parse_lora_request` scale parsing** (server-common). The driver supplies
   per-entry ids; the port owns the array check and the id-set size.

## Gate

One generator (`tests/gen_schema.py`) emits the native probe and the Goose
driver from the same case table, so there is no second copy of the requests.

- Native: `tests/probe_schema.cpp` includes `tools/server/server-schema.cpp`
  and calls `make_llama_cmpl_schema`, `eval_llama_cmpl_schema`,
  `make_tts_schema` and `eval_tts_schema`. Compiled `-O1 -ffunction-sections
  -fdata-sections`, linked `-Wl,--gc-sections` against `libllama-common`,
  `libllama`, `libmtmd`, `libggml` only. `libllama-server-impl.so` is *not*
  linked: it carries an unrelated experiment's static initialiser that prints
  on load. No sockets, no server, no model, no vocab.
- Goose: `build/gate_schema.goose` = the port plus the generated driver, run
  with `goose run`.  The driver calls the public `schema_*` wrappers, not the
  private `sc_*` bodies, so the gate checks the entry points a runtime uses;
  it only uses `sc_puts`/`sc_copy` to build the input buffers and the error
  text line.

```
docs/experiments/…/tests$ bash tests/build.sh
gate: 181 native lines, 181 Goose lines, diff = 0
```

181 lines = 63 completion table rows + 6 TTS table rows + 101 completion
request cases (`C0..C100`) + 11 TTS request cases (`D0..D10`). The native side
prints 78 `ok` and 23 `err` completion cases, 9 `ok` and 2 `err` TTS cases;
the errors are 12 hard-limit range messages, 7 JSON type errors and 6
handler-owned messages, all compared as text.

Coverage: defaults and base copying (empty bodies, nulls, variant base),
soft clamps (`top_p`, `temperature`, `mirostat`, `adaptive_target`), hard limits
(`n_predict`, `n_cmpl` with two `n_parallel` values, `n_indent`, `n_keep`,
`repeat_last_n`, `min_keep`, `n_discard`, `dry_allowed_length`,
`adaptive_decay`), `i32`/`i64`/`u32` conversions (`seed -1` wraps,
`4294967296` wraps to 0, `1.5` truncates), type errors, `response_fields` with
arrays, non-string arrays and wrong types, lora empty/one/duplicate-id/non-array,
breakers, `stop` (array, empty array, string, empty string, empty element,
number, object), samplers (names, kebab/case aliases, unknown names, chars,
unknown chars, non-array), `json_schema`/`grammar` selection (both present, tool
calls, empty grammar, null), every `reasoning_format` name and the unknown one,
`continue_final_message`, `echo`, nested object/null/array/string, alias order
(`n_predict` vs `max_tokens` vs `max_completion_tokens`, `n`, `logprobs`),
booleans, and the TTS baseline with its own alias and type error.

## Mutations

`tests/mutate.py <lo> <hi>` edits the port, regenerates the driver, runs
it and compares with the recorded oracle (`build/native_schema.out`).

```
python3 tests/mutate.py 0 6    # 6/6 caught
python3 tests/mutate.py 6 12   # 6/6 caught
```

Every entry names the defect, not the line; the report records the measured
diff for each.

## Recorded limits and approximations

1. **Field count.** The inventory says 76 fields; the translation unit has 62
   live top-level `add((new field…` calls, one nested subfield and six aliases.
   The difference is 7 `#if 0` speculative-decoding adds and one commented-out
   `t_max_prompt_ms` line. The gate proves the 62+1 structure, not 76.
2. **`n_cmpl` limit.** The upper bound is `params_base.n_parallel`, so the
   table dump uses the default base (`hi=1`); cases use both 1 and 4.
3. **Float sentinels.** `numeric_limits<float>::lowest()/max()` and `+inf` are
   represented as `-inf/+inf` (including `adaptive_target`'s `-FLT_MAX`). No f32
   request value lies outside those bounds, so clamping is identical. The
   shared renderer prints `-inf`, `inf`, `-fmax`, `fmax` and six fixed decimals,
   so a table dump cannot distinguish `-FLT_MAX` from `-inf`.
4. **nlohmann quirks pinned deliberately**, because they are observable API
   behaviour: `get<int32_t>` from a float truncates, `get<uint32_t>` wraps
   negative integers, `get<bool>` from a number is a type error, and `empty()`
   is **false for every string**, so `stop: ["", "a"]` keeps both entries. The
   type-error wording (`[json.exception.type_error.302] …`) is pinned to this
   tree's library; a library upgrade may change it and the gate will say so.
5. **`json_value` fallback.** A wrong type for `response_fields` or
   `dry_sequence_breakers` logs a warning and returns the default, it does not
   throw; `dry_sequence_breakers` then fails its own non-empty check. The port
   reproduces the result, not the warning (stderr is not compared).
6. **`lora` entries.** `parse_lora_request` is a ten-line transcription in the
   probe because the real one lives in server-common; only the id-set size is
   compared, and only for `[]`, one id and two identical ids.
7. **Gate form.** The table and value dumps are the gate's own format, not the
   server's JSON. `sc_tline`/`sc_vline` exist for the gate and are not integration
   API.
8. **No server, no model, no network, no timing.** This says nothing about
   throughput and is not a performance claim.

## Public API

`source/server-schema.goose` compiles with `goose --standalone --header`; the
23 `export fn` wrappers are one-line forwards to the private bodies, so the
decisions stay in one place.  The gate driver calls these wrappers.

| export | forward | purpose |
|---|---|---|
| `schema_find(name)` | `sc_find` | row of a request key, aliases included, `-1` when absent |
| `schema_tfind(name)` | `sc_tfind` | TTS row of a request key |
| `schema_reset(I, F, ANT)` | `sc_reset` | zero the task state |
| `schema_base_std(BI, BF, B, BANT)` | `sc_base_std` | `common_params` defaults for the base |
| `schema_base_set_i(BI, BF, row, v)` | `sc_base_set_i` | base integer override by row |
| `schema_base_set_f(BI, BF, row, v)` | `sc_base_set_f` | base float override by row |
| `schema_base_cfg(B, verbosity, np)` | `sc_base_cfg` | base `verbosity` and `n_parallel` |
| `schema_base_set_ant(B, BANT, txt)` | `sc_base_set_ant` | base stop list (`antiprompt`) |
| `schema_defaults(I, F, BI, BF, B, ANT, BANT, SAM, KEYN)` | `sc_defaults` | `task_params` defaults + base copy |
| `schema_apply(..., row, key, k, iv, fv, txt)` | `sc_apply` | apply/validate one completion request entry |
| `schema_vline(row, I, F, SAM, ANT, out, at)` | `sc_vline` | text dump of one completion row |
| `schema_tline(row, np, out, at)` | `sc_tline` | table line of one completion row |
| `schema_tts_defaults(I, F, BI)` | `sc_tts_defaults` | TTS baseline |
| `schema_tts_apply(I, F, MSG, row, k, iv, fv)` | `sc_tts_apply` | apply/validate one TTS request entry |
| `schema_tts_vline(row, I, F, out, at)` | `sc_tts_vline` | text dump of one TTS row |
| `schema_tts_tline(row, out, at)` | `sc_tts_tline` | table line of one TTS row |
| `schema_row_count()` | constant | 63 completion rows (62 fields + `include_usage`) |
| `schema_tts_row_count()` | constant | 6 TTS rows |
| `schema_rc_ok` / `schema_rc_range` / `schema_rc_type` / `schema_rc_message` | constants | status codes of `schema_apply` |
| `schema_msg_len(I)` | `I[79]` | length of the message written into `MSG` |

`schema_apply` returns 0 (applied), 1 (hard-limit range error), 2 (JSON type
error), 3 (handler message); on a non-zero code the text is in `MSG` and its
length is `schema_msg_len(I)`.  `I` is 80 `i64` slots, `F` 80 `f32` slots,
`SAM` 16 sampler codes, `KEYN` 63 alias-priority slots, `ANT`/`MSG` byte
buffers; the row layout is the table order documented above.

## Layout

```
source/server-schema.goose    port (1436 lines)
tests/gen_schema.py           one table -> probe + driver + cases_schema.tsv
tests/probe_schema.cpp        native oracle (generated)
tests/mutate.py        12 mutations, two halves
tests/build.sh                gate build and diff
tests/cases_schema.tsv        human-readable request table
build/                        gate outputs (native/goose/diff)
```

Reproduce from this directory:

```
bash tests/build.sh
python3 tests/mutate.py 0 6
python3 tests/mutate.py 6 12
goose --standalone -o build/schema_api.c --header build/schema_api.h \
      source/server-schema.goose
```

`FG_LLAMA`, `FG_GOOSE`, `FG_CXX`, `FG_OUT` override the paths. The scripts
derive the experiment root from their own location and use no absolute
temporary paths.
