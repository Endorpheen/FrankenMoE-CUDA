# EXP-2026-11-15-627 — GGUF/loader/tokenizer/common offline slices (P7-12)

Base commit: `df3ce73a`. Native tree: llama.cpp-flint-cuda at `1b44e307`
(`src/llama-vocab.cpp` sha256 `e6dac06f…`, the same tree the S3b/P7 gates used).
Compiler: `goose-2026.3-string`, runtime `1e714e6146bc`.

## Why these three slices

P7-12 owns "GGUF, loader, tokenizer and common native dependencies". The ledger
entry listed the scope without decomposition, so this experiment starts with the
inventory below and picks the parts that are decidable from metadata alone: no
GGUF file, no weights, no model, no production change.

### Inventory of the scope

| native area | decisions | checkable offline? |
|---|---|---|
| `src/llama-vocab.cpp` `impl::load` vocab-type block | model string -> vocab type, default special ids, merges/charsmap refusal, `tokenizer_pre` -> pre-tokenizer and its nine flag adjustments | **yes**, pure metadata -> decision table |
| `src/llama-vocab.cpp` `tokenizer_st_partition` | special-token splitting, control/unknown skip, LSTRIP/RSTRIP trims | **yes**, pure text + token table |
| `common/common.cpp` string helpers | strip/split/join/replace/repeat/from/lcs/escapes/kv-override | **yes**, real native symbols are linkable |
| `common/common.cpp` `string_from(ctx, tokens)`, `string_get_sortable_timestamp`, `regex_escape` | detokenization, clock, `std::regex` | no: context, wall clock, regex engine |
| `src/llama-vocab.cpp` merges/charsmap/token arrays | bytes, endianness swap, attribute assignment, `cache_special_tokens` build | needs a GGUF file; kept in P7-02's GGUF remainder |
| `common/chat.cpp` template selection, `common/json*`, `common/grammar*` | jinja/pegtl/gbnf stacks | already tracked by P7-08/P7-09 experiments |
| loader-side shard naming, v1 headers, non-LE files | header arithmetic on real files | needs GGUF files; P7-02 remainder |

Taken here: the three "yes" rows. Rows "needs a GGUF file" stay open for a
later P7-12 slice with a synthetic GGUF corpus.

## S1 — vocab type and pre-tokenizer dispatch

`source/vocab-type.goose`, prefix `vt_`, canonical path `goose/model/vocab-type.goose`.

Ported: the whole `determine vocab type` block — nine tokenizer model families
with their default `bos/eos/unk/sep/pad/mask`, the BPE merges refusal rules
(absent unless `kimi-k2`, wrong key type, wrong array type), the t5 precompiled
charsmap checks (key type, element type, size header, bounds), sixty-three
pre-tokenizer names with their flag effects, the normalizer lower-case flag, the
chatglm BOS override, the non-BPE family defaults, and the error text of every
refusal.

The port carries pre-tokenizer **names**, never enum numbers: the integration
maps name -> native enum once.

### Corpus

`tests/gen-cases.py` reads the pre-tokenizer names straight out of
`src/llama-vocab.cpp`, so no name the native dispatch knows can be missing, and
adds fifteen near misses (case, trailing blank, digit suffix, `llama-3` vs
`llama3`, …). Eighteen model strings, five merges shapes, twelve charsmap
shapes, and a both-keys-bad case per pair.

```
cases 2556, record bytes 224908, failures 0      (-O1)
cases 2556, record bytes 224908, failures 0      (-O0)
```

Record form: `st= type= pre= ids= fl= lo= nm= err=`.

### What the gate forced

The first run failed on 71 cases and the failure was a real native order rule:
for the BPE family the merges key is read **before** `special_*_id` is written,
so a refused merges key reports the member-initialized ids (`1,2,0,-1,-1,-1`),
while a refused `tokenizer_pre` reports the BPE defaults (`fl=0,0,1`) because
those are already written. The port now reproduces the order, and the record
documents the state at the throw.

## S2 — special-token partitioning

`source/special-partition.goose`, prefix `stp_`, canonical path
`goose/model/special-partition.goose`.

Ported: `tokenizer_st_partition` — the loader-order scan over special tokens, the
split into left text / token / right text, the rescan of the remainder for the
same token (so adjacent identical tokens both become tokens), the
control-or-unknown skip when `parse_special` is false (user-defined tokens are
still split out), the LSTRIP/RSTRIP trims and the "empty part is not emitted"
rule.

The forward-list algorithm is rewritten over index arrays, which is where the
risk is; the gate is what makes the rewrite acceptable.

### Corpus

