# P10-07: the JSON adapter - what exists and what the swap needs

Checked 2026-11-19 on `main`, text level, no build.

## The two sides

| side | state |
|---|---|
| Goose parser | `goose/json/json-parser.goose`, **one** exported door `json_parse` (eighteen parameters: text, eight value arrays, four edge arrays, the string buffer, three child arrays and the state) |
| Goose dumper | `goose/json/json-dump.goose`, **one** exported door `json_dump` |
| both units | composed, and both round-trip the canonical text - pinned by `tests/json/roundtrip` (5 cases) and `tests/json/door` (5 valid, 4 invalid) |
| consumer | `vendor/flint/common/json.cpp`, 433 lines, **27 uses of `nlohmann::ordered_json`**, with the pimpl tree declared in `common/json.h` |

Neither door is called from `vendor/flint`, `runtime/` or `integration/`: the only callers today
are the two permanent tests, which reach the doors through the shipped library. So the production
consumer still parses and dumps with nlohmann.

## Why an adapter, and not a swap

The Goose parser answers in parallel arrays (`key`, `value`, `string offset`, `length`, `float`,
`number`, `header`, edge arrays, child arrays). `common_json` presents a pimpl tree of
`common_json_item` with a tagged `common_json_value`. A swap would have to reproduce every accessor
the tree offers; an adapter can build the tree from the arrays and leave the accessors alone, which
is the smaller and testable change.

## What the adapter has to reproduce

1. **Key order** - the arrays are in document order, and `ordered_json` is ordered; an adapter that
   goes through an unordered map loses a promise `common_json` makes to its callers.
2. **Number normalisation** - `-1.5e3` prints as `-1500.0`; `tests/json/roundtrip` pins that, and it
   is nlohmann's behaviour, not the input's.
3. **Both directions** - building the tree from the arrays, and filling the arrays from the tree for
   the dumper, so the same module serves readers and writers.
4. **Errors** - `json_parse` returns a code; `common_json` throws `common_json_error` with a
   message. The adapter owns that translation, and it must not lose the position if the parser
   reports one.

## The shadow comparison the requirement asks for

`vendor/flint/common/json.cpp:205` is where the tree is built today. The shadow form is: parse with
nlohmann as now, parse with the Goose door on the same bytes, build the tree from the arrays, and
compare the tree - key order included - and then compare the dump bytes. That is a test, not a
production branch, and it needs both implementations in one binary; nlohmann is header-only and
already vendored, so nothing new is linked.

## What is not claimed

That the adapter exists, that any of this is swapped, or that the comparison has run. Both doors are
composed and exercised by tests, and the production path is unchanged.

## The library these tests were measuring was not the released one

Checked 2026-11-19 while retargeting the tests at the accepted artefact:

| library | sha256 | `json_parse` | mtime |
|---|---|---|---|
| `builds/checkpoint-20261119-EXP1476/libllama.so.0` (the release) | `0ffd98c2` | **absent** | 19:00 |
| `builds/flint-acc/bin/libllama.so.0` | `bea5d6af` | present | **19:18** |

So every JSON test that passed today passed against the working directory, which is newer than the
checkpoint, was written by no script under `tools/`, and has no recorded provenance. The released
artefact has no JSON doors at all - the same "source ahead of the binary" pattern as the model-manager
and api-key doors, now measured on the release itself.

The tests now take their library from `tests/lib-default.sh` (accepted set first) and exit 77 when the
door is absent, which is what they do against the release: five of them skip, and none of them passes
against an artefact nobody accepted.

## What the last step needs, exactly

Established 2026-11-19 by following the call path rather than the plan:

- `vendor/flint/common/json.cpp:247` `fg_json_goose_parse` resolves its door with `fg_json_parse_door()`,
  which is a `dlsym` for **`json_parse`** in the loaded library. With no door it returns false, and in
  strict mode that becomes the "declined" the swap test prints.
- The door is exported by `goose/json/json-parser.goose:977`, whose only extern is
  `gs_json_strtod` - the host primitive that `integration/goose-standalone/json-strtod-provider.c`
  provides.
- The adapter half (`fg_json_adapter::to_tree`) is C++ and lives in `libllama-common`; it is already
  linked in path A's build.

So a build carries the JSON path only when it has all three: the adapter (yes), the Goose module
`json/json-parser.goose`, and a bridge that provides `gs_json_strtod` plus the module's init entry.
`build.py` looks for a bridge under `integration/<module>/source/bridge.c` named after the module's
`_enabled` symbol; `json-strtod-provider.c` provides the primitive but no init, so it cannot serve as
that bridge on its own.

The concrete step, for whoever does it with a build to verify:

1. add a bridge next to `json-strtod-provider.c` that includes the generated host and exports
   `fg<NNN>_goose_init` (the same shape as `fg-unicode-gpt2-bridge.c`);
2. add the pair to `_GOOSE_PAIRS` in `runtime/reconstructed/build.py`:
   `('goose/json/json-parser.goose', 'integration/.../fg-json-bridge.c', 'fg-json', 'fg_json_goose_init')`;
3. rebuild path A and run `bash tests/json/swap/run.sh <build>/run` - mode 2 must stop saying
   "declined" and mode 3 must still refuse without the door;
4. only then set `FG_JSON_GOOSE_PARSE` as the default, and record the run that proved it.

Not done here on purpose: a new C bridge is debt under rule 17, and creating one without a build to
verify it would be exactly the unverified claim this migration keeps paying for.
