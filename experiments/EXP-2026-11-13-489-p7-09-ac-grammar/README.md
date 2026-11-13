# EXP489: GBNF Aho-Corasick rule emitter in Goose (P7-09 eighteenth slice)

`gbnf_excluding_grammar` (until) and `gbnf_including_grammar` (ac) on top of the
EXP488 automaton: state names `prefix[-NN]`, buckets/completing/specific classes,
`gbnf_char_class`/`gbnf_escape_char_class` and the empty-delimiter terminal root.

## Gate

Nine until/ac parsers (single/prefix/shared-suffix/empty delimiters, a dash,
multi-delimiter) are built natively; the native `build_grammar` rule map is
filtered to `until-*`/`ac-*` and compared with the Goose emitter, and the root
reference is checked against the start name: **9 cases, 18 values, 0 failures**.
ASan+UBSan clean.

Mutations 8/8: leading bar, bucket negation, completing class, specific negation,
state-name zero padding, dash escaping, terminal skipping, empty-delimiter rule.

P7-09 still needs non-delegating schemas (add_schema), lazy triggers, schema
parsers and the JSON side.
