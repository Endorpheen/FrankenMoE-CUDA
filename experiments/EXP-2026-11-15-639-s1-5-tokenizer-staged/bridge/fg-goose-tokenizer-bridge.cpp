// Staged bridge between llama.cpp and the Goose tokenizer decision unit.
//
// Compiled only with FG_GOOSE_TOKENIZER; see fg-goose-tokenizer-bridge.h for
// what it owns. The native bodies stay in llama-vocab.cpp and common.cpp.

#include "fg-goose-tokenizer-bridge.h"

#include "llama.h"
#include "llama-vocab.h"

#include "tokenizer-decisions.h"

#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <stdexcept>
#include <vector>

// The composed unit calls the json package's native strtod for the float parse
// in cs_kv. That native part is not linked here, so the host supplies it with
// the semantics the package expects: strtod over the slice, current locale.
extern "C" double gs_json_strtod(sl_u8 s);

extern "C" double gs_json_strtod(sl_u8 s) {
    const std::string t(reinterpret_cast<const char *>(s.data), (size_t) s.len);
    return strtod(t.c_str(), nullptr);
}

namespace {

std::atomic<uint64_t> g_calls{0};
std::atomic<uint64_t> g_pins{0};

void goose_start() {
    static std::once_flag once;
    std::call_once(once, [] { goose_init(0, nullptr); });
}

struct named_value {
    const char * name;
    int          value;
};

// The vocab-type names the module can emit, against the enums of this
// llama.cpp revision. A name the module knows but this tree does not
// ("TEST", "PLAMO3") is not in the table on purpose: the bridge refuses it
// with the native text, so switching the module on cannot accept a model the
// native loader rejected. The table is the only place a native renumbering
// would need attention.
const named_value k_vocab_types[] = {
    { "NONE",   LLAMA_VOCAB_TYPE_NONE   },
    { "SPM",    LLAMA_VOCAB_TYPE_SPM    },
    { "BPE",    LLAMA_VOCAB_TYPE_BPE    },
    { "WPM",    LLAMA_VOCAB_TYPE_WPM    },
    { "UGM",    LLAMA_VOCAB_TYPE_UGM    },
    { "RWKV",   LLAMA_VOCAB_TYPE_RWKV   },
    { "PLAMO2", LLAMA_VOCAB_TYPE_PLAMO2 },
};

// The pre-tokenizer names of this revision, in enum order.
const named_value k_pre_types[] = {
    { "DEFAULT",            0 },
    { "LLAMA3",             1 },
    { "DEEPSEEK_LLM",       2 },
    { "DEEPSEEK_CODER",     3 },
    { "FALCON",             4 },
    { "MPT",                5 },
    { "STARCODER",          6 },
    { "GPT2",               7 },
    { "REFACT",             8 },
    { "COMMAND_R",          9 },
    { "STABLELM2",         10 },
    { "QWEN2",             11 },
    { "OLMO",              12 },
    { "DBRX",              13 },
    { "SMAUG",             14 },
    { "PORO",              15 },
    { "CHATGLM3",          16 },
    { "CHATGLM4",          17 },
    { "VIKING",            18 },
    { "JAIS",              19 },
    { "TEKKEN",            20 },
    { "SMOLLM",            21 },
    { "CODESHELL",         22 },
    { "BLOOM",             23 },
    { "GPT3_FINNISH",      24 },
    { "EXAONE",            25 },
    { "CHAMELEON",         26 },
    { "MINERVA",           27 },
    { "DEEPSEEK3_LLM",     28 },
    { "GPT4O",             29 },
    { "SUPERBPE",          30 },
    { "TRILLION",          31 },
    { "BAILINGMOE",        32 },
    { "LLAMA4",            33 },
    { "PIXTRAL",           34 },
    { "SEED_CODER",        35 },
    { "HUNYUAN",           36 },
    { "KIMI_K2",           37 },
    { "HUNYUAN_DENSE",     38 },
    { "GROK_2",            39 },
    { "GRANITE_DOCLING",   40 },
    { "MINIMAX_M2",        41 },
    { "AFMOE",             42 },
    { "SOLAR_OPEN",        43 },
    { "YOUTU",             44 },
    { "EXAONE_MOE",        45 },
    { "QWEN35",            46 },
    { "TINY_AYA",          47 },
    { "JOYAI_LLM",         48 },
    { "JAIS2",             49 },
    { "GEMMA4",            50 },
    { "SARVAM_MOE",        51 },
    { "MINICPM5",          52 },
    { "WHITESPACE",        53 },
    { "GRANITE_EMB_MULTI", 54 },
    { "MELLUM2",           55 },
    { "LAGUNA",            56 },
};

template <size_t N>
bool lookup(const named_value (&table)[N], const std::string & name, int & out) {
    for (size_t i = 0; i < N; ++i) {
        if (name == table[i].name) {
            out = table[i].value;
            return true;
        }
    }
    return false;
}

// The module writes one canonical text record; the bridge reads it back field
// by field. The layout is fixed by vt_write in goose/vocab-type.goose and both
// sides are checked against each other by the gate, so a drift here fails the
// smoke run rather than the loader.
struct record_reader {
    const char * p;
    const char * end;
    bool         ok = true;

