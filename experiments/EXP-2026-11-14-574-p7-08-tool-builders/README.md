# EXP-2026-11-14-574 — standard tool builders (P7-08 slice 14)

Native counterpart: the `standard_json_tools` and `standard_constructed_tools`
families in `common/chat-peg-parser.cpp`, plus the grammar arena dump they are
compared through. Base commit: `b99d952f`; the block was rebased from the
`sub-566` port onto canon `73c51a5e`.

## What this slice covers

The block adds 1256 lines to the canonical runtime, in one insertion point with
no canon line removed: the JSON and constructed tool builders, their private
helpers, and the dump branches they use. It covers parallel on/off,
`function_is_key`, nested dot keys, per-call wrappers, call-id PRE/POST,
default and custom start markers, the single-quote and double-quote argument
names, the parameter prefix/suffix pair, the array wrapper, the OpenAI wrapper
switch, `force` handling and the flat-field ordering in the dump.

The rebase needed no constant edits: canon at `73c51a5e` already carried
`PEG_SPACE/CHARS/STRING/SCHEMA/AC` and all six `peg_dump_node` branches, and
the block refers to the constants by name. The old numeric assumptions lived
only in the gate fixtures, which were adapted to the canon kind map.

## Gates

`tests/build.sh` builds the canonical runtime with `--standalone` at `-O0` and
links the two fixtures against `libllama-common`; no weights and no model.

Structural gate — native `standard_*` builders and the port build the same
grammar, compared through the textual arena dump, plus a rebuild identity
check:

```
cases=13 failures=0      # CT-1..5, J-1..5, T-1..3; node counts and dump sizes
                         # identical to the sub-566 run on canon 54751184
```

Parse gate — the native chat parser driven by the built grammar:

```
parse_cases=9 failures=0 # P1..P9: JSON/XML single, two calls, args-first,
                         # partial, trailing junk, malformed exponent
```

## Mutations

`tests/mutate.py <lo> <hi>` rewrites the port, regenerates the C at `-O0`,
rebuilds and reruns the structural gate; `tests/parse_mutate.py` does the same
for the parse gate.

```
python3 tests/mutate.py 0 7    # 7/7 caught
python3 tests/mutate.py 7 15   # 8/8 caught
python3 tests/parse_mutate.py  # 5/5 caught (gate is not vacuous)
```

The recorded runs are `tests/mutations_h1_7.json`,
`tests/mutations_h8_15.json` and `tests/parse_mutations.json`.

## Exports

Canon `73c51a5e`: 81 `export fn` + 1 recursive. After the block: 83 + 1, the
two new names are `chat_standard_constructed_tools` and
`chat_standard_json_tools`; no export is lost and no name is duplicated.

## Recorded limits

- Hoisted rule names still use a local pid and do not match native `to_gbnf`
  (0/4); a real native insertion index is a separate slice.
- The gates run at `-O0` for the generated C (the 11.3 MB source makes `-O1`
  a separate budget), while the fixtures compile at `-O1`.
- The block is behaviour-preserving on this corpus; it does not add new native
  builders.
