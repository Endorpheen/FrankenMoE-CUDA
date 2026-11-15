#pragma once

// Staged bridge between llama.cpp and the Goose tokenizer decision unit
// (goose-gen/tokenizer-decisions.c, composed from goose/vocab-type.goose,
// goose/special-partition.goose and goose/common-strings.goose).
//
// This header is compiled into libllama only when FG_GOOSE_TOKENIZER is on;
// libllama-common reaches the string helper through it. No llama.h declaration
// is added, so the public C ABI is untouched: the three entry points below are
// C++ internal names that no released header exposes.
//
// The bridge owns the one thing the modules do not: the name -> enum mapping
// of this llama.cpp revision, plus the byte-for-byte refusal text the native
// code produced (one message carries a trailing newline).

#include <cstdint>
#include <string>

// The gguf answers the loader must collect before the decision can run. The
// module never calls gguf; the host reads keys and array headers and hands the
// answers over. Names follow the loader question, not the gguf API.
struct fg_goose_vt_answers {
    int     has_merges           = 0;
    int     merges_kv_is_array   = 0;
    int     merges_arr_is_string = 0;
    int64_t n_merges             = 0;

    int      charsmap_present    = 0;
    int      charsmap_kv_is_array = 0;
    int      charsmap_is_i8_or_u8 = 0;
    int64_t  charsmap_len        = 0;
    uint32_t charsmap_blob       = 0; // the xcda_blob_size header value, byte-swapped
};

struct fg_goose_vt_decision {
    int status   = 0; // 0 ok, otherwise the module status (see vocab-type.goose)
    int type     = 0; // llama_vocab_type
    int pre_type = 0; // llama_vocab_pre_type

    int bos  = 0;
    int eos  = 0;
    int unk  = 0;
    int sep  = 0;
    int pad  = 0;
    int mask = 0;

    int add_space_prefix   = 0;
    int escape_whitespaces = 0;
    int clean_spaces       = 0;
    int ignore_merges      = 0;
    int add_bos            = 0;
    int add_eos            = 0;
    int add_sep            = 0;
    int lower_case         = 0;

    int64_t merge_count = 0;

    std::string pre_out; // non-empty: the loader must overwrite tokenizer_pre
    std::string error;   // status != 0: the exact text the native code threw
};

// Returns 0 on a decision, nonzero on a refusal; in both cases `out` is filled.
int fg_goose_vocab_type_decide(const std::string & model,
                               const std::string & pre,
                               const fg_goose_vt_answers & answers,
                               fg_goose_vt_decision & out);

// Special-token partition over caller-owned arrays, the layout the module
// expects: n_tok entries of token text (back to back in tok_text, addressed by
// tok_off/tok_len), one attribute per token, the loader order as token ids.
// Writes one entry per fragment (kind 0 = token in out_tok, kind 1 = raw text
// at out_off/out_len) and returns the fragment count, or -1 when out_cap is
// too small for the bound the module was gated on.
int64_t fg_goose_st_partition(const char * text, int64_t text_len,
                              int64_t n_tok,
                              const int64_t * tok_off, const int64_t * tok_len,
                              const char * tok_text, const int64_t * tok_attr,
                              const int64_t * order, int64_t n_order,
                              int64_t parse_special,
                              int64_t * out_kind, int64_t * out_tok,
                              int64_t * out_off, int64_t * out_len,
                              int64_t out_cap);

// cs_strip over a byte range: writes the stripped bytes to out and returns the
// length, or -1 when the destination is smaller than the input (the module
// cannot fail otherwise).
int64_t fg_goose_cs_strip(const char * s, int64_t n, char * out, int64_t cap);

// Witness counter: one increment per dispatch into the Goose unit. The gate
// uses it to prove the switched path is the one that ran.
uint64_t fg_goose_tokenizer_calls();

// Number of revision pins that fired (the glm4/glm5 pin in the .cpp). One pin
// means the module wanted a decision this llama.cpp revision does not have; the
// gate prints the count so the difference cannot be forgotten.
uint64_t fg_goose_tokenizer_pins();