    void expect(const char * lit) {
        const size_t n = strlen(lit);
        if (!ok || (size_t) (end - p) < n || memcmp(p, lit, n) != 0) {
            ok = false;
            return;
        }
        p += n;
    }

    int64_t num() {
        if (!ok) {
            return 0;
        }
        bool neg = false;
        if (p < end && *p == '-') {
            neg = true;
            ++p;
        }
        if (p >= end || *p < '0' || *p > '9') {
            ok = false;
            return 0;
        }
        int64_t v = 0;
        while (p < end && *p >= '0' && *p <= '9') {
            v = v * 10 + (*p - '0');
            ++p;
        }
        return neg ? -v : v;
    }

    std::string word(char stop) {
        if (!ok) {
            return std::string();
        }
        const char * q = p;
        while (q < end && *q != stop) {
            ++q;
        }
        if (q == end) {
            ok = false;
            return std::string();
        }
        std::string s(p, q);
        p = q;
        return s;
    }

    std::string rest() {
        if (!ok) {
            return std::string();
        }
        std::string s(p, end);
        p = end;
        return s;
    }
};

} // namespace

int fg_goose_vocab_type_decide(const std::string & model,
                               const std::string & pre,
                               const fg_goose_vt_answers & answers,
                               fg_goose_vt_decision & out) {
    goose_start();
    g_calls.fetch_add(1, std::memory_order_relaxed);

    out = fg_goose_vt_decision();

    // The record is bounded by the two names, six ids, seven flags and the
    // message, which repeats one of the names.
    std::vector<char> buf(256 + 2 * model.size() + 2 * pre.size());

    // The module's slice type is not const-qualified: it only ever reads the
    // input slices, but the ABI cannot say so.
    sl_u8 m{ const_cast<uint8_t *>(reinterpret_cast<const uint8_t *>(model.data())), (int64_t) model.size() };
    sl_u8 p{ const_cast<uint8_t *>(reinterpret_cast<const uint8_t *>(pre.data())),   (int64_t) pre.size()   };
    sl_u8 o{ reinterpret_cast<uint8_t *>(buf.data()), (int64_t) buf.size() };

    const int64_t n = vt_dispatch(m, p,
                                  answers.has_merges,
                                  answers.merges_kv_is_array,
                                  answers.merges_arr_is_string,
                                  answers.n_merges,
                                  answers.charsmap_present,
                                  answers.charsmap_kv_is_array,
                                  answers.charsmap_is_i8_or_u8,
                                  answers.charsmap_len,
                                  (int64_t) answers.charsmap_blob,
                                  o);

    if (n < 0 || n > (int64_t) buf.size()) {
        out.status = 9;
        out.error  = "goose: vt_dispatch record does not fit its buffer";
        return 9;
    }

    record_reader r{ buf.data(), buf.data() + n };

    r.expect("st=");
    out.status = (int) r.num();
    r.expect(" type=");
    const std::string tname = r.word(' ');
    r.expect(" pre=");
    const std::string pname = r.word(' ');
    r.expect(" ids=");
    int ids[6] = { 0, 0, 0, 0, 0, 0 };
    for (int i = 0; i < 6 && r.ok; ++i) {
        if (i > 0) {
            r.expect(",");
        }
        ids[i] = (int) r.num();
    }
    r.expect(" fl=");
    int flags[7] = { 0, 0, 0, 0, 0, 0, 0 };
    for (int i = 0; i < 7 && r.ok; ++i) {
        if (i > 0) {
            r.expect(",");
        }
        flags[i] = (int) r.num();
    }
    r.expect(" lo=");
    const int lower = (int) r.num();
    r.expect(" nm=");
    const int64_t nm = r.num();
    r.expect(" err=");

    out.error = r.rest();

    if (!r.ok) {
        out.status = 9;
        out.error  = "goose: vt_dispatch record is malformed";
        return 9;
    }

    out.bos  = ids[0];
    out.eos  = ids[1];
    out.unk  = ids[2];
    out.sep  = ids[3];
    out.pad  = ids[4];
    out.mask = ids[5];

    out.add_space_prefix   = flags[0];
    out.escape_whitespaces = flags[1];
    out.clean_spaces       = flags[2];
    out.ignore_merges      = flags[3];
    out.add_bos            = flags[4];
    out.add_eos            = flags[5];
    out.add_sep            = flags[6];

    out.lower_case  = lower;
    out.merge_count = nm;

    if (out.status != 0) {
        // The native merges message ends with a newline, the module writes the
        // sentence alone; the loader's refusal text must not change.
        if (out.status == 3) {
            out.error += "\n";
        }
        return out.status;
    }

    if (!lookup(k_vocab_types, tname, out.type)) {
        out.status = 1;
        out.error  = "unknown tokenizer: '" + model + "'";
        return out.status;
    }

    if (!lookup(k_pre_types, pname, out.pre_type)) {
        out.status = 2;
        out.error  = "unknown pre-tokenizer type: '" + pre + "'";
        return out.status;
    }

    // The native block overwrites the pre-tokenizer name for the gemma4 model
    // before the chain runs; the module reports the resulting pre type only.
    if (model == "gemma4") {
        out.pre_out = "gemma4";
    }

    // Revision pins: decisions where the canon module is one upstream revision
    // ahead of this llama.cpp. The module sets ignore_merges for glm4/glm5;
    // this tree groups glm4 with chatglm-bpe and leaves ignore_merges false.
    // Switching the module on must not change this tree's tokenization, so the
    // tree's value wins until the runtime moves to the canon revision. Each pin
    // bumps a counter the gate prints, so the delta stays visible.
    if (pre == "glm4" || pre == "glm5") {
        out.ignore_merges = 0;
        g_pins.fetch_add(1, std::memory_order_relaxed);
    }

    return 0;
}

