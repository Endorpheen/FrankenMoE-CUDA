# Codex handoff: blockers the local agent could not close

Created 2026-11-17 by the local agent (OpenCode session) at Igor's request.
Everything else in the migration is progressing in the normal cycle; only the
items below are handed over because they were attempted and did not yield.

Read this file top to bottom. Each section states: the goal, the exact
experiments already run with their results, the artefacts, and what is left.

---

## 1. Goose cannot call a libc symbol that the generated C already declares

**Goal.** Let Goose acquire the model's OS resources itself (open, fstat, mmap,
munmap, close) so model ownership stops being the caller's job. This is the last
piece of EXP924/open ownership work; everything else there (owner record,
generation counter, frame leases, close ordering) already exists in
`goose/model/qwen35-model-loader.goose` and is gated by 79 EXP919 checks.

**What was tried, in order.**

1. `extern "open" fn os_open(path: u8[:], flags: i64, mode: i64) -> i64;` plus
   `close`/`mmap`. Goose typechecks and emits C, but the C compile fails:

       error: conflicting types for 'open'; have 'int64_t(sl_u8, int64_t, int64_t)'
       /usr/include/x86_64-linux-gnu/bits/fcntl2.h:43: previous definition

   The generated translation unit includes libc headers that already declare
   `open` and `close`, and the Goose declaration uses the same C identifier with
   a different prototype. Same for `close` (`int(int)`).

2. Renaming the Goose-side function does not help: the extern *string* is the C
   symbol name, and the generated unit declares that name with the Goose
   prototype. Verified: `extern "open" fn goose_open(...)` still emits
   `int64_t open(sl_u8, int64_t, int64_t)`.

