// Staged S1.5 gate: the three switched tokenizer decisions against the native
// text they replace.
//
// What runs where:
//
//   OFF build   S1/S2 compare the extraction of the staged llama-vocab.cpp with
//               the extraction of the pristine reference copy, so the moved
//               native bodies must produce the same records case by case.
//               S3 compares the library's string_strip (native body) with the
//               reference extraction.
//   ON  build   S1/S2 compare the Goose bridge (the exact function the loader
//               calls) with the staged extraction. S3 compares the library's
//               string_strip (Goose body) with the reference extraction.
//
// No model, no weights, no gguf file: every case is synthetic metadata and
// synthetic strings.

#include <cstdio>
#include <cstring>
#include <cstdint>
#include <string>
#include <vector>
#include <deque>

#include "native-dispatch-base.h"
#include "native-dispatch-staged.h"
#include "native-partition-base.h"
#include "native-partition-staged.h"
#include "native-strip-base.h"

#include "common.h" // string_strip, the switched helper
#include "sweep-cases.h" // every model and pre-tokenizer name of the tree

#ifdef FG_GOOSE_TOKENIZER
#include "fg-goose-tokenizer-bridge.h"
#endif

namespace {

int  g_failures = 0;
bool g_dump     = false;

void report(const char * what, const std::string & got, const std::string & want) {
    if (got == want) {
        if (g_dump) {
            printf("OK %s | %s\n", what, got.c_str());
        }
        return;
    }
    ++g_failures;
    printf("MISMATCH %s\n  got  %s\n  want %s\n", what, got.c_str(), want.c_str());
}

[[maybe_unused]] std::string strip_newlines(std::string s) {
    while (!s.empty() && s.back() == '\n') {
        s.pop_back();
    }
    return s;
}

// ---------------------------------------------------------------------------
// S1: vocab type and pre-tokenizer dispatch
// ---------------------------------------------------------------------------

struct vt_case {
    const char * name;
    const char * model;
    const char * pre;
    bool     has_merges           = true;
    bool     merges_kv_is_array   = true;
    bool     merges_arr_is_string = true;
    int      n_merges             = 4;
    bool     has_charsmap         = false;
    bool     charsmap_kv_is_array = true;
    bool     charsmap_is_i8_or_u8 = true;
    int      charsmap_len         = 8;
    uint32_t charsmap_blob        = 2;
    bool     vocab_size_known     = false;
    uint32_t vocab_size           = 0;
};

std::vector<vt_case> build_vt_cases() {
    std::vector<vt_case> cases = {
        { "no-vocab",            "no_vocab",   ""            },
        { "none",                "none",       ""            },
        { "no-vocab-vocab-size", "no_vocab",   "", true, true, true, 4, false, true, true, 8, 2, true, 64 },
        { "llama",               "llama",      ""            },
        { "llama-pre-ignored",   "llama",      "llama3"      },
        { "bert",                "bert",       ""            },
        { "bert-pre-ignored",    "bert",       "qwen2"       },
        { "rwkv",                "rwkv",       ""            },
        { "plamo2",              "plamo2",     ""            },
        { "test-tree-gap",       "test",       ""            },
        { "plamo3-tree-gap",     "plamo3",     ""            },
        { "unknown-model",       "nope",       ""            },

        { "gpt2-empty-pre",      "gpt2",       ""            },
        { "gpt2-default",        "gpt2",       "default"     },
        { "gpt2-qwen2",          "gpt2",       "qwen2"       },
        { "gpt2-llama3",         "gpt2",       "llama3"      },
        { "gpt2-chatglm",        "gpt2",       "chatglm-bpe" },
        { "gpt2-glm4",           "gpt2",       "glm4"        },
        { "gpt2-tekken",         "gpt2",       "tekken"      },
        { "gpt2-gpt4o",          "gpt2",       "gpt-4o"      },
        { "gpt2-youtu",          "gpt2",       "youtu"       },
        { "gpt2-gemma4-pre",     "gpt2",       "granite-embed-multi-311m" },
        { "gpt2-sarvam",         "gpt2",       "sarvam-moe"  },
        { "gpt2-deepseek-v3",    "gpt2",       "deepseek-v3" },
        { "gpt2-jina-sep",       "gpt2",       "jina-v1-en"  },
        { "gpt2-chameleon",      "gpt2",       "chameleon"   },
        { "gpt2-megrez",         "gpt2",       "megrez"      },
        { "gpt2-exaone4",        "gpt2",       "exaone4"     },
        { "gpt2-minicpm5",       "gpt2",       "minicpm5"    },
        { "gpt2-no-merges-kimi", "gpt2",       "kimi-k2",  false },
        { "gpt2-no-merges",      "gpt2",       "default",  false },
        { "gpt2-merges-not-arr", "gpt2",       "default",  true, false },
        { "gpt2-merges-not-str", "gpt2",       "default",  true, true, false },
        { "gpt2-unknown-pre",    "gpt2",       "no-such-pre" },
        { "gpt2-pre-flint-gap",  "gpt2",       "flint"       },
        { "hybriddna",           "hybriddna",  "llama3"      },
        { "whitespace-model",    "whitespace", "whitespace"  },
        { "gpt2-zero-merges",    "gpt2",       "default",  true, true, true, 0 },

        { "t5-no-charsmap",      "t5",         ""            },
        { "t5-charsmap-ok",      "t5",         "", true, true, true, 0, true, true, true, 8, 2 },
        { "t5-charsmap-blob",    "t5",         "", true, true, true, 0, true, true, true, 8, 4 },
        { "t5-charsmap-small",   "t5",         "", true, true, true, 0, true, true, true, 3, 0 },
        { "t5-charsmap-kv-type", "t5",         "", true, true, true, 0, true, false },
        { "t5-charsmap-el-type", "t5",         "", true, true, true, 0, true, true, false },

        { "gemma4",              "gemma4",     ""            },
        { "gemma4-pre",          "gemma4",     "qwen2"       },
        { "gemma4-no-merges",    "gemma4",     "", false      },
        { "gemma4-merges-type",  "gemma4",     "", true, false },
    };

    // Name sweep: every model name and every pre-tokenizer name this revision
    // compares against. Labels are owned by this function, which runs once.
    // A deque: references to its strings stay valid while it grows.
    static std::deque<std::string> labels;

    for (const char * model : fg_sweep_models) {
        labels.push_back(std::string("sweep-model/") + model);
        vt_case c;
        c.name        = labels.back().c_str();
        c.model       = model;
        c.pre         = "";
        cases.push_back(c);
    }

    for (const char * pre : fg_sweep_pres) {
        labels.push_back(std::string("sweep-pre/") + pre);
        vt_case c;
        c.name        = labels.back().c_str();
        c.model       = "gpt2";
        c.pre         = pre;
        cases.push_back(c);
    }

    return cases;
}

const std::vector<vt_case> & vt_cases() {
    static const std::vector<vt_case> cases = build_vt_cases();
    return cases;
}

case_cfg_staged to_cfg_staged(const vt_case & c) {
    case_cfg_staged cfg;
    cfg.has_merges           = c.has_merges;
    cfg.merges_kv_type       = c.merges_kv_is_array ? GGUF_TYPE_ARRAY : GGUF_TYPE_STRING;
    cfg.merges_arr_type      = c.merges_arr_is_string ? GGUF_TYPE_STRING : GGUF_TYPE_UINT32;
    cfg.n_merges             = c.n_merges;
    cfg.has_charsmap         = c.has_charsmap;
    cfg.charsmap_kv_type     = c.charsmap_kv_is_array ? GGUF_TYPE_ARRAY : GGUF_TYPE_STRING;
    cfg.charsmap_arr_type    = c.charsmap_is_i8_or_u8 ? GGUF_TYPE_INT8 : GGUF_TYPE_FLOAT32;
    cfg.charsmap_len         = c.charsmap_len;
    cfg.charsmap_blob        = c.charsmap_blob;
    cfg.vocab_size_known     = c.vocab_size_known;
    cfg.vocab_size           = c.vocab_size;
    return cfg;
}

case_cfg_base to_cfg_base(const vt_case & c) {
    case_cfg_base cfg;
    cfg.has_merges           = c.has_merges;
    cfg.merges_kv_type       = c.merges_kv_is_array ? GGUF_TYPE_ARRAY : GGUF_TYPE_STRING;
    cfg.merges_arr_type      = c.merges_arr_is_string ? GGUF_TYPE_STRING : GGUF_TYPE_UINT32;
    cfg.n_merges             = c.n_merges;
    cfg.has_charsmap         = c.has_charsmap;
    cfg.charsmap_kv_type     = c.charsmap_kv_is_array ? GGUF_TYPE_ARRAY : GGUF_TYPE_STRING;
    cfg.charsmap_arr_type    = c.charsmap_is_i8_or_u8 ? GGUF_TYPE_INT8 : GGUF_TYPE_FLOAT32;
    cfg.charsmap_len         = c.charsmap_len;
    cfg.charsmap_blob        = c.charsmap_blob;
    cfg.vocab_size_known     = c.vocab_size_known;
    cfg.vocab_size           = c.vocab_size;
    return cfg;
}

#ifdef FG_GOOSE_TOKENIZER
// The record the bridge produced, printed in the layout of the native one.
std::string goose_record(const fg_goose_vt_decision & d) {
    const std::string err = strip_newlines(d.error);
    char buf[1024];
    snprintf(buf, sizeof(buf),
             "st=%d type=%s pre=%s ids=%d,%d,%d,%d,%d,%d fl=%d,%d,%d,%d,%d,%d,%d lo=%d nm=%d err=%s",
             d.status, vocab_type_name_staged(d.type), pre_type_name_staged(d.pre_type),
             d.bos, d.eos, d.unk, d.sep, d.pad, d.mask,
             d.add_space_prefix, d.escape_whitespaces, d.clean_spaces,
             d.ignore_merges, d.add_bos, d.add_eos, d.add_sep,
             d.lower_case, (int) d.merge_count, err.c_str());
    return std::string(buf);
}
#endif

// A refusal is a status and a message: the loader throws and never reads the
// remaining fields, so the record is compared in full only when both sides
// decided. Neither side's state at a throw is part of any contract.
std::string case_key(int status, const std::string & record, const std::string & error) {
    if (status == 0) {
        return record;
    }
    char buf[64];
    snprintf(buf, sizeof(buf), "st=%d err=", status);
    return std::string(buf) + strip_newlines(error);
}

void run_vt(int & cases, int & passed) {
    for (const auto & c : vt_cases()) {
        ++cases;

        const dispatch_decision_base d_base = native_dispatch_base(c.model, c.pre, to_cfg_base(c));
        const dispatch_decision_staged d_staged = native_dispatch_staged(c.model, c.pre, to_cfg_staged(c));

        const std::string got_base = case_key(d_base.status, format_record_base(d_base), d_base.error);
        const std::string want = case_key(d_staged.status, format_record_staged(d_staged), d_staged.error);

#ifdef FG_GOOSE_TOKENIZER
        fg_goose_vt_answers answers;
        answers.has_merges           = c.has_merges ? 1 : 0;
        answers.merges_kv_is_array   = c.merges_kv_is_array ? 1 : 0;
        answers.merges_arr_is_string = c.merges_arr_is_string ? 1 : 0;
        answers.n_merges             = c.n_merges;
        answers.charsmap_present     = c.has_charsmap ? 1 : 0;
        answers.charsmap_kv_is_array = c.charsmap_kv_is_array ? 1 : 0;
        answers.charsmap_is_i8_or_u8 = c.charsmap_is_i8_or_u8 ? 1 : 0;
        answers.charsmap_len         = c.charsmap_len;
        answers.charsmap_blob        = c.charsmap_blob;

        fg_goose_vt_decision d;
        fg_goose_vocab_type_decide(c.model, c.pre, answers, d);
        const std::string got = case_key(d.status, goose_record(d), d.error);
#else
        const std::string got = got_base;
#endif

        const std::string what = std::string("vt/") + c.name;
        const int before = g_failures;
        report(what.c_str(), got, want);
        if (g_failures == before) {
            ++passed;
        }
    }
}

// ---------------------------------------------------------------------------
// S2: special-token partition
// ---------------------------------------------------------------------------

struct partition_case {
    const char * name;
    const char * text;
    std::vector<const char *> tokens;
    std::vector<int>          attrs;
    std::vector<int>          order;
    bool parse_special;
};

// Attribute bits: UNKNOWN 1, CONTROL 8, USER_DEFINED 16, LSTRIP 128, RSTRIP 256.
enum { ATTR_UNKNOWN = 1, ATTR_NORMAL = 4, ATTR_CONTROL = 8, ATTR_USER = 16,
       ATTR_LSTRIP = 128, ATTR_RSTRIP = 256 };

const std::vector<partition_case> & partition_cases() {
    static const std::vector<partition_case> cases = {
        { "plain-no-special",   "hello world",      { "<s>" }, { ATTR_CONTROL }, { 0 }, true },
        { "one-token",          "a<s>b",            { "<s>" }, { ATTR_CONTROL }, { 0 }, true },
        { "adjacent",           "<s><s>",           { "<s>" }, { ATTR_CONTROL }, { 0 }, true },
        { "token-only",         "<s>",              { "<s>" }, { ATTR_CONTROL }, { 0 }, true },
        { "leading",            "<s>abc",           { "<s>" }, { ATTR_CONTROL }, { 0 }, true },
        { "trailing",           "abc<s>",           { "<s>" }, { ATTR_CONTROL }, { 0 }, true },
        { "control-skip",       "a<s>b",            { "<s>" }, { ATTR_CONTROL }, { 0 }, false },
        { "unknown-skip",       "a<x>b",            { "<x>" }, { ATTR_UNKNOWN }, { 0 }, false },
        { "user-kept",          "a<u>b",            { "<u>" }, { ATTR_USER },    { 0 }, false },
        { "control-and-user",   "a<k>b",            { "<k>" }, { ATTR_CONTROL | ATTR_USER }, { 0 }, false },
        { "lstrip",             "a   <s>b",         { "<s>" }, { ATTR_CONTROL | ATTR_LSTRIP }, { 0 }, true },
        { "rstrip",             "a<s>   b",         { "<s>" }, { ATTR_CONTROL | ATTR_RSTRIP }, { 0 }, true },
        { "both-strip",         "a  <s>  b",        { "<s>" }, { ATTR_CONTROL | ATTR_LSTRIP | ATTR_RSTRIP }, { 0 }, true },
        { "strip-to-empty",     "   <s>   ",        { "<s>" }, { ATTR_CONTROL | ATTR_LSTRIP | ATTR_RSTRIP }, { 0 }, true },
        { "two-tokens-order",   "x<b>y<a>z",        { "<a>", "<b>" }, { ATTR_CONTROL, ATTR_CONTROL }, { 1, 0 }, true },
        { "nested-order",       "<ab><a>",          { "<a>", "<ab>" }, { ATTR_CONTROL, ATTR_CONTROL }, { 1, 0 }, true },
        { "repeat-overlap",     "aaaa",             { "aa" }, { ATTR_CONTROL }, { 0 }, true },
        { "utf8",               "héllo<s>wörld",    { "<s>" }, { ATTR_CONTROL }, { 0 }, true },
        { "no-match",           "plain text",       { "<s>" }, { ATTR_CONTROL }, { 0 }, true },
        { "three-specials",     "<a><b><c>",        { "<a>", "<b>", "<c>" }, { ATTR_CONTROL, ATTR_CONTROL, ATTR_CONTROL }, { 0, 1, 2 }, true },
    };
    return cases;
}

template <typename result_t>
std::string render_native(const result_t & r) {
    std::string s;
    char buf[64];
    for (size_t i = 0; i < r.type.size(); ++i) {
        snprintf(buf, sizeof(buf), "%d:%d:%u:%u;", r.type[i], r.token[i],
                 r.offset[i], r.length[i]);
        s += buf;
    }
    return s;
}

void run_partition(int & cases, int & passed) {
    for (const auto & c : partition_cases()) {
        ++cases;

        stub_vocab_staged vs;
        for (size_t i = 0; i < c.tokens.size(); ++i) {
            token_text_staged t;
            t.text  = c.tokens[i];
            t.score = 0.0f;
            t.attr  = (int32_t) c.attrs[i];
            vs.tokens.push_back(t);
        }
        std::vector<llama_token> order;
        for (int id : c.order) {
            order.push_back((llama_token) id);
        }

        stub_vocab_base vb;
        for (size_t i = 0; i < c.tokens.size(); ++i) {
            token_text_base t;
            t.text  = c.tokens[i];
            t.score = 0.0f;
            t.attr  = (int32_t) c.attrs[i];
            vb.tokens.push_back(t);
        }

        const std::string got_base = render_native(
            native_partition_base(c.text, order, vb, c.parse_special));
        const std::string want = render_native(
            native_partition_staged(c.text, order, vs, c.parse_special));

#ifdef FG_GOOSE_TOKENIZER
        const int64_t n_tok = (int64_t) c.tokens.size();
        std::vector<int64_t> off(n_tok), len(n_tok), attr(n_tok);
        std::string text_bytes;
        for (int64_t i = 0; i < n_tok; ++i) {
            off[i]  = (int64_t) text_bytes.size();
            len[i]  = (int64_t) std::strlen(c.tokens[i]);
            attr[i] = c.attrs[i];
            text_bytes.append(c.tokens[i]);
        }
        std::vector<int64_t> ord(order.begin(), order.end());

        const int64_t text_len = (int64_t) std::strlen(c.text);
        const int64_t cap = 4 * (text_len + 2) + 16 * n_tok + 16;
        std::vector<int64_t> out_kind(cap), out_tok(cap), out_off(cap), out_len(cap);

        const int64_t n = fg_goose_st_partition(c.text, text_len, n_tok,
                                                off.data(), len.data(), text_bytes.data(),
                                                attr.data(), ord.data(), (int64_t) ord.size(),
                                                c.parse_special ? 1 : 0,
                                                out_kind.data(), out_tok.data(),
                                                out_off.data(), out_len.data(), cap);
        std::string got;
        char buf[64];
        for (int64_t i = 0; i < n; ++i) {
            snprintf(buf, sizeof(buf), "%d:%d:%lld:%lld;", (int) out_kind[i],
                     (int) (out_kind[i] == 0 ? out_tok[i] : -1),
                     (long long) (out_kind[i] == 0 ? 0 : out_off[i]),
                     (long long) (out_kind[i] == 0 ? 0 : out_len[i]));
            got += buf;
        }
#else
        const std::string got = got_base;
#endif

        const std::string what = std::string("part/") + c.name;
        const int before = g_failures;
        report(what.c_str(), got, want);
        if (g_failures == before) {
            ++passed;
        }
    }
}

// ---------------------------------------------------------------------------
// S3: string_strip
// ---------------------------------------------------------------------------

const std::vector<const char *> & strip_cases() {
    static const std::vector<const char *> cases = {
        "",
        " ",
        "   ",
        "a",
        "  a  ",
        " a b ",
        "\t\n\v\f\r a \t\n\v\f\r",
        "\n\nx\n\n",
        " x\ty ",
        "\t",
        "a  b",
        "  hello   world  ",
        " héllo ",
        "\r\nline\r\n",
        "no-space-at-all",
        " \ttwo\nwords\r ",
    };
    return cases;
}

void run_strip(int & cases, int & passed) {
    for (const char * c : strip_cases()) {
        ++cases;
        const std::string in(c);
        const std::string got  = string_strip(in);
        // Both arms compare against the reference extraction, not against "the input unchanged".
        // The OFF arm used to do the latter on the premise that the native body had been deleted;
        // nm on libllama-common.so shows `T string_strip` still defined, so that premise was false
        // and the arm reported 12 false failures (all leading/trailing whitespace). Comparing both
        // arms with the same reference is what makes the pairing meaningful: ON then proves the
        // Goose body equals the native text, OFF proves the native body does.
        const std::string want = native_strip_base(in);
        const std::string what = std::string("strip/") + (in.empty() ? "<empty>" : in);
        const int before = g_failures;
        report(what.c_str(), "[" + got + "]", "[" + want + "]");
        if (g_failures == before) {
            ++passed;
        }
    }
}

} // namespace

