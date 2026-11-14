# FG572 — nine parser families, final fixes

Base commit: `5ab757e0`. Starting point: `runtime.goose` from
`/tmp/opencode/parsers-accept` (canon + `d_*` parser block, 19022 lines).
Working copy: `/tmp/opencode/sub-590/exp`.

Result: `FG572_PARSERS cases=18 values=99 failures=0`, mutations 12/12
(6/6 in each half). The fixture, the expected values and the build script are
byte-identical to the input.

## 1. Root causes

### 1.1 End-of-message tokens are seven bytes, not six (three defects)

`<|eom|>` and `<|eot|>` are 7 bytes (`<`,`|`,`e`,`o`,`m`,`|`,`>`). The Muse
port advanced past a whole message with `+ 6`, so the remaining stream began
with the last byte of the token, `>`, instead of the next message marker.
The message loop then stopped, because both its entry conditions are exact
string tests: `d_prefix(cur, " to=")` at entry and
`d_prefix(cur, "<|start|>assistant")` at the tail of every iteration.

The native grammar has no such arithmetic: the end token is a literal inside
the rule that consumes the message, and the next message restarts at the
`start` rule (`common/parsers/muse-glimmer.cpp:52,59,66-67,75`,
`common/parsers/muse-spark.cpp:54,61,69-70,75-76`). The port's loop is the
only place that consumes the token, so the token length must be exact there.

Consequences observed:

| case | site (line before the fix) | effect |
|---|---|---|
| `muse-glimmer-reasoning` | analysis tail, `rest[end + 6 ..]` (18162) | leftover `>` made the `<|start|>assistant` strip miss, so `to=user` was never parsed; content empty, reasoning correct |
| `muse-spark-commentary` | commentary tail, `cur = body[e + 6 ..]` (18199) | the `to=user` message with `Done!` was dropped |
| `muse-spark-tool` | commentary tail (18199) and tool tail (18242) | the ATEM turn was never parsed; `ncalls` 0 |

Two more sites had the same defect and were fixed for the same reason: the
second `to=self` block (18188) and the `to=user` final message (18209). They
are not exercised by the failing cases (a trailing `>` after the final content
is harmless), but they would drop any message that follows them.

### 1.2 The K2 XML branch never opened its argument object

Native `common/parsers/k2-horizon.cpp:152-158` builds one JSON object per
call: `p.permute(...)` becomes the `{...}` body and
`p.tool(... << p.tool_args(args) << p.tool_close(...))` wraps it. The port's
`call_format == 2` (json) branch compacted a complete JSON object and was
correct, but the XML branch wrote parameters into the argument buffer without
the opening `{`, and sliced the call from the first parameter instead of from
the start of the object. The tool JSON therefore contained
`"arg1":1}` instead of `{"arg1":1}` — valid text, invalid JSON object.

## 2. Changes

`runtime.goose` only (six hunks, listed by `diff -u` against the input):

1. `d_muse` analysis tail: `rest[end + 6 ..]` → `rest[end + 7 ..]` (18162).
2. `d_muse` second `to=self` block: `body[e + 6 ..]` → `body[e + 7 ..]` (18188).
3. `d_muse` commentary tail: `body[e + 6 ..]` → `body[e + 7 ..]` (18199).
4. `d_muse` `to=user` tail: `body[stop + 6 ..]` → `body[stop + 7 ..]` (18209).
5. `d_muse` ATEM call tail: `body[stop + 6 ..]` → `body[stop + 7 ..]` (18242).
6. `d_k2_horizon` XML branch (18608-18623): keep `call_start = q`, write the
   opening brace with `d_params_begin(abuf, call_start)`, pass the position
   after the brace to `d_parse_k2_params`, and set `aoff`/`alen` from
   `call_start`, exactly as the Muse and MiniCPM families already do.

No other file changed: `fixture.cpp`, `cases.md` and `build.sh` are byte
equal to the input (checked with `diff -q`). The new file is
`tests/mutate.py`.

## 3. Gate

```
$ ./build.sh
[fg572] goose --standalone
parsed ok: 855 top-level declarations, 3 file(s)
typechecked ok: 868 specialization(s), ...
wrote build/runtime-gen.c (11560190 bytes)
[fg572] gcc -O0 runtime-gen.c
[fg572] g++ fixture.cpp
[fg572] link
[fg572] run
FG572_PARSERS cases=18 values=99 failures=0
```

Exit status 0. The gate compares the native session (`common_chat_session`
from `libllama-common`) against the port for 18 raw completions and 99 values
(role, content, reasoning, call count, name, arguments, id).

## 4. Mutations

