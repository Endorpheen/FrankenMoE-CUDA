# EXP-2026-11-14-574 — hoisted-name parity (P7-08 slice 15)

Base commit: `29a930e6`. Work tree: `/tmp/opencode/sub-620/exp574` (a copy of the
experiment; the repository itself was not touched). Ported file:
`source/runtime.goose`, byte-identical to `goose/jinja/tool-builders.goose` and
to the first 17575 lines of `goose/jinja/runtime.goose` at the base commit.

## Result

| gate | before | after |
|---|---|---|
| structural, dump + identity | `cases=13 failures=0` | `cases=13 failures=0` |
| structural, GBNF rule map | `gbnf=ok 1/13` (CT-3 only) | `gbnf=ok 13/13` |
| parse (native parser on the built grammar) | `parse_cases=9 failures=0` | `parse_cases=9 failures=0` |
| mutations, 12 new on this logic | — | `6/6` + `6/6` caught |
| mutations, 15 existing | `7/7` + `8/8` | `7/7` + `8/8` |
| parse mutations | `5/5` | `5/5` |

No JSON case is left marked: the schema stub turned out to be one rule name plus
one missing descent, so all eight JSON/T cases reach `gbnf=ok` as well.

The ported source is `source/runtime.goose`, SHA-256
`24d7f18982eda13471c54433ebff2842144024803772082309668d2328de4533`.

## What was wrong and what changed

The port's node id has to be the native arena insertion index, because native
`to_gbnf` names every hoisted node `until-<arena id>` / `ac-<arena id>`. Four
separate defects kept it from being one.

1. **Creation order of the constructed builders.** Native builders are `+`
   chains and the compiler creates the operands right to left; e.g. the arg rule
   is built as `tool_arg_close`, `tool_arg_value(until(closer))`,
   `literal(key_suffix)`, `tool_arg_name(names)`, `tool_arg_open(literal(prefix))`,
   then the sequence nodes. The port built the same graph left to right, which
   put five nodes before the `until` node that native creates after it — the
   measured +5 (`native until-10/31` vs `port until-15/36`).
   `constructed_arg_rule`, the tool body and the section body are now written in
   native order.

2. **The Ref slot of every `rule()`.** Native `rule(name, p)` ends with
   `ref(name)` and `resolve_refs()` rewrites the children that pointed at it, so
   each rule call leaves one unreachable Ref node in the arena. The port links
   the rule id directly and allocated nothing there. CT-5 needs the slot: three
   `rule()` calls precede its later hoisted nodes, and the port was 2 nodes short
   (`native until-83/104` vs `port until-81/102`). `peg_rule_ref` allocates the
   slot and `peg_rule_seal` does it for the `rule(name, fn)` form.

3. **GBNF text of empty children and grouping.** `gb_seq`/`gb_choice` kept their
   own notion of an empty child, so a `Tag(Epsilon)` or an empty choice produced
   `space  space` and `()` where native produces nothing. They now render the
   child and rewind, matching `build_grammar`'s
   `if (child_gbnf.empty()) continue;`. `gb_eff` returned a *node id* and the
   choice branch compared it to `PEG_CHOICE`, so a literal with id 5 was printed
   as `("'x'")`; it returns the kind now.