int main(int argc, char ** argv) {
    g_dump = argc > 1 && std::string(argv[1]) == "--dump";

#ifdef FG_GOOSE_TOKENIZER
    const char * mode = "goose";
#else
    const char * mode = "native";
#endif

    int vt_cases_n = 0, vt_passed = 0;
    int part_cases_n = 0, part_passed = 0;
    int strip_cases_n = 0, strip_passed = 0;

    run_vt(vt_cases_n, vt_passed);
    run_partition(part_cases_n, part_passed);
    run_strip(strip_cases_n, strip_passed);

#ifdef FG_GOOSE_TOKENIZER
    const uint64_t calls = fg_goose_tokenizer_calls();
    const uint64_t pins  = fg_goose_tokenizer_pins();
#else
    const uint64_t calls = 0;
    const uint64_t pins  = 0;
#endif

    const int total = vt_cases_n + part_cases_n + strip_cases_n;
    const int ok    = vt_passed + part_passed + strip_passed;

    printf("FG639_SMOKE mode=%s vt=%d/%d part=%d/%d strip=%d/%d cases=%d/%d failures=%d goose_calls=%llu pins=%llu\n",
           mode, vt_passed, vt_cases_n, part_passed, part_cases_n,
           strip_passed, strip_cases_n, ok, total, g_failures,
           (unsigned long long) calls, (unsigned long long) pins);

    return g_failures == 0 ? 0 : 1;
}
