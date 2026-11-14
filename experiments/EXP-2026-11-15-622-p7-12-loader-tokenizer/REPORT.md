# REPORT — EXP-2026-11-15-627 (P7-12, GGUF/loader/tokenizer/common offline slices)

Date: 2026-11-15. Base commit `df3ce73a`. Native tree llama.cpp-flint-cuda at
`1b44e307`. No production file touched, no model started, no weight read.

## Result

| item | value |
|---|---|
| slices ported | 3 |
| gates | 3/3 green (`-O1`, repeated at `-O0`) |
| mutations | 12/12 caught in two halves (6/6 + 6/6) |
| oracle | 2 native extractions + 9 real `libllama-common.so` symbols |
| port source SHA-256 | `f744f1a7…` (S1), `154cfb0f…` (S2), `f6647d36…` (S3) |
| generated C SHA-256 | `3d7d029f…` (S1), `333b0271…` (S2), `a3677e02…` (S3) |
| oracle extract SHA-256 | `a1fefc66…` (S1), `b16a92f5…` (S2) |

Gate numbers:

```
S1  cases 2556, record bytes 224908, failures 0
S2  cases 443, fragments 1469, failures 0
S3  cases 140, output bytes 1012, failures 0
```

## 1. Canon and native interfaces the ports rely on

### Canon (Goose) side

* `export fn` with an explicit C signature: `u8[:]` -> `sl_u8 {uint8_t *data;
  int64_t len}`, `i64[:]` -> `sl_i64`, `f64[:]` -> `sl_f64`; `goose_init(0,
  nullptr)` runs before the first call; no `main` in the module.
* No allocation in the modules: every output is a caller-provided slice, so the
  integration must own the buffers.
* `--standalone` codegen, one object per module, linked with a plain C/C++ host.

### Native semantics taken as contract

* `llama_vocab_type` and `llama_vocab_pre_type` **names** (`NONE`, `SPM`, `BPE`,
  `WPM`, `UGM`, `RWKV`, `PLAMO2`, `PLAMO3`, `TEST`; the 63
  `LLAMA_VOCAB_PRE_TYPE_*` names), never their numbers.
* `LLAMA_TOKEN_NULL == -1`; `llama_token_attr` bits UNKNOWN `1`, CONTROL `8`,
  USER_DEFINED `16`, LSTRIP `128`, RSTRIP `256`.
* The tokenizer metadata keys `tokenizer.ggml.model`, `tokenizer.ggml.pre`,
  `tokenizer.ggml.merges`, `tokenizer.ggml.precompiled_charsmap`, and the two
  overrides `tokenizer.ggml.add_prefix`, `tokenizer.ggml.remove_extra_ws`.
* C locale for `isspace`/`tolower` decisions; `strtod` for the float kv value.
* Tokenizer refusal messages, byte for byte, including the trailing newline of
  the merges message (dropped only inside the gate record).

### Interfaces the ports must be fed

| module | input | source in the runtime |
|---|---|---|
| `vt_dispatch` | model/pre strings + five booleans + three counts derived from the gguf readers | `llama_model_loader` after metadata parse |
| `stp_partition` | text, token text table, attribute table, loader order, `parse_special` | `llama_vocab::impl` caches |
| `cs_*` | byte slices, no loader state | CLI/args, metadata, chat preprocessing |
| `cs_kv` | the raw `key=type:value` argument | command-line parser |

`vt_dispatch` receives the gguf answers as booleans (`has_merges`,
`merges_kv_is_array`, `merges_arr_is_string`, `charsmap_present`,
`charsmap_kv_is_array`, `charsmap_is_i8_or_u8`) plus the two lengths and the
charsmap size-header value. The port never calls gguf itself.

## 2. Exact integration step

### S1 — `goose/model/vocab-type.goose`

* Function: `vt_dispatch(model, pre_in, has_merges, merges_kv_is_array,
  merges_arr_is_string, n_merges, charsmap_present, charsmap_kv_is_array,
  charsmap_is_i8_or_u8, charsmap_len, charsmap_blob, out) -> i64`.
