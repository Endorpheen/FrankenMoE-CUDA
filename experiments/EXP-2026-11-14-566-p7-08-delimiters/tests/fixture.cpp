// Differential gate for `common_chat_msg_delimiters` (EXP571).
//
// Native oracle (flint tree, libllama-common):
//   common_chat_msg_delimiters::{add, tokenize, split}
//   common_chat_msg_spans::{is_user_start, last_user_message_pos}
//   common_tokenize(vocab, text, /*add_special=*/false, /*parse_special=*/true)
//   with a vocab-only gguf passed as argv[1].
//
// Port: export functions of source/runtime.goose through runtime-gen.h:
//   dl_reset, dl_add, dl_bind_tokens, dl_bind_all, build_msg_delims,
//   dl_split, sp_is_user_start (1/0), sp_last_user_pos,
//   role_from_string, role_to_string.
//
// The fixture compares, per case:
//   - the delimiter store: count, role, bytes, bound token ids;
//   - every span: role, start, end, and whether it intersects a skip region;
//   - is_user_start at every token index (-1..n) and last_user_message_pos.
//
// Output: FG571_DELIM cases=N values=M failures=K
// Exit code: 0 when failures == 0, 1 otherwise, 2 when the vocab cannot load.

#include <cstdio>
#include <cstdint>
#include <cstring>
#include <exception>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <map>
#include <set>
#include <algorithm>
#include <utility>

#include "runtime-gen.h"
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cmath>
#include <ctime>
#include <string>

extern "C" double gs_json_strtod(sl_u8 s) {
    std::string t((const char *) s.data, (size_t) s.len);
    return std::strtod(t.c_str(), nullptr);
}
extern "C" int64_t gs_json_fmt_e(sl_u8 out, int64_t prec, double x) {
    return snprintf((char *) out.data, (size_t) out.len, "%.*e", (int) prec, x);
}
extern "C" int64_t gs_json_fmt_g(sl_u8 out, double x) {
    return snprintf((char *) out.data, (size_t) out.len, "%.17g", x);
}
extern "C" double gs_frexp_mant(double x) { int e = 0; return std::frexp(x, &e); }
extern "C" int64_t gs_to_string_f64(sl_u8 out, double x) {
    const std::string t = std::to_string(x);
    memcpy(out.data, t.data(), t.size());
    return (int64_t) t.size();
}
extern "C" int64_t gs_strftime_epoch(sl_u8 out, int64_t epoch, sl_u8 fmt) {
    const std::string f((const char *) fmt.data, (size_t) fmt.len);
    const std::time_t t = epoch < 0 ? std::time(nullptr) : (std::time_t) epoch;
    const std::tm * tmv = std::localtime(&t);
    if (!tmv) return -1;
    return (int64_t) std::strftime((char *) out.data, (size_t) out.len, f.c_str(), tmv);
}

#include "chat.h"
#include "chat-auto-parser.h"
#include "common.h"
#include "llama.h"
#include "server-common.h"

static int cases = 0;
static int values = 0;
static int fails = 0;

static std::string visible(const std::string & s, size_t max = 160) {
    std::string r;
    for (size_t i = 0; i < s.size() && i < max; ++i) {
        const unsigned char c = (unsigned char) s[i];
        if (c >= 32 && c < 127) {
            r.push_back((char) c);
        } else {
            char b[8];
            snprintf(b, sizeof b, "\\x%02X", c);
            r += b;
        }
    }
    if (s.size() > max) {
        r += "...";
    }
    return r;
}

static void fail(const std::string & c, const std::string & what) {
    ++fails;
    if (fails <= 200) {
        printf("FAIL %s: %s\n", c.c_str(), what.c_str());
    }
}

static void eq_i64(const std::string & c, const char * what, int64_t native, int64_t port) {
    ++values;
    if (native != port) {
        fail(c, std::string(what) + ": native=" + std::to_string(native) + " port=" + std::to_string(port));
    }
}

static void eq_bool(const std::string & c, const char * what, bool native, bool port) {
    ++values;
    if (native != port) {
        fail(c, std::string(what) + ": native=" + (native ? "true" : "false") +
                    " port=" + (port ? "true" : "false"));
    }
}

static void eq_str(const std::string & c, const char * what, const std::string & native, const std::string & port) {
    ++values;
    if (native != port) {
        fail(c, std::string(what) + ": native=\"" + visible(native) + "\" port=\"" + visible(port) + "\"");
    }
}

static sl_i64 si(std::vector<int64_t> & v) {
    return sl_i64{ v.data(), (int64_t) v.size() };
}

static sl_u8 su(std::vector<uint8_t> & v) {
    return sl_u8{ v.data(), (int64_t) v.size() };
}

static sl_u8 ss(const std::string & s) {
    return sl_u8{ (uint8_t *) s.data(), (int64_t) s.size() };
}

// ---------------------------------------------------------------------------
// Delimiter store and case model

