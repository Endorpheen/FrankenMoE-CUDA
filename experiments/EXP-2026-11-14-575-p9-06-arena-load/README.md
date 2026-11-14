# EXP-2026-11-14-575 — serialized arena load and build_parse_params (P9-06 S4)

Native counterpart: `common_peg_arena::load` and `from_json` in
`common/peg-parser.cpp`, and `build_parse_params` in
`runtime/core/src/engine/chat_parse.cpp:8-21`. Base commit: `17b3c2c5`.

## What this slice covers

`source/parse_params.goose` adds the inverse of the native JSON serialization of
a PEG arena:

- `arena_load(json, kind, cfirst, ccount, children, sidx, dstart, dcount,
  delims, mn, mx, strs, sdir, st, err) -> status` fills the flat arrays read by
  the accepted `peg_dump`;
- `pq_build_params(format, gen_prompt_len, json, ...)` copies the format and
  the generation prompt, loads the arena, then forces `reasoning_format = AUTO`
  only after a successful load, exactly like the native constructor;
- `pq_warn_once(st)` is `warn_parse_failed_once`: the first call returns 1 and
  the rest 0, with the flag kept per session.

Serialization shape: `{"parsers":[...],"rules":{...},"root":N}`, one object per
variant keyed by `type` (epsilon/start/end/any/space, literal, sequence/choice,
repetition, and/not/atomic, chars, string, until, schema, rule, ref, tag, gbnf,
ac). Status codes: 0 ok, -1 not JSON, -2 missing members, -3 unknown type or a
missing field, -4 an id outside the array, -5 a caller array too small. The
native error texts are reproduced byte for byte for the checked cases.

Owned decision: child ids are bounds-checked here although the native code only
checks rules and root, because an out-of-range child would trap in `get()` later.

## Gate

`tests/build.sh` builds the two-module pair (`parse_params` + the accepted
`jsonparse`) with `--standalone`, compiles at `-O0` and runs ten cases / 58
checks:

```
FG577_S4 cases=10 values=58 failures=0
```

The cases cover a content-only arena, a 15-node tool-style arena, repetition
bounds and two until delimiters, quoted/newline/UTF-8 literals, unknown type,
non-JSON, missing members, invalid child/root ids, `build_parse_params`, and a
failed load with the warn-once flag.

## Mutations

`tests/mutate_s4.py 1|2` holds 8 + 7 mutations; each rewrites the loader,
rebuilds and reruns the gate.

```
python3 tests/mutate_s4.py 1   # 8/8 caught
python3 tests/mutate_s4.py 2   # 7/7 caught
```

## Recorded limits

- Ref/Rule resolution, `schema`/`gbnf`/`ac` payloads beyond what the dump
  reads, and `chars.ranges` storage stay open.
- The parser call over the loaded arena remains native; the ported engine still
  lacks `until`/`chars` nodes (the P7-08 gap).
- The load-to-dump text round-trip is not gated yet; the gate checks the flat
  fields.
- `source/jsonparse.goose` is a copy of the canonical parser (EXP493); the tree
  check verifies it.
