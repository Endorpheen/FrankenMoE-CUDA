# EXP-2026-11-15-666 — D1: two native bodies deleted, every stage flag ON by default

Base: `35516ce8`. Build and gate only. No model, no server, no GPU.

## 1. Two changes in one increment

**D1 removal (P11-04).** Two of the ten fully-replaced native bodies are gone:

| id | body | where it went |
|---|---|---|
| F1 | `json_schema_to_grammar` native converter in `common/json-schema-to-grammar.cpp` | frozen as `json_schema_to_grammar_oracle` in `gates/s11/fg_goose_smoke.cpp` |
| F9 | `string_strip_native` in `common/common.cpp` | already frozen in `gates/s15/native-strip-base.cpp` |

Both are function-level deletions inside the carrier patch, not file deletions:
the file stays, the body does not. `json_schema_to_grammar()` now has one path
per configuration (the Goose bridge, or the documented isolation stub) and
`string_strip()` likewise. Neither file carries a `#if 0` copy or a `*_native`
twin; the only surviving copy of each body is the gate's frozen oracle.

**Rule 10 (D0 remainder).** All ten `FG_GOOSE_*` flags now default ON in
`integration/m1/CMakeLists.txt`. A plain `cmake -B build` gets the ten Goose
stages; each flag exists only to turn one stage off for a bisect, an A/B or a
rollback. The comment above the option table says so.

## 2. Result

| check | OFF (isolation) | ON (product) |
|---|---|---|
| configure + build | rc=0 | rc=0 |
| `llama-server` | 17 793 496 B | 22 135 112 B |
| `goose_init` definitions | 0 | 1 |
| gates | 10/10 green, `goose_calls=0` | 10/10 green, counters match |
| S1.1 schema | 30 cases / 182 rules | 30 cases / 182 rules, `goose_calls=30` |
| S1.5 tokenizer | 182/182, strip 16/16 | 182/182, strip 16/16, `goose_calls=182` |
| `fg-host-smoke` | — | `threads=2 calls=512 goose_init_runs=1 iq_bound=1` PASS |
| `fg-iq-seam` | — | 15 360 comparisons, 0 failures PASS |

The S1.5 gate now compares `string_strip()` with the frozen `native_strip_base`
in an ON build and pins the unchanged-input stub in an OFF build, because the
production body it used to compare against no longer exists. The S1.1 gate does
the same with its local oracle.

## 3. Why the ON path is the one that proves the deletion

An OFF build cannot distinguish "the native body was deleted" from "the native
body is still there and we did not call it". Only the ON build proves the
replacement: it reports `goose_calls>0` and its output must equal the frozen
oracle. Both are recorded above.

The OFF build is kept as a diagnostic configuration, and the report says what it
answers (an empty grammar, the unchanged input) rather than pretending it is a
product build.

## 4. Scope

Deleted: 2 of the 18 candidate units in `P11-04-REMOVAL.md` (D1 of D1–D4).
Still open: F2–F4 (GGUF writer entries), F5 (split plan), F6–F7 (quant row
bodies), F8 (`determine_vocab_type`), F10 (`validate`), and the eight partial
units. The quant bodies go last, per R-1 in the removal plan, because they are
the only byte-parity fallback on the decode path.

No performance claim is made: the ON build has not been run against a model.

## 5. Reproduce

```
bash tools/check.sh integration
```

The staged tree at `builds/stage-m1` is assembled from tracked inputs by
`tools/stage-m1-tree.sh`; the check re-applies the integration layer in place and
lets Ninja rebuild only what changed.