* Call site: replace the body of the `determine vocab type` block in
  `llama_vocab::impl::load`, i.e. everything from
  `if (tokenizer_model == "no_vocab" || tokenizer_model == "none") {` up to (not
  including) `ml.get_key(LLM_KV_TOKENIZER_ADD_PREFIX, …)`.
* Order: (1) read `tokenizer.ggml.model`, `tokenizer.ggml.pre`,
  `tokenizer.ggml.token_type_count`; (2) ask the four gguf questions about
  merges and charsmap; (3) call `vt_dispatch`; (4) parse the returned record
  once: status `0` continues, any other status refuses the model with the
  record's message, reproducing today's throw; (5) map the returned names to the
  native enums and assign the six ids, the nine flags and the normalizer
  lower-case flag; (6) only then apply `add_prefix` / `remove_extra_ws` and read
  the token list.
* Because the record carries names, the mapping table is the single place where
  a native renumbering would need attention.

### S2 — `goose/model/special-partition.goose`

* Function: `stp_partition(text, n_tok, tok_off, tok_len, tok_text, tok_attr,
  order, parse_special, out_kind, out_tok, out_off, out_len) -> i64`.
* Call site: `llama_vocab::impl::tokenize` replaces its
  `tokenizer_st_partition(fragment_buffer, parse_special)` call.
* Order: (1) build the order exactly as the loader does — special tokens sorted
  by text length, longest first (the tie order cannot matter: two different
  texts of equal length are never a prefix of each other); (2) call once per
  input text, before any pre-tokenizer runs, so each text fragment afterwards
  goes through the pretokenizer as today; (3) walk the returned fragments in
  index order, mapping `kind == 0` to the token id and `kind == 1` to
  `text[off .. off + len)`.
* Buffer sizing: `4 * (text.len + 2) + 16 * order.len + 16` fragments is what
  the gate uses and is an upper bound for the split growth (each match adds at
  most two fragments).

### S3 — `goose/misc/common-strings.goose`

* Functions (replace the same-named `common/common.cpp` helpers at their call
  sites): `cs_strip`, `cs_split`, `cs_join`, `cs_replace_all`, `cs_repeat`,
  `cs_from_bool`, `cs_from_ints`, `cs_lcs`, `cs_process_escapes`, `cs_kv`.
* `cs_split` returns the part count and writes parts back to back with their
  lengths; `cs_join` takes that layout, so a split/join pair needs no
  intermediate vector.
* `cs_kv` writes `res[0]` tag, `res[1]` int, `res[2]` bool, `res[3]` key length,
  `res[4]` value length, plus `out_f64[0]`; statuses `0` ok, `1` malformed,
  `2` invalid bool, `3` invalid type, `4` string too long. The integration maps
  these onto the `llama_model_kv_override` struct it already owns.
* Order for the loader: `cs_kv` runs while parsing CLI overrides, i.e. before
  the model is opened; `cs_process_escapes` runs where a shell-quoted string is
  ingested; `cs_strip` / `cs_split` / `cs_join` are called by chat and metadata
  preprocessing.

## 3. Assumptions and possible conflicts

* **Names vs numbers**: if the integration keeps native enum numbers and maps
  them from names, it must be updated only when a name is added; if it instead
  compares names, no numeric drift is possible. Do not store the record.
* **State at refusal** (S1): the record for a refused model is the native member
  state at the throw, not a valid vocab. Only the message and the status are
  meaningful for the caller.
* **BPE defaults before the chain**: a refused `tokenizer_pre` reports
  `add_space_prefix=0, escape_whitespaces=0, clean_spaces=1` while a refused
  merges key reports `0,1,0`. If the integration reorders those writes, the
  gate's record changes — that is intentional, the order is a native rule.
* **Empty special-token text** (S2) makes the native loop spin; the loader must
  keep refusing it. The port is not hardened against being called with it
  (calling it would simply emit the token without advancing the search bound).
* **`string_split` with an empty delimiter** returns `-1` here instead of
  spinning. A caller that currently relies on the native spin would see a
  difference; none exists in the tree.
* **`cs_kv` NUL semantics** differ from native for embedded NULs; pass the bytes
  `strlen` would see.
