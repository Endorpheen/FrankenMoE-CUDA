# EXP421: FrankenMoE terminal logging -- classic, franken and JSON

The server keeps every existing technical line but gives it a FrankenMoE
identity. The event classifier and the three formatters are Goose; the C++
logger only forwards the line, level, timestamp and previous event.

## Architecture

| layer | owner |
|---|---|
| event classification, banner choice, level tag, JSON escaping and output bytes | Goose (`source/logfmt.goose`) |
| byte load/store, i64 store | three C primitives (`integration/logfmt_bridge.c`) |
| existing log queue, file/stdout/stderr routing, verbosity, prefixes | unchanged `common/log.cpp` |
| mode selection | `FRANKENMOE_LOG_MODE=classic|franken|json`, and the existing `--log-jsonl` maps to JSON |

Modes:

- `classic` -- the original line, no decorations.
- `franken` -- one banner per real event, `[INFO]/[WARN]/[ERROR]/[DEBUG]` tag,
  the original text unchanged.
- `json` -- one object per line on **stdout**:
  `{"time":"...","level":"...","event":"...","msg":"..."}`.

The compiler witness lines (`FG...`) are written by the generated modules to
stderr, so a JSON consumer reading stdout gets only JSON. That was verified by
separating the streams.

## Goose formatter

`logfmt.goose` classifies by level and by byte search over the line:
server start, CUDA/GPU, model loading, model loaded, cache/session, ready,
performance, error, warning, shutdown, HTTP, MoE. It writes the selected banner
once, then the level tag and the original text. JSON mode escapes quotes,
backslashes and control characters and strips the trailing newline from `msg`.

The fixture is 27 checks / 0 failures: classic byte copy, banner-once,
error/warning tags, an 11-case event matrix, JSON shape and escaping, and
truncation safety with a small output buffer.

## Server result

One Apex profile, context 512, one short prompt with `ignore_eos` for a
128-token decode. The same staged `libllama-common.so` was used for all modes.

| mode | content sha | JSON objects | JSON bad lines |
|---|---|---:|---:|
| classic | `d83d9078...` | - | - |
| franken | `d83d9078...` | - | - |
| json | `d83d9078...` | 41 | 0 |

All three returned the same tokens and content. JSON events observed:
`server_start`, `log`, `warning`, `model_loading`, `memory_cache`,
`model_loaded`, `ready`, `performance`, `shutdown`. Banners in franken mode:
ALIVE 1, UNLEASH 1, CACHE ARMED 1, BEAST LOADED 1, ALL SYSTEMS BURNING 1,
FULL THROTTLE 1, POWERING DOWN 1, no repeated warning banner.

Sample franken output:

    🔥 FRANKENMOE IS ALIVE!
    [INFO] srv  llama_server: initializing ...
    ⚡ UNLEASH THE MONSTER!
    [INFO] srv    load_model: loading model '.../Apex-35B-V1.0-Q4_K_M.gguf'
    🧠 CACHE ARMED!
    [INFO] srv    load_model: initializing, n_slots = 1, n_ctx_slot = 512
    🔥 THE BEAST IS LOADED!
    [INFO] srv  llama_server: model loaded
    🔥 ALL SYSTEMS BURNING!
    [INFO] srv  llama_server: listening on http://127.0.0.1:18311
    🚀 FULL THROTTLE!
    [INFO] slot print_timing: id  0 | task 0 | eval time = ... (31.68 tokens per second)
    🛑 FRANKENMOE POWERING DOWN.

## Performance

Paired A/B/B/A on the same staged library and model, 128 decode tokens:
`classic -> franken -> franken -> classic`.

| mode | decode tok/s runs | mean |
|---|---|---:|
| classic | 30.918, 28.691 | 29.804 |
| franken | 30.635, 30.668 | 30.652 |

The franken mean is 2.85 percent above classic, i.e. the difference is
warm-up/noise in franken's favour, not a regression. Prefill on this short
prompt is noisy in both modes. The formatter runs once per log line at startup
and teardown, never in a decode loop, so it cannot enter the hot path; this
measurement is consistent with that.

## Limitations

- The staged library is built from the flint `build-cuda` tree, not from the
  accepted Goose runtime. Promotion to the accepted runtime needs a rebuild of
  its `libllama-common` with the same three files.
- The mode is selected by `FRANKENMOE_LOG_MODE`; a `--log-mode` CLI flag is not
  added yet (the existing `--log-jsonl` still selects JSON).
- Event classification is a byte-search heuristic over the existing text. It
  does not invent fields; `msg` is always the original line.
- Compiler witness lines are outside the logger and stay on stderr.

## Reproduce

    bash experiments/EXP-2026-11-12-421-frankenmoe-logging/build_stage.sh
    runtime/exp421/build/logfmt_fixture
    python3 experiments/EXP-2026-11-12-421-frankenmoe-logging/run_modes.py
    python3 experiments/EXP-2026-11-12-421-frankenmoe-logging/run_pair.py classic franken franken classic