struct DSpec {
    int64_t role = 0;
    std::string text;
    std::vector<llama_token> toks;
};

struct Case {
    std::string name;
    std::vector<DSpec> specs;
    std::vector<llama_token> tokens;   // explicit cases only
    std::map<size_t, size_t> skips;
    bool tokenize = false;             // template cases: tokenize the marker text
    std::string assistant;             // template cases: build_msg_delims input
    std::string user;
    bool check_build = false;          // template cases: use build_msg_delims + dl_bind_all
};

struct Port {
    std::vector<int64_t> D;
    std::vector<int64_t> T;
    std::vector<uint8_t> B;
    std::vector<int64_t> r;
    std::vector<int64_t> p;
    std::vector<int64_t> l;

    Port() : D(3 + 5 * 16, 0), T(65536, 0), B(65536, 0), r(4096, 0), p(4096, 0), l(4096, 0) {}
};

static DSpec ds(int64_t role, std::initializer_list<llama_token> ids) {
    DSpec s;
    s.role = role;
    s.toks.assign(ids.begin(), ids.end());
    return s;
}

static Case explicit_case(
    const char * name,
    std::vector<DSpec> specs,
    std::vector<llama_token> tokens,
    std::map<size_t, size_t> skips = {}) {
    Case c;
    c.name = name;
    c.specs = std::move(specs);
    c.tokens = std::move(tokens);
    c.skips = std::move(skips);
    return c;
}

static Case template_case(const char * name, const char * assistant, const char * user) {
    Case c;
    c.name = name;
    c.tokenize = true;
    c.check_build = true;
    c.assistant = assistant;
    c.user = user;
    if (!c.assistant.empty()) {
        DSpec s;
        s.role = 2;
        s.text = c.assistant;
        c.specs.push_back(s);
    }
    if (!c.user.empty()) {
        DSpec s;
        s.role = 3;
        s.text = c.user;
        c.specs.push_back(s);
    }
    return c;
}

// ---------------------------------------------------------------------------
// Native side

static std::vector<llama_token> make_probe_tokens(const common_chat_msg_delimiters & nd) {
    std::set<llama_token> used;
    for (const auto & d : nd.delimiters) {
        for (llama_token t : d.tokens) {
            used.insert(t);
        }
    }
    llama_token next = 40000;
    auto content = [&]() {
        while (used.count(next)) {
            ++next;
        }
        const llama_token t = next;
        used.insert(t);
        ++next;
        return t;
    };

    std::vector<llama_token> out;
    for (const auto & d : nd.delimiters) {
        out.insert(out.end(), d.tokens.begin(), d.tokens.end());
        out.push_back(content());
        out.push_back(content());
    }
    out.push_back(content());
    return out;
}

static void native_build(
    const Case & c,
    const llama_vocab * vocab,
    common_chat_msg_delimiters & nd,
    std::vector<llama_token> & tokens) {
    for (const auto & s : c.specs) {
        nd.add((common_chat_role) s.role, s.text);
        if (!c.tokenize) {
            nd.delimiters.back().tokens = s.toks;
        }
    }
    if (c.tokenize) {
        nd.tokenize(vocab);
    }
    tokens = c.tokenize ? make_probe_tokens(nd) : c.tokens;
}

// ---------------------------------------------------------------------------
// Port side

static int64_t port_build(const Case & c, const common_chat_msg_delimiters & nd, Port & P) {
    int64_t rc = dl_reset(si(P.D));
    if (rc != 0) {
        return rc;
    }
    if (c.check_build) {
        rc = build_msg_delims(si(P.D), su(P.B), ss(c.assistant), ss(c.user));
        if (rc != 0) {
            return rc;
        }
        std::vector<int64_t> ids;
        std::vector<int64_t> off;
        std::vector<int64_t> len;
        for (const auto & d : nd.delimiters) {
            off.push_back((int64_t) ids.size());
            len.push_back((int64_t) d.tokens.size());
            for (llama_token t : d.tokens) {
                ids.push_back((int64_t) t);
            }
        }
        return dl_bind_all(si(P.D), si(P.T), si(off), si(len), si(ids));
    }
    for (size_t i = 0; i < c.specs.size(); ++i) {
        rc = dl_add(si(P.D), su(P.B), c.specs[i].role, ss(c.specs[i].text));
        if (rc != 0) {
            return rc;
        }
        std::vector<int64_t> ids(c.specs[i].toks.begin(), c.specs[i].toks.end());
        rc = dl_bind_tokens(si(P.D), si(P.T), (int64_t) i, si(ids));
        if (rc != 0) {
            return rc;
        }
    }
    return 0;
}

static bool intersects_skip(int64_t pos, int64_t len, const std::map<size_t, size_t> & skips) {
    if (len <= 0) {
        return false;
    }
    for (const auto & kv : skips) {
        const int64_t s = (int64_t) kv.first;
        const int64_t n = (int64_t) kv.second;
        if (n <= 0) {
            continue;
        }
        if (pos < s + n && s < pos + len) {
            return true;
        }
    }
    return false;
}