3. `-Wl,--defsym=gopen=open` fails: the linker only aliases symbols that are
   already defined or undefined in that link ("expression refers to an
   unresolvable symbol 'close'").

4. `objcopy --redefine-sym` was considered and rejected as fragile (return-value
   width and argument width would be silently wrong on some ABIs).

**What is needed.** One of:

- a compiler capability to declare an extern with an explicit C-compatible
  prototype (so `open` could be declared as `int(const char *, int, ...)`), or
- a way to bind a symbol separately from its C identifier, or
- an existing OS surface in the Goose standard library that this session did not
  find.

**Do not** add a C adapter: the repository rule counts every adapter as debt, and
this one would exist only to work around the compiler.

**Reproduce.** `goose check` on the probe in
`/home/end0/AI/LlamaServer/exp925-tokenizer-wire-20261117/obj/` (the probe source
is quoted in the EXP924 card,
`experiments/EXP-2026-11-17-924-goose-model-os-ownership/README.md`).

---

## 2. Trunk logits differ from llama.cpp at the level of native's own quantisation spread

**Goal.** Decide whether the Goose trunk is numerically equivalent to the shipped
llama.cpp graph, and if not, find and fix the cause. This blocks the stochastic
chain end-to-end comparison and the final trunk acceptance.

**What was excluded, by ablation on truncated real-weight models**
(`experiments/EXP-2026-11-17-923-trunk-logit-divergence/`, generator
`tests/make_layer_gguf.py`):

- MoE, both RMS norms, the residual chain, `output_norm` and the LM head are
  **bitwise equal** (attention zeroed: 248320/248320).
- q/k projections, both head norms, rope, the q-gate split and `wo` are not the
  cause (V zeroed: bitwise equal again; gate zeroed: difference persists).
- the stored V is the canonical Q4_0 quantisation of the exact V (288/288 bytes),
  the cache view geometry is right (144/288), `ggml_flash_attn_ext` reads
  quantised V exactly (512/512), `n_kv_max` does not change the result, and
  `qa_permute` is the same `permute(0,2,1,3)` llama.cpp uses.
- A hand-built branch from the same bytes is **bitwise the Goose path** and
  differs from native by mean 0.1723 (median 0.1449), while native's own Q4_0 vs
  F32 spread is mean 0.1462. Argmax agrees everywhere; greedy ids and text agree
  on all tested prompts.

**Where it stands.** Every structural difference of the measured size has been
eliminated; the residue is the same order as native's own KV-quantisation spread.
Igor has **not** approved a tolerance (`mean <= 0.2` was explicitly refused), so
nothing was relaxed and no claim of numerical equivalence is made.

**What is needed.** Either a definitive explanation of the residual (the most
promising untested lead: which exact cell and precision llama.cpp's V reaches
attention with, since the serialized state blob does not contain goose's cell in
either layout), or Igor's decision on an acceptance criterion.

**Do not** present matching greedy tokens as numerical equivalence - Igor called
that out explicitly.

---

## 3. Array payload reads through Goose break the vocabulary

**Goal.** Let the Goose reader answer `gguf_get_arr_str` and `gguf_get_arr_data`
so the vocabulary texts and the token-type array come from Goose instead of the C
`gguf` container.

**What was done.** Added `gr_val_span` (wire type, payload offset, bytes, element
type) and `gr_arr_str_span` (offset/length of element i) to the reader, plus C++
wrappers that return pointers into the reader's 64 MiB header mapping, with the C
calls as fallback. Replaced the calls in `llama-vocab.cpp`, `llama-model.cpp` and
`llama-model-loader.cpp`.

**Result: the instrumented numbers improved dramatically and the behaviour broke.**

    accepted library : gguf_get_arr_data = 8,     gguf_get_arr_str = 495921
    candidate        : gguf_get_arr_data = 3,     gguf_get_arr_str = 14

    pieces gate      : BAD (was identical)
    tokenize gate    : BAD (was identical)
    sampler gate     : OK

So the vocabulary is now read through Goose - 495921 calls become 14 - but the
texts it returns are not the same, i.e. the span arithmetic or the returned
pointer is wrong somewhere. Not diagnosed further in this session.

**Reverted.** The three files are back to using `gguf_get_arr_str`/`arr_data`,
the candidate was rebuilt and re-gated: pieces OK, tokenize OK, sampler OK. The
reader keeps `gr_val_span`/`gr_arr_str_span` (unused by the runtime) and everything
else from the previous slices stays wired (tensors, keys, kv_type/arr_type/arr_n,
tokenizer, sampler).

**Next attempt should check, in this order:**

1. whether `gr_arr_str_span`'s offset is measured from the header window or from
   the file start, and which the wrapper adds;
2. whether the payload offset `gr_val_span` returns for an array of strings is the
   first string's length word or the first character (they differ by 8 bytes);
3. whether any consumer needs a NUL-terminated string (our span is not
   terminated) - a `std::string` copy would be fine, `strlen` would not.

The instruments are `countarr.so` (LD_PRELOAD counters for the two calls) and the
three gates above; `ggufcmp` already proves the reader's own key/tensor indexes.

## 4. Not blockers: work in progress, for context only

- Metadata: `gguf_get_kv_type/arr_type/arr_n` now go through Goose (252 -> 99,
  17 -> 5, 17 -> 5 on a load); the remainder is `gguf_kv_to_str` logging and
  `gguf_get_val_*`, which are the current slice.
- Tokenizer, detokenizer and sampler inside `libllama` are accepted and gated
  (EXP925): pieces 5673158 bytes identical, tokenize 687 ids identical, sampler
  384 draws identical, `gguf_find_tensor` 0 calls, `gguf_find_key` 94 -> 1.
- Bridges already in the shipped library: `fg202t*` (tokenizer), `fg213p*` (piece
  copy), `fg401s*` (detok), 15 symbols total in `libllama.so.0`; the server
  library carries `fg268p/fg270q/fg272s/fg274b/fg276t`.

## How to check the current state

    cd /home/end0/AI/LlamaServer/exp925-tokenizer-wire-20261117
    LD_LIBRARY_PATH=run-cand ./pieces  /home/end0/AI/LocalAI/Models/Apex-35B-V1.0-Q4_K_M.gguf p.bin
    cmp accepted.bin p.bin                       # expect identical
    LD_LIBRARY_PATH=run-cand ./tokdump  /home/end0/AI/LocalAI/Models/Apex-35B-V1.0-Q4_K_M.gguf corpus.txt t.bin
    cmp tok_accepted.bin t.bin                   # expect identical
    LD_LIBRARY_PATH=run-cand ./sampdump s.bin
    cmp samp_accepted.bin s.bin                  # expect identical

`countgguf.so`, `countkey.so`, `countmeta.so` (LD_PRELOAD counters) and
`ggufcmp` (Goose index vs gguf over the whole file) live in the same directory and
are the instruments used for every claim above.
