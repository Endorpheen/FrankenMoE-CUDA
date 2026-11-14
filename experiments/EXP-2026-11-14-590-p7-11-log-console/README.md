# EXP-2026-11-14-581 — log policy and console editing (P7-11 S4 + S5)

Native counterparts:

- `common/log.cpp` — level map, verbosity gate, stream choice, prefix,
  timestamps, colors, JSONL, reset placement;
- `common/debug.cpp` — the tensor filter and the NaN abort decision;
- `common/console.cpp` — the `readline_advanced` editing loop, the UTF-8
  primitives and the history state machine.

Base commit: `4dac50f2`.

Two modules in one experiment, two independent gate lines:

| module | native | gate line |
|---|---|---|
| `source/log-policy.goose` | `common/log.cpp`, `common/debug.cpp` | `FG_LOG` |
| `source/console-editing.goose` | `common/console.cpp` | `FG_CONSOLE` |

## What S4 covers

`source/log-policy.goose`:

| export | native | returns |
|---|---|---|
| `lp_level_str(level, out)` | `level_str` | `none/debug/info/warn/error/cont` |
| `lp_verbosity(level)` | `common_log_get_verbosity` | the `LOG_LEVEL_*` value |
| `lp_passes(level, thold)` | `common_log_default_callback` | verbosity gate |
| `lp_stream(level, jsonl, thold)` | `common_log_entry::print`, no file | `-1` dropped, `0` stdout, `1` stderr |
| `lp_colors(mode, tty_can, tty_ansi)` | `common_log_set_colors` + `set_colors` | the final boolean |
| `lp_render(level, prefix, ts, jsonl, is_json, colors, msg, out)` | `print(FILE*)` | the exact bytes |
| `lp_filter_match(blob, off, len, n, name)` | `common_debug_cb_eval` filter half | `0/1` |
| `lp_ne_string(ne, out)` | `common_ggml_ne_string` | `"ne0, ne1, ne2, ne3"` |
| `lp_dump_f32(ne, vals, n, abort, out, outn)` | `common_debug_print_tensor` F32 | `0` returned, `1` would exit |
| `lp_nan_abort(abort, is_nan)` | the `exit(0)` condition | `0/1` |

Rules reproduced from the native text:

- `common_log_get_verbosity`: DEBUG → 5, INFO/CONT → 4, WARN → 2, ERROR → 1,
  NONE → 0; the default callback accepts `verbosity <= threshold`.
- `print` with no file: a DEBUG entry is dropped while
  `threshold < LOG_DEFAULT_DEBUG` (5); an entry that is not NONE and not JSONL
  goes to stderr, everything else to stdout; JSONL always goes to stdout.
- The prefix is suppressed for NONE and CONT and for `prefix == false`; the
  timestamp `M.mm.mmm.uuu` is minutes, seconds, milliseconds, microseconds in
  blue, then default; the level letter is `I ` green + default, `W ` magenta,
  `E ` red, `D ` yellow.
- The reset is written before the trailing newlines and only for
  WARN/ERROR/DEBUG, so every line carries its own colors.
- JSONL: a serialized message is printed as-is; otherwise
  `{"type":"log","time":N,"level":"L","msg":"M"}` with a newline.
- Colors: AUTO (-1) asks `tty_can_use_colors`, DISABLED (0) and ENABLED (1) do
  not; the answer is ANDed with `tty_enable_ansi`.
- Debug filter: an empty set matches everything; otherwise the anchored
  `^pattern` search decides. The port matches a literal prefix.
- F32 dump: 4 decimal places, width 12, `n = 3` truncation on each axis with
  the native `..., ` markers, `sum = %f`, then the NaN abort.

## What S5 covers

`source/console-editing.goose`:

| export | native | returns |
|---|---|---|
| `ce_decode(text, len, pos, adv)` | `decode_utf8` | code point, advance |
| `ce_encode(cp, out, at)` | `append_utf8` | new offset |
| `ce_prev_pos` / `ce_next_pos` | same | character boundary |
| `ce_has_ctrl(params, len)` | `has_ctrl_modifier` | `0/1` |
| `ce_width(cp)` / `ce_is_space(cp)` | `estimateWidth`/`iswspace` | pinned value |
| `ce_edit(keys, multiline, line, llen, pos, out, olen, arena, used, hoff, hlen, hcount, view, backup, blen)` | `readline_advanced` | `has_more` |