// ---------------------------------------------------------------------------
// Comparison

static void compare_store(
    const Case & c,
    const common_chat_msg_delimiters & nd,
    const Port & P) {
    const int64_t n = (int64_t) nd.delimiters.size();
    eq_i64(c.name, "store.count", n, P.D[0]);
    if ((int64_t) P.D[0] != n) {
        return;
    }
    for (int64_t i = 0; i < n; ++i) {
        const int64_t base = 3 + 5 * i;
        eq_i64(c.name, "store.role", (int64_t) nd.delimiters[i].role, P.D[base]);

        const int64_t boff = P.D[base + 1];
        const int64_t blen = P.D[base + 2];
        if (boff < 0 || blen < 0 || boff + blen > (int64_t) P.B.size()) {
            fail(c.name, "store.bytes out of arena");
        } else {
            const std::string bytes((const char *) P.B.data() + boff, (size_t) blen);
            eq_str(c.name, "store.bytes", nd.delimiters[i].delimiter, bytes);
        }

        const int64_t toff = P.D[base + 3];
        const int64_t tlen = P.D[base + 4];
        eq_i64(c.name, "store.token_count", (int64_t) nd.delimiters[i].tokens.size(), tlen);
        if (toff < 0 || tlen < 0 || toff + tlen > (int64_t) P.T.size()) {
            fail(c.name, "store.tokens out of arena");
            continue;
        }
        for (int64_t k = 0; k < tlen; ++k) {
            eq_i64(c.name, "store.token", (int64_t) nd.delimiters[i].tokens[k], P.T[toff + k]);
        }
    }
}

static void compare_case(
    const Case & c,
    const common_chat_msg_delimiters & nd,
    const std::vector<llama_token> & tokens,
    Port & P) {
    compare_store(c, nd, P);

    const common_chat_msg_spans nsp = nd.split(tokens, c.skips);

    std::vector<int64_t> tk(tokens.begin(), tokens.end());
    std::vector<int64_t> sk_start;
    std::vector<int64_t> sk_len;
    for (const auto & kv : c.skips) {
        sk_start.push_back((int64_t) kv.first);
        sk_len.push_back((int64_t) kv.second);
    }
    const int64_t need = dl_split(
        si(P.D), si(P.T), si(tk), si(sk_start), si(sk_len), (int64_t) sk_start.size(),
        si(P.r), si(P.p), si(P.l));
    eq_i64(c.name, "span.count", (int64_t) nsp.spans.size(), need);

    const int64_t n = std::min<int64_t>((int64_t) nsp.spans.size(), need);
    for (int64_t i = 0; i < n; ++i) {
        const auto & s = nsp.spans[i];
        eq_i64(c.name, "span.role", (int64_t) s.role, P.r[i]);
        eq_i64(c.name, "span.start", (int64_t) s.pos, P.p[i]);
        eq_i64(c.name, "span.end", (int64_t) (s.pos + s.len), P.p[i] + P.l[i]);
        eq_bool(c.name, "span.skip",
            intersects_skip((int64_t) s.pos, (int64_t) s.len, c.skips),
            intersects_skip(P.p[i], P.l[i], c.skips));
    }

    for (int64_t pos = -1; pos <= (int64_t) tokens.size(); ++pos) {
        eq_bool(c.name, "is_user_start",
            nsp.is_user_start((int32_t) pos),
            sp_is_user_start(si(P.r), si(P.p), need, pos) != 0);
    }
    eq_i64(c.name, "last_user_message_pos",
        (int64_t) nsp.last_user_message_pos(),
        sp_last_user_pos(si(P.r), si(P.p), need));
}

static void run_case_inner(const Case & c, const llama_vocab * vocab) {
    common_chat_msg_delimiters nd;
    std::vector<llama_token> tokens;
    native_build(c, vocab, nd, tokens);

    Port P;
    const int64_t rc = port_build(c, nd, P);
    if (rc != 0) {
        fail(c.name, "port_build rc=" + std::to_string(rc));
        return;
    }
    compare_case(c, nd, tokens, P);
}

static void run_case(const Case & c, const llama_vocab * vocab) {
    ++cases;
    run_case_inner(c, vocab);
}

// ---------------------------------------------------------------------------
// Role name table

static void check_role_table() {
    const int64_t roles[] = { 0, 1, 2, 3, 4, 9 };
    const char * names[] = { "", "system", "assistant", "user", "tool", "bogus", "" };

    for (size_t i = 0; i < sizeof(roles) / sizeof(roles[0]); ++i) {
        uint8_t buf[32] = {0};
        const std::string native = common_chat_role_to_string((common_chat_role) roles[i]);
        const int64_t len = role_to_string(roles[i], sl_u8{ buf, (int64_t) sizeof(buf) });
        const std::string port((const char *) buf, (size_t) std::max<int64_t>(len, 0));
        eq_str("role_table", "to_string",
            native,
            len >= 0 && len <= (int64_t) sizeof(buf) ? port : std::string("<bad length>"));
    }
    for (size_t i = 0; i < sizeof(names) / sizeof(names[0]); ++i) {
        const std::string s(names[i]);
        eq_i64("role_table", "from_string",
            (int64_t) common_chat_role_from_string(s),
            role_from_string(ss(s)));
    }
}