* **Float exactness** (S3) depends on the host `strtod`. If the integration ever
  parses kv floats without `strtod`, the last-bit agreement with `atof` is lost.
* **`isspace` above 127** stays excluded (native UB).
* **Duplicate export already in the tree**: `align_ok` exists in both
  `goose/model/gguf-offsets.goose` and `goose/model/mmap.goose` (EXP457 /
  mmap work), unrelated to this experiment. It is recorded here so the lead can
  decide; no file was changed.
* **Scope left in P7-12**: the token arrays, merges map, charsmap bytes and
  attribute assignment (`cache_special_tokens`) need a GGUF file; `chat.cpp`
  template selection, `json*`, `grammar*` stay with P7-08/P7-09; shard naming and
  v1/non-LE headers stay with P7-02's GGUF remainder.

## 4. Artifacts

```
source/vocab-type.goose          f744f1a7a31ffa52c62024388e28363858b9a2a47d7a5c09bef47b0637e8a5d2
source/special-partition.goose   154cfb0f910a66c147c74ba52f1277d60d43fa6c6cb21438240c9925b6a6e682
source/common-strings.goose      f6647d3636090a1ded775976945f9f6b2b7b8ca4c5f3da57a69e2f2e89026c5b
tests/make-native.py             496eaa7b6f277ad63a2f403535038f3134ed67e2be652e3cc58d3ccae9e8f0f8
tests/gen-vocab-type.py          21fbb2e1df4769dc3fef6f26dd0c166087f9cf661b568f2b2c8ca32618806b72
tests/gen-cases.py               3d686b34e7bb1fd8182ddf7f08f41a3831019403c6ad58e5afc61d0105df6983
tests/gen-partition-cases.py     08c47814289a4a98989366688a1341d73c300a03332ac87536942f2dd43bb4be
tests/build.sh                   e9436354c5e2a5d9f87c0c82ba7da60a38d6a4e275f85b2fa3fc8c67acb97bea
tests/mutate.py                  86d04505c446f423c8a9ab440b31329f12c3469bd98d1a3df2b99ca13c8a4751
tests/fixture_dispatch.cpp       a1c898ae13120862a269c5a251cbc207301c976a30c185251ca4ff60cc28e272
tests/fixture_partition.cpp      097cc3e207d8fde374969d257fdff924cd3878c05ee06f4099933c666613aefe
tests/fixture_strings.cpp        0b19f9b66fa241ab271f021c1bfd02281bd839e1fa83b038b0f637cb7524a077
build/native-dispatch.cpp        a1fefc6607c1d47382f39760a90fa349c06b52125355f2c1a305711920b45c60
build/native-partition.cpp       b16a92f51e0d3df67521641dd714f33705723024f51a6d4a93795ee302ffd467
build/vocab-type.c               3d7d029f99292596639ca507d38bb36bd4c149399d0583ae7667dbd2a4c2cf26
build/special-partition.c        333b0271af418d6a135426fdc8d13810e2c6fbe2cc4485a9343a0b7bd69bf841
build/common-strings.c           a3677e023f0b5cabecb77951fd5281b21f2ed81cc2aa7fa46b0528956d01037c
```

Generated C, objects and gate binaries are reproducible from the sources and
the pinned compiler and are **not** part of an accepted tree; the hashes above
are recorded so the lead can confirm reproducibility with
`goose --standalone` at the same runtime.

## 5. Reproduce

```
tests/build.sh                 # all three gates at -O1
OPT=-O0 OUT_DIR=build/o0 tests/build.sh
python3 tests/mutate.py 0 6
python3 tests/mutate.py 6 12
```

`LLAMA_DIR` overrides the native tree, `GOOSE_BIN` the compiler, `OUT_DIR` the
build directory; there is no absolute temporary path inside the scripts.

## 6. Verdict

ACCEPTED as an offline slice of P7-12: three decisions ported, each with a gate
that fails on every one of four targeted mutations, no production change, no
model, no weight. The remaining P7-12 work is the GGUF-file-dependent half and
is listed in section 3.
