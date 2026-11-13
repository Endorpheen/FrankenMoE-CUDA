# EXP501: malformed, deep and large corpus for the grammar and PEG parsers (P7-09 30th slice)

The last outstanding item of the P7-09 coverage audit. The functional slices
(EXP472-500) proved behaviour on ordinary inputs; this slice proves that the
GBNF parser and the PEG executor keep the same behaviour at scale and on damage.

## Modules under test

| module | source | oracle |
|---|---|---|
| GBNF parser | `source/gbnf.goose` (EXP480 plus the fix below) | `llama_grammar_parser::parse` (`src/llama-grammar.cpp`) |
| PEG executor, first half | `source/pegparse.goose` (EXP482) | `common_peg_arena::parse` (`common/peg-parser.cpp`) |
| PEG executor, second half | `source/pegparse2.goose` (EXP483) | `common_peg_arena::parse` |

The PEG modules are byte-identical to their slices; only the GBNF module changed.

## Defect found and fixed: the repetition scratch

`handle_reps` copied the preceding expression into two local `i64[1024]` arrays
before emitting it `mn..mx` times. A literal longer than 1024 elements therefore
overflowed the scratch: a 4000-byte literal with `{2}` was rejected by the port
while native accepted it. The port now keeps the copy in the tail of the rule
arena (`rtype`/`rval`, which are caller-sized) and refuses to run only when the
caller did not leave room, with the message `repetition scratch does not fit the
rule arena`. The experiment card keeps the fixed module so the slice is
self-contained; EXP480's card is unchanged.

## Corpus

GBNF (`tests/gbnf_deep_fixture.cpp`), 10 cases:

| case | what it proves |
|---|---|
| 500 nested groups | group depth does not change the parse |
| 20 000 rules | symbol table, rule table and element arena at scale |
| 100 000-byte literal | literal length independent of any fixed buffer |
| 4 000-byte literal `{2}` | the scratch fix above |
| 10 000 alternatives | alternative list at scale |
| 200 nested `{2}` | repetition nesting |
| 1 500 repetitions of a doubly grouped literal | the multiplied-rules guard |
| unterminated 100 000-byte literal | native error text at scale |
| 300-byte name missing `::=` | native error text at scale |
| `{2001}` | the repetition ceiling |

Every accepted case compares the full symbol map, the rule count and every
element `(type, value)` of every rule. Every rejected case compares the exact
native error text, captured from stderr.

PEG first half (`tests/peg_deep_fixture.cpp`), 28 result comparisons:

| case | what it proves |
|---|---|
| 600 nested `repeat(any,1,1)` | executor recursion depth |
| `any*` over 200 000 bytes | repetition at scale, and on a short/empty input |
| 100 000-byte literal | literal comparison at scale, and both off-by-one lengths |
| 50 000-byte literal then `any*` | sequential large match |
| 400 alternatives | choice at scale |

PEG second half (`tests/peg2_deep_fixture.cpp`), 11 result comparisons:

| case | what it proves |
|---|---|
| `until` with 200 delimiters over 200 000 bytes | delimiter search at scale, with and without a hit |
| `chars("a-z")` over 200 000 bytes | character class run, accepted and stopped |
| 100 000-byte string content | content scan, unterminated and heavily escaped |
| `ac(any, …)` over 200 000 bytes | Aho-Corasick run, with and without a delimiter |

Each PEG case compares the result type and both offsets for the lenient and
strict flag; the first half also compares several start offsets.

## Results

```text
FG501_GBNF cases=10 values=190340 failures=0
FG501_PEG  cases=28 values=28     failures=0
FG501_PEG2 cases=11 values=11     failures=0
```

- ASan + UBSan: all three fixtures clean, rc 0.
- Mutations, 7/7 caught: the scratch shrinks to 1024 (the original defect), the
  repetition guard loosens, the PEG repetition ignores its upper bound, the PEG
  literal compares one byte, `until` stops at the first byte, `chars` stops after
  one codepoint, string content ignores its delimiter.

Two mutations were dropped as equivalent or unreachable: the scratch overlap
guard cannot fire when the caller sizes the arena as documented, and the
`{2001}` ceiling mutation is already covered by EXP480's functional corpus.

## Scope

This closes the last item of `docs/goose-migration/P7-09-COVERAGE.md`. P7-09 is
declared DONE in `docs/goose-migration/P7-09-CLOSURE.md`; the remaining migration
work is P7-08 and the TODO items of the other phases.