4. **Schema rule name and schema descent.** `standard_json_tools` puts the
   argument value behind `schema(json(), "tool-N-schema", params)`. Native prints
   the *rule name* returned by `add_schema` and `collect_reachable_rules` walks
   into the schema child, which is why
   `json-value/json-object/json-array/json-string/json-number/json-bool/json-null`
   are in the native grammar. The port kept only the schema kind (`object`),
   printed nothing and did not descend. The schema node now carries the rule name
   in `mx` (unused for that kind; the kind stays in `sidx`, so the identity
   gate's payload comparison is unchanged), prints it as a rule reference and is
   descended into while collecting rules.

## Evidence

Before (canonical `tool-builders.goose`, `GBNF_DIFF=2`, CT-1):

```
  body-diff tool-0
    native: ("<function=" "get_weather" ">") space (("<param=" ("location" | "\"location\"" | "'locati
    port  : ("<function=" "get_weather" ">") space (("<param=" ("location" | "\"location\"" | ("'locat
  only-native until-10 ::= | [<] until-10-01 | [^<] until-10
  id native=10 port=15
  id native=31 port=36
  only-port until-15 ::= | [<] until-15-01 | [^<] until-15
```

After (same command): no output, `CT-1 ok ... gbnf=ok`.

Allocation parity is now per node, not just per name. `GBNF_DIFF=3` plus the
`NODES` dump give the native id -> port id map; the delta is zero for every
reachable node and the node counts are equal:

```
CT-1 native=81  port=81   CT-2 native=107 port=107  CT-3 native=1   port=1
CT-4 native=11  port=11   CT-5 native=155 port=155
```

The same patch applied to the canonical `goose/jinja/runtime.goose` (which also
carries the newer delimiter block) typechecks with goose and runs both gates at
`gbnf=ok 13/13`, `parse_cases=9 failures=0`.

## Integration

1. Apply `patch/tool-builders.hoisted-names.patch` to
   `goose/jinja/tool-builders.goose`, and
   `patch/runtime.hoisted-names.patch` to `goose/jinja/runtime.goose`. The two
   files carry the same functions, so the hunks are identical; the offset
   differs by nothing (the delimiter block starts after line 17575).
2. `patch/fixture.diagnostics.patch` is optional gate tooling: `GBNF_TEXT=1`
   prints both rule maps and `NODES=1` both node tables. Neither runs unless the
   env var is set and neither changes the pass/fail logic.
3. `tests/build.sh` then `tests/mutate.py 15 21`, `tests/mutate.py 21 27`,
   `tests/parse_mutate.py`.
4. No exported name or signature changed: 83 `export fn` before and after, same
   list. `chat_standard_json_tools` keeps its `schema_kind` parameter (the schema
   *kind* is still stored); the rule name is derived per tool as `tool-<i>-schema`
   exactly like the native builder.
5. Ledger synchronisation (the lead has to do it, the repository was not
   touched here). `docs/goose-migration/tasks.json`, requirement `P7-08`:
   `slices_total 8 -> 9`, `slices_done 8 -> 9`, drop the first entry of
   `remaining` ("hoisted native rule names ..."), refresh `status_totals` and
   `updated_at`; top-level `work_front`: `slices_total 111 -> 112`,
   `slices_done 110 -> 111`, `slices_remaining 1 -> 0`. In
   `docs/goose-migration/P7-08-COVERAGE.md` the "Hoisted rule names" section says
   the fix is a separate slice and that the native comparison is 0/4; it has to
   be rewritten to the closed state, and the `P7-08` evidence line in
   `tasks.json` already ends with "pending: hoisted native names".

## Limits

- The constructed family has full arena-id parity; the JSON family does not
  (J-3: native 211 nodes, port 210) because its builders were not reordered. It
  is invisible to GBNF today: that family creates no `until`/`ac` node. A later
  slice that adds one must reorder those builders the same way.
- Schema *bodies* remain out of scope: the gate's `add_schema` returns the rule
  name only and the port matches that plus the reachable `json-*` rules. The
  per-parameter schema grammar is still the schema-in-GBNF slice.
- `gb_choice` drops the separator of an empty alternative like native, but no
  case in this corpus has an empty alternative, so that branch is code-covered
  only (an earlier mutation proposal for it was measured as not observable and
  was replaced).
- Gates still run the generated C at `-O0`.

## Artefacts and hashes

```
source/runtime.goose                             24d7f18982eda13471c54433ebff2842144024803772082309668d2328de4533
patch/tool-builders.hoisted-names.patch          1ab2251bb0e8ecbb9ef10dc84eae8e1d3c8dd292ce4f423377ca01d47d3b47b3
patch/runtime.hoisted-names.patch                e3f777576f619b34229a9c7836634c7495d6228a0e338f5f4368ef28f43a00bf
patch/fixture.diagnostics.patch                  e97582622a602f700cd6053c7608dba50b817a4f26210caa625b30d45403bb85
before/gate.txt                                  db8fc408dae3bb94f9ef88d1ba97bd3a1a7aa6ba18d78e4b3c915a627bf27c44
before/gbnf_diff.txt                             ca378d8eb8cf439f272f1c86c459c0e3792be5e34b88e2c2e69062eb7bd758c0
after_gate.txt                                   2e9f16d5f0457a7d7cbafe49592e4a0f8d3c61876833f9521c7a4168603c25e6
after_parse.txt                                  cdd2e78a93b82756c23716e317720baa3de41cc47efea13fc007315c273c256f
after_gbnf_diff.txt                              32dfb245c74ed2a7a8983b79d3f69332a6363c65f5ab7dd938d1b15aaa97212a
tests/mutations_h1_7.json                        33828de426d91133e9b5f493b02bd526e96b4305f6ece1ac1ad1856488ba2951
tests/mutations_h8_15.json                       1e42fa55bf09c44670412d4ef825f79d3400b5c398a887d3f383e19883249426
tests/mutations_h16_21.json                      5221ac883d25f4398d6bb8e851dea6c759fe42d290864c9556ba5b20ae8f7a08
tests/mutations_h22_27.json                      0b789bef10be48934d1653f11936132f6706de074264c8de0620ba4717036e56
tests/parse_mutations.json                       56b808edcc808d698c35fc65968460c2eca2e9d731895caa60069954f0a34b19
```

Compiler: `goose-2026.3-string` (standalone), `gcc-13 -O0 -std=c11` for the
generated C, `g++-13 -O1 -std=c++17` for the fixtures, native side from
`llama.cpp-flint-cuda/build-cuda/bin/libllama-common`. No weights, no model, no
server.