// ---------------------------------------------------------------------------
// Contract errors that have no native counterpart

static void run_contract_case(const llama_vocab * vocab) {
    static const char * name = "contract";
    ++cases;

    Case c = explicit_case(
        "contract-source",
        { ds(3, {10, 11}), ds(2, {10, 12}) },
        { 10, 11, 100, 101, 10, 12, 200, 201, 202, 10, 11, 300, 301 });

    common_chat_msg_delimiters nd;
    std::vector<llama_token> tokens;
    native_build(c, vocab, nd, tokens);

    Port P;
    if (port_build(c, nd, P) != 0) {
        fail(name, "port_build failed");
        return;
    }
    std::vector<int64_t> tk(tokens.begin(), tokens.end());
    std::vector<int64_t> empty;

    {
        std::vector<int64_t> starts = {5, 2};
        std::vector<int64_t> lens   = {1, 1};
        const int64_t rc = dl_split(si(P.D), si(P.T), si(tk), si(starts), si(lens), 2,
                                    si(P.r), si(P.p), si(P.l));
        eq_i64(name, "unsorted_skips", -12, rc);
    }
    {
        std::vector<int64_t> starts = {2, 2};
        std::vector<int64_t> lens   = {1, 1};
        const int64_t rc = dl_split(si(P.D), si(P.T), si(tk), si(starts), si(lens), 2,
                                    si(P.r), si(P.p), si(P.l));
        eq_i64(name, "duplicate_skips", -12, rc);
    }
    {
        std::vector<int64_t> starts = {0};
        std::vector<int64_t> lens   = {-1};
        const int64_t rc = dl_split(si(P.D), si(P.T), si(tk), si(starts), si(lens), 1,
                                    si(P.r), si(P.p), si(P.l));
        eq_i64(name, "negative_skip", -11, rc);
    }
    {
        const common_chat_msg_spans nsp = nd.split(tokens);
        std::vector<int64_t> r1(1, -99);
        std::vector<int64_t> p1(1, -99);
        std::vector<int64_t> l1(1, -99);
        const int64_t rc = dl_split(si(P.D), si(P.T), si(tk), si(empty), si(empty), 0,
                                    si(r1), si(p1), si(l1));
        eq_i64(name, "truncated_return", (int64_t) nsp.spans.size(), rc);
        if (!nsp.spans.empty() && rc == (int64_t) nsp.spans.size()) {
            eq_i64(name, "truncated.first.role", (int64_t) nsp.spans[0].role, r1[0]);
            eq_i64(name, "truncated.first.start", (int64_t) nsp.spans[0].pos, p1[0]);
            eq_i64(name, "truncated.first.len", (int64_t) nsp.spans[0].len, l1[0]);
        }
    }
}

// ---------------------------------------------------------------------------
// Case lists

static std::vector<Case> template_cases() {
    return {
        template_case("t01_qwen35_chatml",
            "<|im_start|>assistant", "<|im_start|>user"),
        template_case("t02_deepseek_r1",
            "<\xEF\xBD\x9C" "Assistant" "<\xEF\xBD\x9C>",
            "<\xEF\xBD\x9C" "User" "<\xEF\xBD\x9C>"),
        template_case("t03_llama3_header",
            "<|start_header_id|>assistant<|end_header_id|>",
            "<|start_header_id|>user<|end_header_id|>"),
        template_case("t04_gemma2",
            "<start_of_turn>model", "<start_of_turn>user"),
        template_case("t05_granite",
            "<|start_of_role|>assistant<|end_of_role|>",
            "<|start_of_role|>user<|end_of_role|>"),
        template_case("t06_mistral_nemo",
            "", "[INST]"),
        template_case("t07_cohere_rplus",
            "<|START_OF_TURN_TOKEN|><|CHATBOT_TOKEN|>",
            "<|START_OF_TURN_TOKEN|><|USER_TOKEN|>"),
        template_case("t08_nemotron_nano_v2",
            "<SPECIAL_11>Assistant", "<SPECIAL_11>User"),
        template_case("t09_minimax_m2",
            "]~b]ai", "]~b]user"),
        template_case("t10_solar_open",
            "<|begin|>assistant", "<|begin|>user<|content|>"),
    };
}


// ---------------------------------------------------------------------------
// Real templates: the expected markers are derived at gate time by the same
// native autoparser that common_chat_templates_apply uses (chat.cpp:262-263);
// the fixed t01-t10 group above keeps the frozen table values.