`tests/mutate.py` applies twelve text mutations of the parser block, one
build per mutation, sequentially. A mutation is caught when the gate reports
`failures > 0` (or rejects the port JSON). Build failures are reported
separately and are not counted as catches; none occurred.

| # | half | mutation | caught by | gate |
|---|---|---|---|---|
| 1 | 1 | muse analysis skip is one byte short (`+7` → `+6`) | `muse-glimmer-reasoning` | failures=1 |
| 2 | 1 | muse commentary tail skip is one byte short | `muse-spark-commentary` | failures=4 |
| 3 | 1 | muse start-marker strip swallows the space (`18` → `19`) | spark tool/commentary | failures=4 |
| 4 | 1 | muse final recipient matches `assistant` instead of `user` | `muse-glimmer-content` | failures=3 |
| 5 | 1 | ATEM parameter open tag `name=` → `id=` | `muse-glimmer-tool` | failures=2 |
| 6 | 1 | muse typed argument (`D_JSON`) is escaped as a string | `muse-glimmer-tool` | failures=4 |
| 7 | 2 | k2 xml call loses its opening brace | `k2-xml-tool` | failures=1 |
| 8 | 2 | k2 xml arguments are sliced past the opening brace | `k2-xml-tool` | failures=1 |
| 9 | 2 | k2 arg key open tag typo (`<ifm|arg_key>` → `<ifm|arg_keys>`) | `k2-xml-tool` | failures=1 |
| 10 | 2 | k2 call name is split on CR instead of LF | `k2-xml-tool` | failures=2 |
| 11 | 2 | message JSON renames `reasoning_content` to `reasoning` | reasoning cases | failures=4 |
| 12 | 2 | parameter separator fires on the first parameter | tool cases | failures=6 |

```
half 1 (muse/ATEM): 6/6 caught
half 2 (k2/shared): 6/6 caught
mutations 12/12 caught; halves 6/6 and 6/6
```

One candidate mutation was rejected as equivalent: writing the tool
`arguments` value without JSON escaping produces a JSON object instead of a
string, and `common_chat_msgs_parse_oaicompat` converts it back to the same
text, so the gate cannot see the difference. It is recorded here rather than
counted.

## 5. SHA-256

```
58634e3171fe3d65e05f4e7c2ab7c1709d0a91ac42c838f7231c0d7441bab564  runtime.goose
1cb87f00bf496a4093b275cbd0381975ae22f62f93fe0154a2993494d746a428  fixture.cpp
af29f7345773cdc82fe03fde710ceb7998eeb458a252b78451f4c27339453389  cases.md
d6db3415ec070ab4122278309e67b921d7a55c633a9b310d08b53d5e6c3c3ff8  build.sh
245d10c405836b22ee5563d1f234130b3807d90eb5baed362696ad595c93eb96  tests/mutate.py
```

Compiler and toolchain: `goose-2026.3-string`, `gcc-13 -O0` for the emitted
runtime (correctness only, not a performance build), `g++-13 -O1` for the
fixture, linked against `build-cuda/bin/libllama-common` in
`/home/end0/AI/LlamaServer/llama.cpp-flint-cuda`.

## 6. Limitations

- The gate covers 18 raw completions and 99 values. Only the paths those
  cases exercise are proven; other grammar branches of the nine families
  remain unverified by this fixture.
- Muse: a second `to=self` block is still not concatenated into the reasoning
  (the branch keeps the first block only, `reas.len == 0`). Native
  concatenation is documented in the comment there; no case covers it, so it
  was left unchanged rather than changed unmeasured.
- Muse: one ATEM invoke per message is parsed; the native grammar allows a
  sequence of calls inside `<atem:function_calls>`.
- K2: parallel calls loop over `<ifm|tool_call>` entries with a fixed
  `D_MAX_CALLS` bound; the last call may trail `</ifm|tool_calls>` text into
  no-man's land only when `parallel != 0`, which no case exercises.
- All argument buffers are fixed size (16384 bytes for Muse and K2); a call
  whose arguments exceed that would silently truncate. Native is dynamic.
- Mutations are text substitutions of the parser block; they prove the gate
  notices the listed defects, not that the port is mutation-complete.
- The gate is a correctness run at `-O0`; it says nothing about the cost of
  the port relative to the native parser.

## 7. Reproduction

```
mkdir -p /tmp/opencode/sub-590
cp -r /tmp/opencode/parsers-accept /tmp/opencode/sub-590/exp
cd /tmp/opencode/sub-590/exp
# apply the six hunks of section 2 to runtime.goose, then
./build.sh                              # expect failures=0
python3 tests/mutate.py                 # expect 6/6 and 6/6
```
