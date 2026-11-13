# EXP488: Aho-Corasick automaton in Goose (P7-09 seventeenth slice)

The automaton behind `gbnf_excluding/including_grammar` for until/ac: trie over
codepoints, failure links, terminal propagation and the next(state, codepoint)
walk. Gated against `common_aho_corasick` (trie.cpp).

## Gate

12 delimiter sets (single/prefix/shared-suffix/duplicate/empty/unicode) x every
state x a fixed codepoint alphabet: state count, match_pattern per state and
every transition: **12 cases, 987 values, 0 failures**; ASan+UBSan clean.

Mutations 4/4: self fail link (non-terminating, caught by timeout), terminal
propagation, next without fail links, duplicate pattern overwrite. Child sorting
is not observable in this gate (lookups are by codepoint), so it is not mutated.

The GBNF text emitter (`gbnf_ac_grammar`, char classes, excluding/including
rules) is the next slice.