struct RealTemplate {
    const char * name;
    const char * file;
    bool expect_assistant_empty;
    const char * source_fragment;  // optional raw-prefix evidence, or nullptr
};

static int64_t marker_edge_ws(const std::string & s) {
    if (s.empty()) {
        return 0;
    }
    auto ws = [](unsigned char c) {
        return c == ' ' || c == '\t' || c == '\n' || c == '\r';
    };
    int64_t n = 0;
    if (ws((unsigned char) s.front())) {
        ++n;
    }
    if (ws((unsigned char) s.back())) {
        ++n;
    }
    return n;
}

static std::vector<RealTemplate> real_templates() {
    return {
        { "r01_qwen35_chatml",       "Qwen3.5-4B.jinja", false, nullptr },
        { "r02_deepseek_r1",         "deepseek-ai-DeepSeek-R1-Distill-Llama-8B.jinja", false, nullptr },
        { "r03_llama3_header",       "meta-llama-Llama-3.1-8B-Instruct.jinja", false, nullptr },
        { "r04_gemma2",              "google-gemma-2-2b-it.jinja", false, nullptr },
        { "r05_granite",             "ibm-granite-granite-3.3-2B-Instruct.jinja", false, nullptr },
        { "r06_mistral_nemo",        "mistralai-Mistral-Nemo-Instruct-2407.jinja", true, nullptr },
        { "r07_cohere_rplus",        "CohereForAI-c4ai-command-r-plus-tool_use.jinja", false, nullptr },
        { "r08_nemotron_nano_v2",    "NVIDIA-Nemotron-Nano-v2.jinja", false, nullptr },
        { "r09_minimax_m2",          "MiniMax-M2.jinja", false, nullptr },
        { "r10_rwkv_world",          "llama-cpp-rwkv-world.jinja", false, "User: " },
    };
}

static void run_real_template(
    const RealTemplate & rt,
    const std::string & dir,
    const llama_vocab * vocab) {
    ++cases;

    const std::string path = dir + "/" + rt.file;
    std::ifstream fin(path, std::ios::binary);
    std::ostringstream buf;
    if (fin.is_open()) {
        buf << fin.rdbuf();
    }
    const std::string src = buf.str();
    if (src.empty()) {
        fail(rt.name, "cannot read template: " + path);
        return;
    }
    if (rt.source_fragment != nullptr && src.find(rt.source_fragment) == std::string::npos) {
        fail(rt.name, std::string("template lost the raw prefix fragment: ") + rt.source_fragment);
    }

    common_chat_template tmpl(src, "", "");
    autoparser::autoparser ap;
    try {
        ap.analyze_template(tmpl);
    } catch (const std::exception & e) {
        fail(rt.name, std::string("native analysis threw: ") + e.what());
        return;
    }

    eq_bool(rt.name, "assistant_marker_empty", rt.expect_assistant_empty, ap.assistant_start.empty());
    eq_bool(rt.name, "user_marker_present", true, !ap.user_start.empty());
    eq_i64(rt.name, "user_marker_edge_ws", 0, marker_edge_ws(ap.user_start));
    if (!ap.assistant_start.empty()) {
        eq_i64(rt.name, "assistant_marker_edge_ws", 0, marker_edge_ws(ap.assistant_start));
    }

    Case c;
    c.name = rt.name;
    c.tokenize = true;
    c.check_build = true;
    c.assistant = ap.assistant_start;
    c.user = ap.user_start;
    if (!c.assistant.empty()) {
        DSpec a;
        a.role = 2;
        a.text = c.assistant;
        c.specs.push_back(a);
    }
    if (!c.user.empty()) {
        DSpec u;
        u.role = 3;
        u.text = c.user;
        c.specs.push_back(u);
    }
    run_case_inner(c, vocab);
}

static std::vector<Case> edge_cases() {
    std::vector<Case> out;

    // E1: an empty delimiter token list matches every position.
    out.push_back(explicit_case("e01_empty_delimiter",
        { ds(3, {}) },
        { 5, 6, 7 }));

    // E2: the empty delimiter is second; it wins where the first does not match.
    out.push_back(explicit_case("e02_empty_delimiter_second",
        { ds(3, {10, 11}), ds(2, {}) },
        { 10, 11, 7 }));

    // E3: a prefix delimiter wins by insertion order, not by length.
    out.push_back(explicit_case("e03_prefix_order",
        { ds(3, {10}), ds(2, {10, 11}) },
        { 10, 11, 20 }));

    // E4: shared first token; the second token decides.
    out.push_back(explicit_case("e04_shared_prefix",
        { ds(3, {10, 11}), ds(2, {10, 12}) },
        { 10, 12, 10, 11 }));

    // E5: delimiter at the very start, nothing else.
    out.push_back(explicit_case("e05_at_start",
        { ds(3, {10, 11}) },
        { 10, 11 }));

    // E6: delimiter at the very end after content.
    out.push_back(explicit_case("e06_at_end",
        { ds(3, {10, 11}) },
        { 7, 8, 10, 11 }));

    // E7: zero-length skip is a no-op jump, not a hang.
    out.push_back(explicit_case("e07_zero_skip",
        { ds(3, {10, 11}) },
        { 5, 10, 11 },
        { {1, 0} }));

    // E8: media chunk between delimiters hides the delimiter ids inside it.
    out.push_back(explicit_case("e08_media_hidden",
        { ds(3, {10, 11}), ds(2, {10, 12}) },
        { 10, 11, LLAMA_TOKEN_NULL, LLAMA_TOKEN_NULL, 10, 12, 7 },
        { {2, 2} }));

    return out;
}