The whole editing loop is owned: insertion at the end and in the middle,
backspace (0x08 and 0x7F), delete at cursor, home/end, left/right, ctrl-arrow
word moves, CSI `~` forms 1/7/3/4/8, unknown finals, `ESC ESC` discard,
history up/down with the backup line, the `\` and `/` continuation rule, the
`Ctrl-D` end-of-stream rule, and the bytes the native code writes
(`\b`, `ESC[C`, redraws, blanking, newlines).

State is explicit and passed in: the line buffer and length, a two-element
position array (character index, byte index), the output buffer and length,
and the history arena with per-entry offsets, count, view index and backup.

## Gate

`tests/gen_corpus.py` emits three native probes and two Goose drivers from one
table set:

```
tests/build.sh                      # -O1
FG_LOG cases=905 failures=0
FG_CONSOLE cases=291 failures=0
```

`FG_LOG` is 720 render cases (6 levels x prefix x 6 timestamps x JSONL x
colors x `is_json` x 10 message shapes), 60 stream-choice cases (5 thresholds
x 6 levels x JSONL, both process streams captured), 36 verbosity-gate cases,
6 level names, 12 color-enablement cases, 56 filter cases (8 pattern sets x 7
names through the real `common_debug_cb_eval`), 4 `ne_string` cases and 11
F32 dump cases including three NaN-abort cases run in child processes.

`FG_CONSOLE` is 33 decode steps, 11 encodings, 54 position checks, 12
ctrl-modifier checks, 6 width rows, 6 space rows and 169 key scripts (49 hand
written plus 120 deterministic mixed scripts from an LCG). Each script is one
`readline` call with history carried across calls, so later scripts observe the
entries earlier ones added.

The native side includes the real `.cpp` files; only tty probes, `ggml_*`
accessors, `ggml_abort` and the log API used by `debug.cpp` are stubbed.
`tests/cases.tsv` is the hand-readable subset with the exact native lines.

## Mutations

`tests/mutate.py <lo> <hi>` copies the sources into a temporary tree, applies
one semantic edit, regenerates the driver and runs the same gate at `-O0`
against the cached `-O1` native oracle.

```
python3 tests/mutate.py 0 6    # 6/6 caught, FG_LOG
python3 tests/mutate.py 6 12   # 6/6 caught, FG_CONSOLE
```

## Recorded limits

- No terminal: `wcwidth` is pinned (control → 0, otherwise 1) and the gate only
  uses ASCII, Cyrillic and two control bytes. Wide and combining code points
  stay native.
- The `\033[6n` cursor-position query in `put_codepoint` needs a real tty and
  is not graded; the `tty == nullptr` branch is.
- `termios` raw mode, `ioctl` window size, signal handling and the Windows
  console API are native and outside the gate.
- The completion callback is not installed, so a tab is a plain character with
  the native width clamp.
- `set_display` and the spinner are not graded; they are the only users of
  `common_log_flush` in `console.cpp`.
- `std::regex` is replaced by a literal prefix match, identical for patterns
  without metacharacters; metacharacters and invalid patterns stay native.
- `%12.4f` and `%f` are reproduced with round-half-away-from-zero on values
  exact at the printed precision. General float formatting stays native, and
  the sign of a NaN produced by `inf + (-inf)` (x86 `-nan`) is not ported.
- Only the F32 tensor dump is ported; F16/BF16/I8/I16/I32/I64 decoding stays
  native.
- JSON escaping covers quote, backslash, the short control escapes and
  `\u00XX`; the `error_handler_t::replace` behaviour on invalid UTF-8 is
  native.
- The log writer thread, the ring queue, `FILE*` management and varargs
  formatting stay native. The color cases do start the singleton writer thread
  through `common_log_init`, but the graded print path never uses it.
- None of the three native files reads an environment variable; the on/off
  rules are the `common_log_set_*` flags, the verbosity threshold and the color
  mode enum, and those are covered.