int64_t fg_goose_st_partition(const char * text, int64_t text_len,
                              int64_t n_tok,
                              const int64_t * tok_off, const int64_t * tok_len,
                              const char * tok_text, const int64_t * tok_attr,
                              const int64_t * order, int64_t n_order,
                              int64_t parse_special,
                              int64_t * out_kind, int64_t * out_tok,
                              int64_t * out_off, int64_t * out_len,
                              int64_t out_cap) {
    goose_start();
    g_calls.fetch_add(1, std::memory_order_relaxed);

    // Read-only input slices, same qualifier note as above.
    sl_u8 t{ const_cast<uint8_t *>(reinterpret_cast<const uint8_t *>(text)), text_len };
    sl_i64 off { const_cast<int64_t *>(tok_off),  n_tok   };
    sl_i64 len { const_cast<int64_t *>(tok_len),  n_tok   };
    sl_u8  txt { const_cast<uint8_t *>(reinterpret_cast<const uint8_t *>(tok_text)),
                 n_tok > 0 ? tok_off[n_tok - 1] + tok_len[n_tok - 1] : 0 };
    sl_i64 attr{ const_cast<int64_t *>(tok_attr), n_tok   };
    sl_i64 ord { const_cast<int64_t *>(order),    n_order };

    sl_i64 okind{ out_kind, out_cap };
    sl_i64 otok { out_tok,  out_cap };
    sl_i64 ooff { out_off,  out_cap };
    sl_i64 olen { out_len,  out_cap };

    const int64_t n = stp_partition(t, n_tok, off, len, txt, attr, ord, parse_special,
                                    okind, otok, ooff, olen);

    return (n >= 0 && n <= out_cap) ? n : -1;
}

int64_t fg_goose_cs_strip(const char * s, int64_t n, char * out, int64_t cap) {
    goose_start();
    g_calls.fetch_add(1, std::memory_order_relaxed);

    sl_u8 in{ const_cast<uint8_t *>(reinterpret_cast<const uint8_t *>(s)), n };
    sl_u8 o { reinterpret_cast<uint8_t *>(out), cap };

    const int64_t r = cs_strip(in, o);

    return (r >= 0 && r <= cap) ? r : -1;
}

uint64_t fg_goose_tokenizer_calls() {
    return g_calls.load(std::memory_order_relaxed);
}

uint64_t fg_goose_tokenizer_pins() {
    return g_pins.load(std::memory_order_relaxed);
}