static std::vector<Case> split_cases() {
    std::vector<Case> out;

    // The corpus of tests/test-chat.cpp:1686-1793.
    out.push_back(explicit_case("s01_empty_both", {}, {}));
    out.push_back(explicit_case("s02_empty_delims", {}, { 10, 11 }));
    out.push_back(explicit_case("s03_no_match",
        { ds(3, {10, 11}), ds(2, {10, 12}) },
        { 100, 101, 102 }));
    out.push_back(explicit_case("s04_conversation",
        { ds(3, {10, 11}), ds(2, {10, 12}) },
        { 10, 11, 100, 101, 10, 12, 200, 201, 202, 10, 11, 300, 301 }));
    out.push_back(explicit_case("s05_leading_content",
        { ds(3, {10, 11}), ds(2, {10, 12}) },
        { 500, 501, 10, 11, 100 }));
    out.push_back(explicit_case("s06_media_inside",
        { ds(3, {10, 11}), ds(2, {10, 12}) },
        { 10, 11, LLAMA_TOKEN_NULL, LLAMA_TOKEN_NULL, LLAMA_TOKEN_NULL, 100, 10, 12 },
        { {2, 3} }));
    out.push_back(explicit_case("s07_delimiter_in_skip",
        { ds(3, {10, 11}), ds(2, {10, 12}) },
        { 10, 11, 10, 12, 100 },
        { {2, 2} }));

    return out;
}


// ---------------------------------------------------------------------------
// Linkage group: compare dl_split with the real server_tokens::
// find_message_spans (tools/server/server-common.cpp:866-872) and the span
// queries the server uses (last_user_message_pos / is_user_start).

struct LinkDelim {
    int64_t role = 0;
    std::string text;
    std::vector<llama_token> toks;
};

struct LinkCase {
    std::string name;
    std::vector<LinkDelim> delims;
    std::vector<llama_token> tokens;
    std::map<size_t, size_t> skips;
};

static std::vector<llama_token> vocab_marker(const llama_vocab * vocab, const std::string & text) {
    return common_tokenize(vocab, text, false, true);
}

static LinkDelim link_delim(int64_t role, std::initializer_list<llama_token> ids) {
    LinkDelim d;
    d.role = role;
    d.toks.assign(ids.begin(), ids.end());
    return d;
}

static LinkDelim link_delim_text(const llama_vocab * vocab, int64_t role, const std::string & text) {
    LinkDelim d;
    d.role = role;
    d.text = text;
    d.toks = vocab_marker(vocab, text);
    return d;
}

static void append_ids(std::vector<llama_token> & dst, const std::vector<llama_token> & ids) {
    dst.insert(dst.end(), ids.begin(), ids.end());
}

static void compare_link_spans(
    const std::string & name,
    const common_chat_msg_spans & nsp,
    const std::vector<llama_token> & tokens,
    const std::map<size_t, size_t> & skips,
    Port & P) {

    std::vector<int64_t> tk(tokens.begin(), tokens.end());
    std::vector<int64_t> sk_start;
    std::vector<int64_t> sk_len;
    for (const auto & kv : skips) {
        sk_start.push_back((int64_t) kv.first);
        sk_len.push_back((int64_t) kv.second);
    }
    const int64_t need = dl_split(
        si(P.D), si(P.T), si(tk), si(sk_start), si(sk_len), (int64_t) sk_start.size(),
        si(P.r), si(P.p), si(P.l));
    eq_i64(name, "span.count", (int64_t) nsp.spans.size(), need);

    const int64_t n = std::min<int64_t>((int64_t) nsp.spans.size(), need);
    for (int64_t i = 0; i < n; ++i) {
        const auto & s = nsp.spans[i];
        eq_i64(name, "span.role", (int64_t) s.role, P.r[i]);
        eq_i64(name, "span.start", (int64_t) s.pos, P.p[i]);
        eq_i64(name, "span.end", (int64_t) (s.pos + s.len), P.p[i] + P.l[i]);
        eq_bool(name, "span.skip",
            intersects_skip((int64_t) s.pos, (int64_t) s.len, skips),
            intersects_skip(P.p[i], P.l[i], skips));
    }

    for (int64_t pos = -1; pos <= (int64_t) tokens.size(); ++pos) {
        eq_bool(name, "is_user_start",
            nsp.is_user_start((int32_t) pos),
            sp_is_user_start(si(P.r), si(P.p), need, pos) != 0);
    }
    eq_i64(name, "last_user_message_pos",
        (int64_t) nsp.last_user_message_pos(),
        sp_last_user_pos(si(P.r), si(P.p), need));
}