43 hand-written cases (positions, adjacency, overlapping candidates,
longest-first, strips, `parse_special` combinations, literal `.`/`*`/`\`, bytes
above 127, several tokens at once, empty text) plus 400 deterministic fuzz cases
over `{a,b,space,tab,\n,<,>,|}` with 1–5 tokens and random attributes.

```
cases 443, fragments 1469, failures 0            (-O1)
cases 443, fragments 1469, failures 0            (-O0)
```

## S3 — common string helpers

`source/common-strings.goose`, prefix `cs_`, canonical path
`goose/misc/common-strings.goose`.

Ported from `common/common.cpp`: `string_strip`, `string_split`, `string_join`,
`string_replace_all`, `string_repeat`, `string_from(bool)`,
`string_from(vector<int>)`, `string_lcs`, `string_process_escapes`,
`string_parse_kv_override`.

The oracle is the real library: the gate links `libllama-common.so` from
`build-cuda/bin` and calls the nine functions directly. This is the strongest
oracle in the three slices — there is no transcription at all.

### Corpus

140 comparisons over fourteen strip inputs, eighteen split pairs, seven join
shapes, twelve replace triples, six repeats, both booleans, six int vectors,
fifteen lcs pairs, twenty-eight escape strings and thirty-three kv-override
strings (including both key/value length limits, saturation, `0x1p3`, `1e400`,
`nan`, `inf`).

```
cases 140, output bytes 1012, failures 0         (-O1)
cases 140, output bytes 1012, failures 0         (-O0)
```

### What the gate forced

Two findings. First, `string_from(vector<int>)` is `"[ " … " ]"`, so the empty
vector is `"[  ]"` with two spaces. Second, and more interesting:
`string_process_escapes` accepts `\xHH` only when `strtol` consumes **exactly**
two bytes, so `\x 1`, `\x+1`, `\x-1`, `\x\tf` are all accepted with a one-digit
value (` 1` -> 0x01, `+1` -> 0x01, `-1` -> 0xFF, `\tf` -> 0x0F), while `\x0x`
and `\xg1` are not. The first port treated the pair as two hex digits and was
wrong for all four.

## Mutations

Twelve, two halves, each applied to one port, rebuilt by `tests/build.sh` at
`-O0` and re-run against the same oracle. Only a gate verdict counts as caught;
a compile error or a missing corpus is not a catch.

| # | slice | mutation | result |
|---|---|---|---|
| 0 | S1 | `no_vocab` reports the SPM type name | 138 failures |
| 1 | S1 | bert mask id becomes the sep id | 138 |
| 2 | S1 | `llama-bpe` loses its llama3 rule | 3 |
| 3 | S1 | BPE default keeps `clean_spaces` off | 364 |
| 4 | S2 | unknown tokens are split out too | 22 |
| 5 | S2 | LSTRIP reads the RSTRIP bit | 40 |
| 6 | S2 | the left remainder is kept even when empty | 10 |
| 7 | S2 | the partition whitespace set loses `\r` | 1 |
| 8 | S3 | the strip set loses the vertical tab | 2 |
| 9 | S3 | split advances one byte past the delimiter | 6 |
| 10 | S3 | `from(vector<int>)` opens without the blank | 6 |
| 11 | S3 | only uppercase `X` starts an escape | 8 |

```
mutations 6/6 caught (range 0:6) missed=[]
mutations 6/6 caught (range 6:12) missed=[]
```

Two candidate mutations were dropped after they turned out to be equivalent
mutants, not misses: `m + tl < bo + bl` -> `<=` (the right remainder is empty in
both readings) and `i = it` -> `i = it + 1` in the no-right branch (the next
fragment cannot contain any remaining token text). Both were replaced by
mutations that change observed output.

## Gate construction

`tests/make-native.py` cuts the two native bodies out of `src/llama-vocab.cpp`
on every build: S1 is the 522-line vocab-type block, S2 is the 116-line body of
`tokenizer_st_partition`. The wrapper supplies only what a header-only harness
needs — gguf readers that echo the case parameters, a `kv()` name map, a
`format()` over `vsnprintf`, `ggml_abort`, and an enum-name table parsed from
the native headers. One transformation is applied to the S1 text: the single
`return;` of the `no_vocab` arm becomes `goto done;` so the harness can report
the post-block state.

Enum values therefore never appear in the port, and a native change shows up as
a gate failure rather than as silent drift. `tests/gen-vocab-type.py` is the
authoring aid that emitted the S1 if/else chain; it fails loudly on any
statement it cannot express, and the gate, not the converter, is the authority.

## Compatibility checks

* `goose --standalone` parses, typechecks and emits C for all three modules
  with zero warnings at `-O1` and `-O0`.
* Gates 3/3 green at `-O1`, and again at `-O0`.
* Mutations 12/12 in two halves.
* No duplicate exports: `vt_`, `stp_` and `cs_` are unused in `goose/`
  (`grep -rho "export fn …" goose/ | sort | uniq -d`). Note that the tree
  already carries one unrelated duplicate, `align_ok` in
  `goose/model/gguf-offsets.goose` and `goose/model/mmap.goose`; it is not from
  this experiment and is reported, not touched.
* No production file was modified; the repository is untouched.

## Assumptions and known limits

1. Pre-tokenizer and vocab-type **numbers** stay native. The port returns names;
   the integration must map them back with the native enum, so a renumbering in
   the native header does not silently re-map a port decision.
2. For a refused model the S1 record is the state at the throw (see above). The
   native `llama_vocab::impl` is discarded at that point, so the record is only
   meaningful as a comparison artifact.
3. `isspace` on bytes above 127 is undefined in native (it is called on a
   possibly negative `char`). Strip and partition trim cases stay on ASCII; bytes
   above 127 appear only where no strip attribute is set.
4. An empty special-token text makes the native partition loop unable to
   advance. Such a vocab is refused upstream and is not in the corpus; the
   integration must keep that precondition.
5. `string_split` with an empty delimiter spins forever in native. The port
   returns `-1`; the call sites must keep their non-empty-delimiter
   precondition.
6. `cs_kv` takes a byte slice; native takes a NUL-terminated C string. An
   embedded NUL changes the meaning, so the caller must pass the same bytes
   `strlen` would see.
7. Float values in `cs_kv` are parsed by the host's `strtod` through the
   `gs_json_strtod` extern (the pattern EXP615 established), so "identical"
   assumes the same libc the native `atof` uses. The gate compares raw bits.
8. Not ported, by design: `string_from(ctx, tokens)`, `string_get_sortable_timestamp`,
   `regex_escape`, and the stray-storage half of `string_parse_kv_override`
   (the `llama_model_kv_override` struct itself).
