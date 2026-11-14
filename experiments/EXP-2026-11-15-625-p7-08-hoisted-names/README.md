# EXP-2026-11-14-574 — standard tool builders (P7-08 slice 14)

Follow-up slice in this tree: hoisted native rule names (`until-*`/`ac-*`), see
"Hoisted-name parity" — `gbnf=ok 13/13`, `REPORT.md` carries the evidence and
the integration steps.

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
grammar, compared three ways: the textual arena dump, a rebuild identity check
(same graph isomorphism including node sharing) and the full GBNF rule map
(`gbnf=ok`):

```
cases=13 failures=0      # CT-1..5, J-1..5, T-1..3; node counts and dump sizes
                         # identical to the sub-566 run on canon 54751184
gbnf=ok 13/13            # was 1/13 before the hoist slice (CT-3 only)
```

Parse gate — the native chat parser driven by the built grammar:

```
parse_cases=9 failures=0 # P1..P9: JSON/XML single, two calls, args-first,
                         # partial, trailing junk, malformed exponent
```

## Mutations

`tests/mutate.py <lo> <hi>` rewrites the port, regenerates the C at `-O0`,
rebuilds and reruns the structural gate; `tests/parse_mutate.py` does the same
for the parse gate. A case counts as caught when the dump, the identity walk or
the GBNF map differs (`GBNF_DIFF` is a failure for the runner, not a note).

```
python3 tests/mutate.py 0 7    # 7/7 caught   (builders)
python3 tests/mutate.py 7 15   # 8/8 caught   (builders)
python3 tests/mutate.py 15 21  # 6/6 caught   (hoisted-name slice, half 1)
python3 tests/mutate.py 21 27  # 6/6 caught   (hoisted-name slice, half 2)
python3 tests/parse_mutate.py  # 5/5 caught   (gate is not vacuous)
```

The twelve hoist mutations are: effective kind back to a node id, seq separator,
seq group test, seq empty child, choice separator, choice group test, `gb_need`
Tag/Atomic unwrap, `gb_need` seq/choice grouping, schema rule name vs kind,
schema descent, the missing rule Ref slot and the arg-rule creation order.

The recorded runs are `tests/mutations_h1_7.json`,
`tests/mutations_h8_15.json`, `tests/mutations_h16_21.json`,
`tests/mutations_h22_27.json` and `tests/parse_mutations.json`.

## Exports

Canon `73c51a5e`: 81 `export fn` + 1 recursive. After the block: 83 + 1, the
two new names are `chat_standard_constructed_tools` and
`chat_standard_json_tools`; no export is lost and no name is duplicated.

## Hoisted-name parity

Status after the hoist slice: `gbnf=ok` on all 13 cases. The gate builds the GBNF
text on both sides and compares the whole rule map (ignoring the built-in `space`
rule); `gbnf=ok` therefore covers the hoisted `until-*`/`ac-*` names, the rule
bodies and the reachable rule set.

```
before: CT-1..CT-5, J-1..J-5, T-1..T-3 = gbnf=ok 1/13  (CT-3 only)
after : CT-1..CT-5, J-1..J-5, T-1..T-3 = gbnf=ok 13/13
```

Four defects produced the 1/13. All four are fixed in `source/runtime.goose` and
the fix is shipped as `tool-builders.hoisted-names.patch` /
`runtime.hoisted-names.patch` (one patch, applied to either canonical file).

1. Native insertion index. `to_gbnf` names a hoisted node after the *arena id*
   the node got when it was created, so the port's node id has to be the native
   id. Two things made it differ:

   - the native builders are `+` chains and the C++ compiler creates their
     operands right to left, while the port created them left to right. In the
     arg rule native creates `tool_arg_close`, then `tool_arg_value(until(...))`,
     then the key suffix, then the arg name, then `tool_arg_open`, and only then
     the sequence nodes. That is the whole +5 of CT-1/CT-2
     (native until-10/31 = port until-15/36). `constructed_arg_rule`, the
     tool body and the section body are now written in that order.
   - native `rule()` ends with `ref(name)` and `resolve_refs()` rewrites the
     children that pointed at it, so each rule leaves one unreachable Ref node in
     the arena. The port links the rule id directly and allocated nothing there;
     CT-5 needs the slot (native until-83/104 = port until-81/102 before the
     fix, because three `rule()` calls precede them). `peg_rule_ref` allocates
     the same slot and `peg_rule_seal` does it for the `rule(name, fn)` form.

   The invariant is now checkable per node: `GBNF_DIFF=3` plus the `NODES` dump
   show `native id == port id` for every reachable node and equal node counts
   (CT-1 81/81, CT-2 107/107, CT-3 1/1, CT-4 11/11, CT-5 155/155).

2. Body text. `gb_seq` and `gb_choice` kept their own idea of an empty child:
   a Tag(Epsilon) or an empty choice added a separator that native never writes
   (`space  space`, `()`). Both now render the child first and rewind, which is
   exactly `build_grammar`'s `if (child_gbnf.empty()) continue;`.

3. Grouping. `gb_eff` returned the *node id* of the effective node and the
   choice branch compared it with `PEG_CHOICE`, so any literal whose id happened
   to be 5 was printed as a parenthesised group (CT-5 `("'x'")`). It now returns
   the kind.

4. Schema rules. The JSON tools put the argument value behind
   `schema(json(), "tool-N-schema", params)`. Native prints the schema rule name
   and `collect_reachable_rules` descends into the schema child, which is why
   `json-value/json-object/json-array/json-string/json-number/json-bool/json-null`
   appear in the native grammar. The port stored only the schema *kind*
   (`object`), printed nothing for the node and did not descend, so all eight
   JSON cases were missing the `json-*` rules. The schema node now stores the
   rule name in `mx` (unused for that kind; the kind stays in `sidx` so the
   identity gate is unchanged), prints it as a rule reference and is descended
   into during collection.

`GBNF_DIFF=1` prints the first differing rules, `GBNF_DIFF=2` the id mapping,
`GBNF_DIFF=3` the first 20 node kinds, `GBNF_TEXT=1` both rule maps in full and
`NODES=1` both node tables; the last two are gate diagnostics added here.

## Recorded limits

- The constructed family now has full arena-id parity; the JSON family does not.
  Its builders were not reordered (J-3: native 211 nodes, port 210), which is
  invisible to GBNF today because that family creates no `until`/`ac` node. A
  later slice that adds one there must reorder those builders the same way.
- Schema *bodies* stay out of scope: `add_schema` in the gate returns the rule
  name only, and the port matches that plus the reachable `json-*` rules. The
  per-parameter schema grammar is still the schema-in-GBNF slice.
- `gb_choice` drops the separator of an empty alternative, like native, but no
  case in this corpus has one, so that branch is code-covered only.
- The gates run at `-O0` for the generated C (the 11.3 MB source makes `-O1`
  a separate budget), while the fixtures compile at `-O1`.
- The block is behaviour-preserving on this corpus; it does not add new native
  builders.