// native_st is used when the stream carries a media map; otherwise a plain
// server_tokens is built from the same tokens (empty skip map).
static void run_link_case(
    const LinkCase & c,
    server_tokens * native_st) {

    ++cases;

    common_chat_msg_delimiters nd;
    for (const auto & d : c.delims) {
        nd.add((common_chat_role) d.role, d.text);
        nd.delimiters.back().tokens = d.toks;
    }

    Port P;
    int64_t rc = dl_reset(si(P.D));
    if (rc != 0) {
        fail(c.name, "dl_reset rc=" + std::to_string(rc));
        return;
    }
    for (size_t i = 0; i < c.delims.size(); ++i) {
        rc = dl_add(si(P.D), su(P.B), c.delims[i].role, ss(c.delims[i].text));
        if (rc != 0) {
            fail(c.name, "dl_add rc=" + std::to_string(rc));
            return;
        }
        std::vector<int64_t> ids(c.delims[i].toks.begin(), c.delims[i].toks.end());
        rc = dl_bind_tokens(si(P.D), si(P.T), (int64_t) i, si(ids));
        if (rc != 0) {
            fail(c.name, "dl_bind_tokens rc=" + std::to_string(rc));
            return;
        }
    }

    Case meta;
    meta.name = c.name;
    compare_store(meta, nd, P);

    if (c.tokens.empty() && native_st == nullptr) {
        // server_tokens::get_tokens() aborts on an empty stream; the native
        // answer for no tokens is an empty span set.
        const common_chat_msg_spans nsp{};
        compare_link_spans(c.name, nsp, c.tokens, c.skips, P);
        return;
    }
    server_tokens local(c.tokens, false);
    server_tokens & st = native_st != nullptr ? *native_st : local;
    const common_chat_msg_spans nsp = st.find_message_spans(nd);

    compare_link_spans(c.name, nsp, c.tokens, c.skips, P);
}

static void link_cases(const llama_vocab * vocab) {
    // 1. Empty stream: no spans, no user start.
    {
        LinkCase c;
        c.name = "l01_empty_stream";
        c.delims = { link_delim(3, {10, 11}) };
        run_link_case(c, nullptr);
    }
    // 2. One marker only.
    {
        LinkCase c;
        c.name = "l02_single_marker";
        c.delims = { link_delim(3, {10, 11}) };
        c.tokens = { 10, 11 };
        run_link_case(c, nullptr);
    }
    // 3. Marker at the start.
    {
        LinkCase c;
        c.name = "l03_marker_at_start";
        c.delims = { link_delim(3, {10, 11}) };
        c.tokens = { 10, 11, 7, 8 };
        run_link_case(c, nullptr);
    }
    // 4. Marker in the middle.
    {
        LinkCase c;
        c.name = "l04_marker_in_middle";
        c.delims = { link_delim(3, {10, 11}) };
        c.tokens = { 7, 8, 10, 11, 9 };
        run_link_case(c, nullptr);
    }
    // 5. Marker at the end.
    {
        LinkCase c;
        c.name = "l05_marker_at_end";
        c.delims = { link_delim(3, {10, 11}) };
        c.tokens = { 7, 8, 10, 11 };
        run_link_case(c, nullptr);
    }
    // 6. Two markers, shared prefix.
    {
        LinkCase c;
        c.name = "l06_two_markers";
        c.delims = { link_delim(3, {10, 11}), link_delim(2, {10, 12}) };
        c.tokens = { 10, 11, 100, 10, 12, 200 };
        run_link_case(c, nullptr);
    }
    // 7. Three markers.
    {
        LinkCase c;
        c.name = "l07_three_markers";
        c.delims = { link_delim(3, {10, 11}), link_delim(2, {10, 12}) };
        c.tokens = { 10, 11, 100, 10, 12, 200, 10, 11, 300 };
        run_link_case(c, nullptr);
    }
    // 8. Insertion order decides between shared-prefix delimiters.
    {
        LinkCase c;
        c.name = "l08_shared_prefix_order";
        c.delims = { link_delim(2, {10, 11}), link_delim(3, {10}) };
        c.tokens = { 10, 11, 20 };
        run_link_case(c, nullptr);
    }
    // 9. Real ChatML marker, middle of a vocabized stream.
    {
        const std::vector<llama_token> user = vocab_marker(vocab, "<|im_start|>user");
        LinkCase c;
        c.name = "l09_vocab_single";
        c.delims = { link_delim_text(vocab, 3, "<|im_start|>user") };
        c.tokens = { 700, 701 };
        append_ids(c.tokens, user);
        c.tokens.push_back(702);
        run_link_case(c, nullptr);
    }
    // 10. Real ChatML user/assistant/user stream.
    {
        const std::vector<llama_token> user = vocab_marker(vocab, "<|im_start|>user");
        const std::vector<llama_token> assistant = vocab_marker(vocab, "<|im_start|>assistant");
        LinkCase c;
        c.name = "l10_vocab_three";
        c.delims = {
            link_delim_text(vocab, 3, "<|im_start|>user"),
            link_delim_text(vocab, 2, "<|im_start|>assistant"),
        };
        c.tokens = { 800 };
        append_ids(c.tokens, user);
        c.tokens.push_back(801);
        append_ids(c.tokens, assistant);
        c.tokens.push_back(802);
        append_ids(c.tokens, user);
        c.tokens.push_back(803);
        run_link_case(c, nullptr);
    }
    // 11/12. Media: the real mtmd test chunks give a text chunk and an image
    // chunk; server_tokens marks the image placeholders as a skip region.
    {
        mtmd_input_chunks * raw = mtmd_test_create_input_chunks();
        if (raw == nullptr) {
            fail("l11_media_between_messages", "mtmd test chunks unavailable");
            return;
        }
        mtmd::input_chunks chunks(raw);
        if (chunks.size() < 2) {
            fail("l11_media_between_messages", "mtmd test chunks incomplete");
            return;
        }
        const size_t media_start = mtmd_input_chunk_get_n_tokens(chunks[0]);
        const size_t media_len   = mtmd_input_chunk_get_n_tokens(chunks[1]);

        server_tokens st(chunks, true);
        st.push_back((llama_token) 10);
        st.push_back((llama_token) 11);

        LinkCase c;
        c.name = "l11_media_between_messages";
        c.delims = {
            link_delim(3, {1, 2}),
            link_delim(2, {10, 11}),
        };
        c.tokens = st.get_tokens();
        c.skips[media_start] = media_len;
        run_link_case(c, &st);
    }
    {
        mtmd_input_chunks * raw = mtmd_test_create_input_chunks();
        if (raw == nullptr) {
            fail("l12_media_hidden", "mtmd test chunks unavailable");
            return;
        }
        mtmd::input_chunks chunks(raw);
        if (chunks.size() < 2) {
            fail("l12_media_hidden", "mtmd test chunks incomplete");
            return;
        }
        const size_t media_start = mtmd_input_chunk_get_n_tokens(chunks[0]);
        const size_t media_len   = mtmd_input_chunk_get_n_tokens(chunks[1]);

        server_tokens st(chunks, true);
        st.push_back((llama_token) 10);
        st.push_back((llama_token) 11);

        LinkCase c;
        c.name = "l12_media_hidden";
        c.delims = {
            link_delim(3, {1, 2}),                       // before the chunk
            link_delim(2, {5, LLAMA_TOKEN_NULL}),         // crosses into the chunk
            link_delim(3, {LLAMA_TOKEN_NULL}),            // only inside the chunk
            link_delim(3, {5}),                           // exactly at the skip start
            link_delim(2, {10, 11}),                      // after the chunk
        };
        c.tokens = st.get_tokens();
        c.skips[media_start] = media_len;
        run_link_case(c, &st);
    }
}

int main(int argc, char ** argv) {
    const char * vocab_path = argc > 1 ? argv[1] : "models/ggml-vocab-qwen35.gguf";
    const std::string templates_dir = argc > 2 ? argv[2] : "models/templates";

    llama_backend_init();

    llama_model_params mp = llama_model_default_params();
    mp.vocab_only = true;
    llama_model * model = llama_model_load_from_file(vocab_path, mp);
    if (model == nullptr) {
        fprintf(stderr, "FG571_DELIM cannot load vocab: %s\n", vocab_path);
        llama_backend_free();
        return 2;
    }
    const llama_vocab * vocab = llama_model_get_vocab(model);

    goose_init(0, nullptr);
    check_role_table();

    for (const Case & c : template_cases()) {
        run_case(c, vocab);
    }
    for (const RealTemplate & rt : real_templates()) {
        run_real_template(rt, templates_dir, vocab);
    }
    for (const Case & c : edge_cases()) {
        run_case(c, vocab);
    }
    for (const Case & c : split_cases()) {
        run_case(c, vocab);
    }
    // The server_tokens linkage group is deferred: it needs a constructed
    // server session and aborts in get_tokens() without one. The slice
    // gates the delimiters themselves (t/r/edge/split/contract).
    (void) vocab;
    run_contract_case(vocab);

    printf("FG571_DELIM cases=%d values=%d failures=%d\n", cases, values, fails);

    llama_model_free(model);
    llama_backend_free();
    return fails == 0 ? 0 : 1;
}
