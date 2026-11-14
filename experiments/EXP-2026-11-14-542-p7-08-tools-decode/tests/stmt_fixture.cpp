// Differential fixture for the Jinja statement runtime (EXP508).
// Oracle: jinja::runtime::execute over a program parsed by the native lexer and
// parser; the port executes the same flat AST from EXP503.
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <cmath>
#include <string>
#include <vector>

#include "runtime-gen.h"
#include "jinja/value.h"
#include "chat.h"
#include "jinja/string.h"
#include "jinja/runtime.h"
#include "jinja/parser.h"

extern "C" double gs_json_strtod(sl_u8 s) {
    std::string tmp((const char *) s.data, (size_t) s.len);
    return std::strtod(tmp.c_str(), nullptr);
}
extern "C" int64_t gs_json_fmt_e(sl_u8 out, int64_t prec, double x) {
    return snprintf((char *) out.data, (size_t) out.len, "%.*e", (int) prec, x);
}
extern "C" int64_t gs_json_fmt_g(sl_u8 out, double x) {
    return snprintf((char *) out.data, (size_t) out.len, "%.17g", x);
}
extern "C" double gs_frexp_mant(double x) {
    int e = 0;
    return std::frexp(x, &e);
}
extern "C" int64_t gs_to_string_f64(sl_u8 out, double x) {
    const std::string s = std::to_string(x);
    memcpy(out.data, s.data(), s.size());
    return (int64_t) s.size();
}

static int cases = 0, values = 0, fails = 0;
static void bad(const char * what, const std::string & detail) {
    ++fails;
    if (fails <= 200) printf("FAIL %s: %s\n", what, detail.c_str());
}
static sl_u8 sl(const std::string & s) { return sl_u8{ (uint8_t *) s.data(), (int64_t) s.size() }; }
static sl_u8 slv(std::vector<uint8_t> & v) { return sl_u8{ v.data(), (int64_t) v.size() }; }
static sl_i64 sli(std::vector<int64_t> & v) { return sl_i64{ v.data(), (int64_t) v.size() }; }
static sl_f64 slf(std::vector<double> & v) { return sl_f64{ v.data(), (int64_t) v.size() }; }

static std::string visible(const std::string & s, size_t max = 400) {
    std::string r;
    size_t n = 0;
    for (unsigned char c : s) {
        if (n++ > max) { r += "..."; break; }
        if (c == '\n') r += "\\n";
        else if (c == '\r') r += "\\r";
        else if (c == '\t') r += "\\t";
        else if (c == '\\') r += "\\\\";
        else if (c == '"') r += "\\\"";
        else if (c >= 32 && c < 127) r.push_back((char) c);
        else { char b[8]; snprintf(b, sizeof b, "\\x%02X", c); r += b; }
    }
    return r;
}

// ---- value DSL -------------------------------------------------------------
struct VP {
    const std::string & s;
    size_t i = 0;
    explicit VP(const std::string & str) : s(str) {}
    void ws() { while (i < s.size() && (s[i] == ' ' || s[i] == '\t')) ++i; }
    char peek() { ws(); return i < s.size() ? s[i] : '\0'; }
    void expect(char c) { ws(); if (i < s.size() && s[i] == c) ++i; }
    std::string raw_until(char end) {
        std::string r;
        while (i < s.size() && s[i] != end) r.push_back(s[i++]);
        if (i < s.size()) ++i;
        return r;
    }
    std::string parse_str() {
        expect('"');
        std::string r;
        while (i < s.size() && s[i] != '"') {
            if (s[i] == '\\' && i + 1 < s.size()) {
                ++i;
                char c = s[i++];
                if (c == 'n') r.push_back('\n');
                else if (c == 'r') r.push_back('\r');
                else if (c == 't') r.push_back('\t');
                else if (c == '\\') r.push_back('\\');
                else if (c == '"') r.push_back('"');
                else if (c == 'x' && i + 1 < s.size()) {
                    const std::string h = s.substr(i, 2);
                    i += 2;
                    r.push_back((char) std::stoi(h, nullptr, 16));
                } else r.push_back(c);
            } else {
                r.push_back(s[i++]);
            }
        }
        if (i < s.size()) ++i;
        return r;
    }
    jinja::value parse() {
        ws();
        char c = s[i++];
        if (c == 'i') {
            char * e = nullptr;
            const long long v = strtoll(s.c_str() + i, &e, 10);
            i = (size_t) (e - s.c_str());
            return jinja::mk_val<jinja::value_int>(v);
        }
        if (c == 'f') {
            char * e = nullptr;
            const double v = strtod(s.c_str() + i, &e);
            i = (size_t) (e - s.c_str());
            return jinja::mk_val<jinja::value_float>(v);
        }
        if (c == 's') { return jinja::mk_val<jinja::value_string>(parse_str()); }
        if (c == 'b') { ws(); bool v = s[i++] == 'T'; return jinja::mk_val<jinja::value_bool>(v); }
        if (c == 'n') { return jinja::mk_val<jinja::value_none>(); }
        if (c == 'u') {
            ws();
            std::string hint;
            if (i < s.size() && s[i] == '"') { ++i; hint = raw_until('"'); }
            return jinja::mk_val<jinja::value_undefined>(hint);
        }
        if (c == 'a' || c == 't') {
            expect('[');
            std::vector<jinja::value> vals;
            while (peek() != ']' && peek() != '\0') { vals.push_back(parse()); expect(','); }
            expect(']');
            if (c == 'a') {
                auto arr = jinja::mk_val<jinja::value_array>();
                for (auto & v : vals) arr->push_back(v);
                return arr;
            }
            return jinja::mk_val<jinja::value_tuple>(std::move(vals));
        }
        if (c == 'o') {
            expect('{');
            std::vector<std::pair<jinja::value, jinja::value>> pairs;
            while (peek() != '}' && peek() != '\0') {
                auto k = parse();
                expect('=');
                auto v = parse();
                pairs.push_back({k, v});
                expect(',');
            }
            expect('}');
            auto obj = jinja::mk_val<jinja::value_object>();
            for (auto & p : pairs) obj->insert(p.first, p.second);
            return obj;
        }
        return jinja::mk_val<jinja::value_undefined>("dsl");
    }
};

// ---- flat encoding ---------------------------------------------------------
struct Enc {
    std::vector<int64_t> kind, ival, foff, flen, cs, cc, ks, kc, vs, vc;
    std::vector<double> F;
    std::vector<uint8_t> B;
    std::vector<int64_t> pool;

    int64_t str(const std::string & s) {
        const int64_t off = (int64_t) B.size();
        B.insert(B.end(), s.begin(), s.end());
        return off;
    }
    int enc(const jinja::value & v) {
        const int id = (int) kind.size();
        kind.push_back(-1); ival.push_back(0); foff.push_back(0); flen.push_back(0);
        cs.push_back(0); cc.push_back(0); ks.push_back(0); kc.push_back(0);
        vs.push_back(0); vc.push_back(0);
        F.push_back(0.0);
        if (jinja::is_val<jinja::value_int>(v)) {
            kind[id] = 0; ival[id] = v->as_int();
            double df = (double) ival[id];
            if ((int64_t) df != ival[id]) df = ival[id] < 0 ? -INFINITY : INFINITY;
            F[id] = df;
        } else if (jinja::is_val<jinja::value_float>(v)) {
            kind[id] = 1; ival[id] = v->as_int(); F[id] = v->as_float();
        } else if (jinja::is_val<jinja::value_string>(v)) {
            kind[id] = 2;
            const std::string s = v->as_string().str();
            foff[id] = str(s); flen[id] = (int64_t) s.size();
        } else if (jinja::is_val<jinja::value_bool>(v)) {
            kind[id] = 3; ival[id] = v->as_int(); F[id] = (double) ival[id];
        } else if (jinja::is_val<jinja::value_tuple>(v)) {
            kind[id] = 5;
            std::vector<int> tmp;
            for (const auto & e : v->as_array()) tmp.push_back(enc(e));
            cs[id] = (int64_t) pool.size(); cc[id] = (int64_t) tmp.size();
            for (int c : tmp) pool.push_back(c);
        } else if (jinja::is_val<jinja::value_array>(v)) {
            kind[id] = 4;
            std::vector<int> tmp;
            for (const auto & e : v->as_array()) tmp.push_back(enc(e));
            cs[id] = (int64_t) pool.size(); cc[id] = (int64_t) tmp.size();
            for (int c : tmp) pool.push_back(c);
        } else if (jinja::is_val<jinja::value_object>(v)) {
            kind[id] = 6;
            std::vector<std::pair<int, int>> tmp;
            for (const auto & p : v->as_ordered_object()) tmp.push_back({enc(p.first), enc(p.second)});
            ks[id] = (int64_t) pool.size(); kc[id] = (int64_t) tmp.size();
            for (auto & p : tmp) pool.push_back(p.first);
            vs[id] = (int64_t) pool.size(); vc[id] = (int64_t) tmp.size();
            for (auto & p : tmp) pool.push_back(p.second);
        } else if (jinja::is_val<jinja::value_none>(v)) {
            kind[id] = 7;
        } else {
            kind[id] = 8;
            std::string hint;
            const std::string t = v->type();
            const std::string pre = "Undefined (hint: '";
            if (t.size() > pre.size() && t.compare(0, pre.size(), pre) == 0) {
                hint = t.substr(pre.size(), t.size() - pre.size() - 2);
            }
            foff[id] = str(hint); flen[id] = (int64_t) hint.size();
        }
        return id;
    }
};

struct AstEnc {
    std::vector<int64_t> kind, pos, ival, soff, slen, c0, c1, c2, l0s, l0c, l1s, l1c;
    std::vector<double> fval;
    std::vector<int64_t> pool;
    std::vector<uint8_t> & B;
    explicit AstEnc(std::vector<uint8_t> & b) : B(b) {}
    int64_t put(const std::string & t) {
        const int64_t o = (int64_t) B.size();
        B.insert(B.end(), t.begin(), t.end());
        return o;
    }
    void init(int id, int k, const jinja::statement * s) {
        kind[id] = k; pos[id] = (int64_t) s->pos;
        ival[id] = 0; soff[id] = 0; slen[id] = 0;
        c0[id] = -1; c1[id] = -1; c2[id] = -1;
        l0s[id] = 0; l0c[id] = 0; l1s[id] = 0; l1c[id] = 0;
        fval[id] = 0.0;
    }
    void put_list(std::vector<int64_t> & target, const std::vector<int> & kids, int id, bool second) {
        if (kids.empty()) return;
        const int64_t start = (int64_t) pool.size();
        for (int k : kids) pool.push_back(k);
        if (second) { l1s[id] = start; l1c[id] = (int64_t) kids.size(); }
        else { l0s[id] = start; l0c[id] = (int64_t) kids.size(); }
    }
    int enc(const jinja::statement * s) {
        const int id = (int) kind.size();
        kind.push_back(0); pos.push_back(0); ival.push_back(0); soff.push_back(0); slen.push_back(0);
        c0.push_back(-1); c1.push_back(-1); c2.push_back(-1);
        l0s.push_back(0); l0c.push_back(0); l1s.push_back(0); l1c.push_back(0);
        fval.push_back(0.0);
        if (auto x = dynamic_cast<const jinja::integer_literal *>(s)) {
            init(id, 13, s); ival[id] = x->val;
        } else if (auto x = dynamic_cast<const jinja::float_literal *>(s)) {
            init(id, 14, s); fval[id] = x->val;
        } else if (auto x = dynamic_cast<const jinja::string_literal *>(s)) {
            init(id, 15, s); soff[id] = put(x->val); slen[id] = (int64_t) x->val.size();
        } else if (auto x = dynamic_cast<const jinja::identifier *>(s)) {
            init(id, 12, s); soff[id] = put(x->val); slen[id] = (int64_t) x->val.size();
        } else if (auto x = dynamic_cast<const jinja::binary_expression *>(s)) {
            init(id, 19, s); soff[id] = put(x->op.value); slen[id] = (int64_t) x->op.value.size();
            c0[id] = enc(x->left.get()); c1[id] = enc(x->right.get());
        } else if (auto x = dynamic_cast<const jinja::unary_expression *>(s)) {
            init(id, 24, s); soff[id] = put(x->op.value); slen[id] = (int64_t) x->op.value.size();
            c0[id] = enc(x->argument.get());
        } else if (auto x = dynamic_cast<const jinja::array_literal *>(s)) {
            init(id, 16, s);
            std::vector<int> kids; for (const auto & c : x->val) kids.push_back(enc(c.get()));
            put_list(l0s, kids, id, false);
        } else if (auto x = dynamic_cast<const jinja::tuple_literal *>(s)) {
            init(id, 17, s);
            std::vector<int> kids; for (const auto & c : x->val) kids.push_back(enc(c.get()));
            put_list(l0s, kids, id, false);
        } else if (auto x = dynamic_cast<const jinja::program *>(s)) {
            init(id, 0, s);
            std::vector<int> kids; for (const auto & c : x->body) kids.push_back(enc(c.get()));
            put_list(l0s, kids, id, false);
        } else if (auto x = dynamic_cast<const jinja::if_statement *>(s)) {
            init(id, 1, s);
            c0[id] = enc(x->test.get());
            std::vector<int> b; for (const auto & c : x->body) b.push_back(enc(c.get()));
            std::vector<int> e; for (const auto & c : x->alternate) e.push_back(enc(c.get()));
            put_list(l0s, b, id, false); put_list(l1s, e, id, true);
        } else if (auto x = dynamic_cast<const jinja::set_statement *>(s)) {
            init(id, 6, s);
            c0[id] = enc(x->assignee.get());
            c1[id] = x->val ? enc(x->val.get()) : -1;
            std::vector<int> b; for (const auto & c : x->body) b.push_back(enc(c.get()));
            put_list(l0s, b, id, false);
        } else if (auto x = dynamic_cast<const jinja::member_expression *>(s)) {
            init(id, 20, s); ival[id] = x->computed ? 1 : 0;
            c0[id] = enc(x->object.get()); c1[id] = enc(x->property.get());
        } else if (auto x = dynamic_cast<const jinja::select_expression *>(s)) {
            init(id, 25, s);
            c0[id] = enc(x->lhs.get()); c1[id] = enc(x->test.get());
        } else if (auto x = dynamic_cast<const jinja::for_statement *>(s)) {
            init(id, 2, s);
            c0[id] = enc(x->loopvar.get()); c1[id] = enc(x->iterable.get());
            std::vector<int> body; for (const auto & c : x->body) body.push_back(enc(c.get()));
            std::vector<int> dflt; for (const auto & c : x->default_block) dflt.push_back(enc(c.get()));
            put_list(l0s, body, id, false); put_list(l1s, dflt, id, true);
        } else if (auto x = dynamic_cast<const jinja::macro_statement *>(s)) {
            init(id, 7, s);
            c0[id] = enc(x->name.get());
            std::vector<int> a; for (const auto & c : x->args) a.push_back(enc(c.get()));
            std::vector<int> b; for (const auto & c : x->body) b.push_back(enc(c.get()));
            put_list(l0s, a, id, false); put_list(l1s, b, id, true);
        } else if (auto x = dynamic_cast<const jinja::call_statement *>(s)) {
            init(id, 8, s);
            c0[id] = enc(x->call.get());
            std::vector<int> a; for (const auto & c : x->caller_args) a.push_back(enc(c.get()));
            std::vector<int> b; for (const auto & c : x->body) b.push_back(enc(c.get()));
            put_list(l0s, a, id, false); put_list(l1s, b, id, true);
        } else if (auto x = dynamic_cast<const jinja::keyword_argument_expression *>(s)) {
            init(id, 22, s);
            c0[id] = enc(x->key.get()); c1[id] = enc(x->val.get());
        } else if (auto x = dynamic_cast<const jinja::test_expression *>(s)) {
            init(id, 26, s); ival[id] = x->negate ? 1 : 0;
            c0[id] = enc(x->operand.get()); c1[id] = enc(x->test.get());
        } else if (auto x = dynamic_cast<const jinja::filter_expression *>(s)) {
            init(id, 23, s);
            c0[id] = enc(x->operand.get()); c1[id] = enc(x->filter.get());
        } else if (auto x = dynamic_cast<const jinja::call_expression *>(s)) {
            init(id, 21, s);
            c0[id] = enc(x->callee.get());
            std::vector<int> a; for (const auto & c : x->args) a.push_back(enc(c.get()));
            put_list(l0s, a, id, false);
        } else if (dynamic_cast<const jinja::break_statement *>(s)) {
            init(id, 3, s);
        } else if (dynamic_cast<const jinja::continue_statement *>(s)) {
            init(id, 4, s);
        } else {
            throw std::runtime_error(std::string("unsupported AST node: ") + s->type());
        }
        return id;
    }
};

static void check_tpl(const std::string & src,
                      const std::vector<std::pair<std::string, std::string>> & vars,
                      const std::string * forced_want = nullptr,
                      int64_t * out_used = nullptr,
                      const std::string & probe_var = "",
                      bool compare_output = true,
                      int64_t * out_mask = nullptr,
                      std::string * out_text = nullptr,
                      bool caps_mode = false,
                      bool * out_error = nullptr,
                      int64_t * out_tool_name = nullptr,
                      int64_t * out_tool_calls = nullptr,
                      int64_t * out_arg = nullptr,
                      int64_t * out_second = nullptr,
                      int64_t * out_var = nullptr) {
    if (getenv("FG_TRACE")) { fprintf(stderr, "CASE %s\n", src.c_str()); fflush(stderr); }
    jinja::lexer lx;
    auto lr = lx.tokenize(src);
    auto prog = jinja::parse_from_tokens(lr);

    jinja::context ctx("");
    std::vector<std::pair<std::string, jinja::value>> cvals;
    for (const auto & kv : vars) {
        VP p(kv.second);
        auto v = p.parse();
        ctx.set_val(kv.first, v);
        cvals.push_back({kv.first, v});
    }
    cvals.push_back({"true", jinja::mk_val<jinja::value_bool>(true)});
    cvals.push_back({"True", jinja::mk_val<jinja::value_bool>(true)});
    cvals.push_back({"false", jinja::mk_val<jinja::value_bool>(false)});
    cvals.push_back({"False", jinja::mk_val<jinja::value_bool>(false)});
    cvals.push_back({"none", jinja::mk_val<jinja::value_none>()});
    cvals.push_back({"None", jinja::mk_val<jinja::value_none>()});

    bool nok = true;
    std::string want, nerr;
    if (forced_want) {
        want = *forced_want;
    } else
    try {
        auto res = jinja::runtime(ctx).execute(prog);
        auto parts = jinja::runtime::gather_string_parts(res);
        want = jinja::render_string_parts(parts);
    } catch (const std::exception & e) {
        nok = false;
        nerr = e.what();
    }

    Enc e;
    std::vector<int64_t> croot;
    for (const auto & kv : cvals) croot.push_back(e.enc(kv.second));
    AstEnc a(e.B);
    const int64_t aroot = a.enc(&prog);

    const int64_t vn = (int64_t) e.kind.size();
    const int64_t valcap = vn + 1024;
    const int64_t poolcap = (int64_t) e.pool.size() + 65536;
    const int64_t an = (int64_t) a.kind.size();
    const int64_t astcap = an + 4; // four spare nodes for bound method calls
    const int64_t astpoolcap = (int64_t) a.pool.size() + 64;
    std::vector<int64_t> A((size_t) (32 + 11 * valcap + poolcap + 12 * astcap + astpoolcap), 0);
    A[0] = valcap; A[1] = vn; A[2] = (int64_t) e.pool.size(); A[3] = astcap; A[5] = poolcap; A[6] = astpoolcap;
    const int64_t vb = 32;
    for (int64_t i = 0; i < vn; ++i) {
        A[(size_t) (vb + i)] = e.kind[(size_t) i];
        A[(size_t) (vb + valcap + i)] = e.ival[(size_t) i];
        A[(size_t) (vb + 2 * valcap + i)] = e.foff[(size_t) i];
        A[(size_t) (vb + 3 * valcap + i)] = e.flen[(size_t) i];
        A[(size_t) (vb + 4 * valcap + i)] = e.cs[(size_t) i];
        A[(size_t) (vb + 5 * valcap + i)] = e.cc[(size_t) i];
        A[(size_t) (vb + 6 * valcap + i)] = e.ks[(size_t) i];
        A[(size_t) (vb + 7 * valcap + i)] = e.kc[(size_t) i];
        A[(size_t) (vb + 8 * valcap + i)] = e.vs[(size_t) i];
        A[(size_t) (vb + 9 * valcap + i)] = e.vc[(size_t) i];
    }
    for (size_t i = 0; i < e.pool.size(); ++i) A[(size_t) (vb + 11 * valcap + i)] = e.pool[i];
    const int64_t ab = 32 + 11 * valcap + poolcap;
    for (int64_t i = 0; i < an; ++i) {
        A[(size_t) (ab + i)] = a.kind[(size_t) i];
        A[(size_t) (ab + astcap + i)] = a.pos[(size_t) i];
        A[(size_t) (ab + 2 * astcap + i)] = a.ival[(size_t) i];
        A[(size_t) (ab + 3 * astcap + i)] = a.soff[(size_t) i];
        A[(size_t) (ab + 4 * astcap + i)] = a.slen[(size_t) i];
        A[(size_t) (ab + 5 * astcap + i)] = a.c0[(size_t) i];
        A[(size_t) (ab + 6 * astcap + i)] = a.c1[(size_t) i];
        A[(size_t) (ab + 7 * astcap + i)] = a.c2[(size_t) i];
        A[(size_t) (ab + 8 * astcap + i)] = a.l0s[(size_t) i];
        A[(size_t) (ab + 9 * astcap + i)] = a.l0c[(size_t) i];
        A[(size_t) (ab + 10 * astcap + i)] = a.l1s[(size_t) i];
        A[(size_t) (ab + 11 * astcap + i)] = a.l1c[(size_t) i];
    }
    for (size_t i = 0; i < a.pool.size(); ++i) A[(size_t) (ab + 12 * astcap + i)] = a.pool[i];

    std::vector<int64_t> noff, nlen;
    for (const auto & kv : cvals) {
        noff.push_back((int64_t) e.B.size());
        nlen.push_back((int64_t) kv.first.size());
        e.B.insert(e.B.end(), kv.first.begin(), kv.first.end());
    }
    const int64_t blen = (int64_t) e.B.size();
    std::vector<uint8_t> B(1 << 22, 0);
    memcpy(B.data(), e.B.data(), e.B.size());
    std::vector<int64_t> cnoff((size_t) (cvals.size() + 1024), 0), cnlen((size_t) (cvals.size() + 1024), 0),
        cval((size_t) (cvals.size() + 1024), 0);
    for (size_t i = 0; i < cvals.size(); ++i) { cnoff[i] = noff[i]; cnlen[i] = nlen[i]; cval[i] = croot[i]; }

    std::vector<double> F((size_t) (valcap + astcap), 0.0);
    for (int64_t i = 0; i < vn; ++i) F[(size_t) i] = e.F[(size_t) i];
    for (int64_t i = 0; i < an; ++i) F[(size_t) (valcap + i)] = a.fval[(size_t) i];

    std::vector<uint8_t> out(1 << 22, 0), E(1 << 16, 0);
    std::vector<int64_t> st(64, 0);
    st[11] = (int64_t) cvals.size();
    ++cases;
    const int64_t len = exec_program(aroot, blen, sli(cnoff), sli(cnlen), sli(cval),
        sli(A), slf(F), slv(B), slv(E), slv(out), sli(st));

    if (caps_mode) {
        int64_t root = -1;
        for (size_t i = 0; i < cvals.size(); ++i) {
            if (cvals[i].first == probe_var) { root = croot[i]; break; }
        }
        if (out_mask) *out_mask = root < 0 ? 0 : chat_probe_content_mask(sli(A), slv(B), sli(st), root);
        if (out_error) *out_error = len < 0 && st[1] != 0;
        if (out_tool_name || out_tool_calls || out_arg || out_second || out_var) {
            int64_t troot = -1;
            for (size_t i = 0; i < cvals.size(); ++i) {
                if (cvals[i].first == "tools") { troot = croot[i]; break; }
            }
            if (out_tool_name) *out_tool_name = troot < 0 ? -1 : chat_probe_tool_name_mask(sli(A), slv(B), sli(st), troot);
            if (out_tool_calls) *out_tool_calls = root < 0 ? -1 : chat_probe_tool_calls_mask(sli(A), slv(B), sli(st), root);
            if (out_arg) *out_arg = root < 0 ? -1 : chat_probe_tool_arg_mask(sli(A), slv(B), sli(st), root);
            if (out_second) *out_second = root < 0 ? -1 : chat_probe_second_call_mask(sli(A), slv(B), sli(st), root);
            if (out_var) *out_var = root < 0 ? -1 : chat_probe_mask(sli(A), sli(st), root);
        }
        if (out_text) *out_text = std::string((const char *) out.data(), (size_t) (len < 0 ? 0 : len));
        return;
    }

    if (!nok) {
        ++values;
        if (len >= 0 || st[1] == 0) { bad("expected rejection", src); return; }
        ++values;
        const std::string gerr((const char *) E.data(), (size_t) st[2]);
        if (gerr != nerr) bad("error text", src + "\n  got =[" + visible(gerr) + "]\n  want=[" + visible(nerr) + "]");
        return;
    }
    ++values;
    if (len < 0) {
        const std::string gerr((const char *) E.data(), (size_t) st[2]);
        bad("unexpected rejection", src + ": " + visible(gerr));
        return;
    }
    ++values;
    int64_t root = -1;
    for (size_t i = 0; i < cvals.size(); ++i) {
        if (cvals[i].first == probe_var) { root = croot[i]; break; }
    }
    if (out_used) {
        *out_used = root < 0 ? -1 : chat_probe_content_used(sli(A), slv(B), sli(st), root);
    }
    const std::string got((const char *) out.data(), (size_t) len);
    if (out_mask) {
        *out_mask = root < 0 ? 0 : chat_probe_content_mask(sli(A), slv(B), sli(st), root);
    }
    if (out_text) {
        *out_text = got;
    }
    if (compare_output && got != want) bad("output", src + "\n  got =[" + visible(got) + "]\n  want=[" + visible(want) + "]");
}

static std::string vp_str(const std::string & s) {
    std::string r = "s\"";
    for (char c : s) {
        if (c == '"' || c == '\\') r.push_back('\\');
        r.push_back(c);
    }
    r.push_back('"');
    return r;
}

static void check_chat(const std::string & src,
                       const std::vector<std::pair<std::string, std::string>> & msgs,
                       bool add_ass) {
    auto tmpls = common_chat_templates_init(nullptr, src);
    if (!tmpls) { printf("FAIL chat init: %s\n", src.c_str()); ++fails; return; }
    common_chat_templates_inputs in;
    for (const auto & m : msgs) {
        common_chat_msg msg;
        msg.role = m.first;
        msg.content = m.second;
        in.messages.push_back(msg);
    }
    in.add_generation_prompt = add_ass;
    std::string want;
    try {
        want = common_chat_templates_apply(tmpls.get(), in).prompt.str();
    } catch (const std::exception & e) {
        printf("FAIL chat oracle: %s: %s\n", src.c_str(), e.what());
        ++fails;
        return;
    }
    std::string arr = "a[";
    for (size_t i = 0; i < msgs.size(); ++i) {
        if (i) arr += ",";
        arr += "o{s\"role\"=" + vp_str(msgs[i].first) + ",s\"content\"=" + vp_str(msgs[i].second) + "}";
    }
    arr += "]";
    const std::vector<std::pair<std::string, std::string>> vars = {
        {"messages", arr},
        {"add_generation_prompt", add_ass ? "bT" : "bF"},
    };
    check_tpl(src, vars, &want);
}

static void check_chat_shape(const std::string & role, const std::string & content) {
    common_chat_msg msg;
    msg.role = role;
    msg.content = content;
    std::string want;
    try {
        want = common_chat_msgs_to_json_oaicompat(std::vector<common_chat_msg>{msg}, false).dump();
    } catch (const std::exception & e) {
        printf("FAIL chat shape oracle: %s\n", e.what());
        ++fails;
        return;
    }
    const int64_t valcap = 64, poolcap = 256;
    std::vector<int64_t> A((size_t) (32 + 11 * valcap + poolcap), 0);
    A[0] = valcap;
    A[5] = poolcap;
    std::vector<double> F((size_t) valcap, 0.0);
    std::vector<uint8_t> B(1 << 16, 0), E(1 << 12, 0), out(1 << 16, 0);
    std::vector<int64_t> st(64, 0);
    sl_i64 as{A.data(), (int64_t) A.size()};
    sl_f64 fs{F.data(), (int64_t) F.size()};
    sl_u8 bs{B.data(), (int64_t) B.size()};
    sl_u8 es{E.data(), (int64_t) E.size()};
    sl_i64 ss{st.data(), (int64_t) st.size()};
    sl_u8 rs{(uint8_t *) role.data(), (int64_t) role.size()};
    sl_u8 cs{(uint8_t *) content.data(), (int64_t) content.size()};
    sl_u8 os{out.data(), (int64_t) out.size()};
    const int64_t n = chat_message_json(as, fs, bs, es, ss, rs, cs, os);
    const std::string got((const char *) out.data(), (size_t) (n < 0 ? 0 : n));
    ++cases;
    ++values;
    bool same = false;
    try {
        same = common_json::parse(want).dump() == common_json::parse(got).dump();
    } catch (const std::exception &) {
        same = false;
    }
    if (!same) {
        ++fails;
        printf("FAIL chat shape: %s/%s\n  got =[%s]\n  want=[%s]\n", role.c_str(), content.c_str(),
               visible(got).c_str(), visible(want).c_str());
    }
}


static jinja::value make_msg(const std::string & role, const std::string & content) {
    auto obj = jinja::mk_val<jinja::value_object>();
    obj->insert("role", jinja::mk_val<jinja::value_string>(role));
    obj->insert("content", jinja::mk_val<jinja::value_string>(content));
    return obj;
}

static std::string native_render_msgs(const std::string & src,
                                      const std::vector<std::pair<std::string, std::string>> & msgs) {
    jinja::lexer lx;
    auto lr = lx.tokenize(src);
    auto prog = jinja::parse_from_tokens(lr);
    jinja::context ctx("");
    auto arr = jinja::mk_val<jinja::value_array>();
    for (const auto & m : msgs) arr->push_back(make_msg(m.first, m.second));
    ctx.set_val("messages", arr);
    ctx.set_val("add_generation_prompt", jinja::mk_val<jinja::value_bool>(false));
    auto res = jinja::runtime(ctx).execute(prog);
    return jinja::render_string_parts(jinja::runtime::gather_string_parts(res));
}

static std::string vp_escape(const std::string & s) {
    std::string r;
    for (unsigned char c : s) {
        if (c == '"' || c == '\\') { r.push_back('\\'); r.push_back((char) c); }
        else if (c == '\n') r += "\\n";
        else if (c == '\r') r += "\\r";
        else if (c == '\t') r += "\\t";
        else if (c < 32 || c >= 127) { char b[8]; snprintf(b, sizeof b, "\\x%02X", c); r += b; }
        else r.push_back((char) c);
    }
    return r;
}

static void check_chat_roles(const std::string & src,
                             const std::vector<std::pair<std::string, std::string>> & msgs,
                             bool add_ass) {
    auto tmpls = common_chat_templates_init(nullptr, src);
    if (!tmpls) { printf("FAIL chat roles init: %s\n", src.c_str()); ++fails; return; }
    common_chat_templates_inputs in;
    for (const auto & m : msgs) {
        common_chat_msg msg;
        msg.role = m.first;
        msg.content = m.second;
        in.messages.push_back(msg);
    }
    in.add_generation_prompt = add_ass;
    std::string want;
    try {
        want = common_chat_templates_apply(tmpls.get(), in).prompt.str();
    } catch (const std::exception & e) {
        printf("FAIL chat roles oracle: %s: %s\n", src.c_str(), e.what());
        ++fails;
        return;
    }

    // caps proxy: the native rule drops the system role when the template does
    // not use messages[0].content under the system+user probe. Compare two
    // renders of that probe with different system content.
    const std::vector<std::pair<std::string, std::string>> probe_a = {{"system", "System message"}, {"user", "User message"}};
    const std::vector<std::pair<std::string, std::string>> probe_b = {{"system", "ZZZ"}, {"user", "User message"}};
    bool supports_system = native_render_msgs(src, probe_a) != native_render_msgs(src, probe_b);

    std::string blob;
    std::vector<int64_t> roff, rlen, coff, clen;
    for (const auto & m : msgs) {
        roff.push_back((int64_t) blob.size()); rlen.push_back((int64_t) m.first.size());
        blob += m.first;
        coff.push_back((int64_t) blob.size()); clen.push_back((int64_t) m.second.size());
        blob += m.second;
    }

    const int64_t valcap = 256, poolcap = 1024;
    std::vector<int64_t> A((size_t) (32 + 11 * valcap + poolcap), 0);
    A[0] = valcap;
    A[5] = poolcap;
    std::vector<double> F((size_t) valcap, 0.0);
    std::vector<uint8_t> B(1 << 16, 0), E(1 << 12, 0), out(1 << 16, 0);
    std::vector<int64_t> st(64, 0);
    std::vector<int64_t> roff2 = roff, rlen2 = rlen, coff2 = coff, clen2 = clen;
    sl_i64 as{A.data(), (int64_t) A.size()};
    sl_f64 fs{F.data(), (int64_t) F.size()};
    sl_u8 bs{B.data(), (int64_t) B.size()};
    sl_u8 es{E.data(), (int64_t) E.size()};
    sl_i64 ss{st.data(), (int64_t) st.size()};
    sl_u8 blob_s{(uint8_t *) blob.data(), (int64_t) blob.size()};
    sl_i64 roff_s{roff2.data(), (int64_t) roff2.size()};
    sl_i64 rlen_s{rlen2.data(), (int64_t) rlen2.size()};
    sl_i64 coff_s{coff2.data(), (int64_t) coff2.size()};
    sl_i64 clen_s{clen2.data(), (int64_t) clen2.size()};
    sl_u8 os{out.data(), (int64_t) out.size()};
    const int64_t n = chat_normalize(as, fs, bs, es, ss, blob_s, roff_s, rlen_s, coff_s, clen_s,
                                     (int64_t) msgs.size(), supports_system ? 1 : 0, os);
    std::string got((const char *) out.data(), (size_t) (n < 0 ? 0 : n));

    // our JSON -> VP array literal for the template context
    std::string vp = "a[";
    try {
        auto arr = common_json::parse(got);
        for (size_t i = 0; i < arr.size(); ++i) {
            if (i) vp += ",";
            auto m = arr[i];
            vp += "o{s\"role\"=s\"" + vp_escape(m.at("role").get<std::string>()) +
                  "\",s\"content\"=s\"" + vp_escape(m.at("content").get<std::string>()) + "\"}";
        }
    } catch (const std::exception & e) {
        printf("FAIL chat roles json: %s: %s got=[%s]\n", src.c_str(), e.what(), visible(got).c_str());
        ++fails;
        return;
    }
    vp += "]";
    const std::vector<std::pair<std::string, std::string>> vars = {
        {"messages", vp},
        {"add_generation_prompt", add_ass ? "bT" : "bF"},
    };
    check_tpl(src, vars, &want);
}



static void check_caps(const std::string & src) {
    bool native_supported = false;
    try {
        common_chat_template tmpl(src, "", "");
        native_supported = tmpl.original_caps().supports_system_role;
    } catch (const std::exception & e) {
        printf("FAIL caps oracle: %s: %s\n", src.c_str(), e.what());
        ++fails;
        return;
    }
    const std::string vp =
        "a[o{s\"role\"=s\"system\",s\"content\"=s\"System message\"},"
        "o{s\"role\"=s\"user\",s\"content\"=s\"User message\"}]";
    const std::vector<std::pair<std::string, std::string>> vars = {
        {"messages", vp},
        {"add_generation_prompt", "bF"},
    };
    int64_t used = -1;
    const std::string sink;
    check_tpl(src, vars, &sink, &used, "messages", false);
    ++cases;
    ++values;
    if ((used != 0) != native_supported) {
        ++fails;
        printf("FAIL caps: %s\n  ours_used=%lld native_supported=%d\n", src.c_str(),
               (long long) used, native_supported ? 1 : 0);
    }
}


static void check_caps_content(const std::string & src) {
    bool n_string = true, n_typed = false;
    try {
        common_chat_template tmpl(src, "", "");
        auto c = tmpl.original_caps();
        n_string = c.supports_string_content;
        n_typed  = c.supports_typed_content;
    } catch (const std::exception & e) {
        printf("FAIL caps content oracle: %s: %s\n", src.c_str(), e.what());
        ++fails;
        return;
    }
    const std::vector<std::pair<std::string, std::string>> vars = {
        {"messages", "a[o{s\"role\"=s\"user\",s\"content\"=s\"STRING_MARKER\"}]"},
    };
    int64_t mask = 0;
    std::string text;
    bool failed = false;
    const std::string sink;
    check_tpl(src, vars, &sink, nullptr, "messages", false, &mask, &text, true, &failed);

    const bool used_as_array = (mask & 6) != 0;
    const bool o_typed = used_as_array;
    const bool o_string = !failed && !(used_as_array && text.find("STRING_MARKER") == std::string::npos);

    ++cases;
    ++values;
    if (o_typed != n_typed || o_string != n_string) {
        ++fails;
        printf("FAIL caps content: %s\n  ours string=%d typed=%d (mask=%lld text=[%s])\n  native string=%d typed=%d\n",
               src.c_str(), o_string ? 1 : 0, o_typed ? 1 : 0, (long long) mask, visible(text).c_str(),
               n_string ? 1 : 0, n_typed ? 1 : 0);
    }
}


static void check_caps_tools(const std::string & src) {
    bool n_tools = true, n_calls = true;
    try {
        common_chat_template tmpl(src, "", "");
        auto c = tmpl.original_caps();
        n_tools = c.supports_tools;
        n_calls = c.supports_tool_calls;
    } catch (const std::exception & e) {
        printf("FAIL caps tools oracle: %s: %s\n", src.c_str(), e.what());
        ++fails;
        return;
    }
    const std::string msgs_vp =
        "a[o{s\"role\"=s\"user\",s\"content\"=s\"User message\"},"
        "o{s\"role\"=s\"assistant\",s\"content\"=s\"\",s\"tool_calls\"=a[o{s\"id\"=s\"call00001\",s\"type\"=s\"function\","
        "s\"function\"=o{s\"name\"=s\"tool1\",s\"arguments\"=o{s\"arg\"=s\"value\"}}}]},"
        "o{s\"role\"=s\"tool\",s\"content\"=s\"Tool response\",s\"tool_call_id\"=s\"call00001\"},"
        "o{s\"role\"=s\"assistant\",s\"content\"=s\"done\"},"
        "o{s\"role\"=s\"user\",s\"content\"=s\"User message\"}]";
    const std::string tools_vp =
        "a[o{s\"name\"=s\"tool\",s\"type\"=s\"function\","
        "s\"function\"=o{s\"name\"=s\"tool1\",s\"description\"=s\"Tool description\"}}]";
    const std::vector<std::pair<std::string, std::string>> vars = {
        {"messages", msgs_vp},
        {"tools", tools_vp},
        {"add_generation_prompt", "bF"},
    };
    bool failed = false;
    int64_t tname = -1, tcalls = -1;
    const std::string sink;
    int64_t mask = 0;
    std::string text;
    check_tpl(src, vars, &sink, nullptr, "messages", false, &mask, &text, true, &failed, &tname, &tcalls);

    const bool o_tools = !failed && tname > 0;
    const bool o_calls = !failed && tcalls > 0;

    ++cases;
    ++values;
    if (o_tools != n_tools || o_calls != n_calls) {
        ++fails;
        printf("FAIL caps tools: %s\n  ours tools=%d calls=%d (tname=%lld tcalls=%lld failed=%d)\n"
               "  native tools=%d calls=%d\n",
               src.c_str(), o_tools ? 1 : 0, o_calls ? 1 : 0, (long long) tname, (long long) tcalls,
               failed ? 1 : 0, n_tools ? 1 : 0, n_calls ? 1 : 0);
    }
}


static std::string tools_probe_msgs(bool two_calls) {
    const std::string call1 =
        "o{s\"id\"=s\"call00001\",s\"type\"=s\"function\","
        "s\"function\"=o{s\"name\"=s\"tool1\",s\"arguments\"=o{s\"arg\"=s\"value\"}}}";
    const std::string call2 =
        "o{s\"id\"=s\"call00002\",s\"type\"=s\"function\","
        "s\"function\"=o{s\"name\"=s\"tool1\",s\"arguments\"=o{s\"arg\"=s\"value\"}}}";
    const std::string calls = "a[" + call1 + (two_calls ? "," + call2 : "") + "]";
    return "a[o{s\"role\"=s\"user\",s\"content\"=s\"User message\"},"
           "o{s\"role\"=s\"assistant\",s\"content\"=s\"\",s\"tool_calls\"=" + calls + "},"
           "o{s\"role\"=s\"tool\",s\"content\"=s\"Tool response\",s\"tool_call_id\"=s\"call00001\"},"
           "o{s\"role\"=s\"assistant\",s\"content\"=s\"done\"},"
           "o{s\"role\"=s\"user\",s\"content\"=s\"User message\"}]";
}

static void check_caps_extra(const std::string & src) {
    bool n_objargs = false, n_parallel = true;
    try {
        common_chat_template tmpl(src, "", "");
        auto c = tmpl.original_caps();
        n_objargs = c.supports_object_arguments;
        n_parallel = c.supports_parallel_tool_calls;
    } catch (const std::exception & e) {
        printf("FAIL caps extra oracle: %s: %s\n", src.c_str(), e.what());
        ++fails;
        return;
    }
    const std::string tools_vp =
        "a[o{s\"name\"=s\"tool\",s\"type\"=s\"function\","
        "s\"function\"=o{s\"name\"=s\"tool1\",s\"description\"=s\"Tool description\"}}]";

    bool failed = false;
    int64_t arg = -1, second = -1;
    const std::string sink;
    int64_t mask = 0;
    std::string text;
    const std::vector<std::pair<std::string, std::string>> vars1 = {
        {"messages", tools_probe_msgs(false)},
        {"tools", tools_vp},
        {"add_generation_prompt", "bF"},
    };
    check_tpl(src, vars1, &sink, nullptr, "messages", false, &mask, &text, true, &failed, nullptr, nullptr, &arg, nullptr);

    bool failed2 = false;
    const std::vector<std::pair<std::string, std::string>> vars2 = {
        {"messages", tools_probe_msgs(true)},
        {"tools", tools_vp},
        {"add_generation_prompt", "bF"},
    };
    check_tpl(src, vars2, &sink, nullptr, "messages", false, &mask, &text, true, &failed2, nullptr, nullptr, nullptr, &second);

    const bool o_objargs = !failed && arg > 0;
    const bool o_parallel = !failed2 && second > 0;

    ++cases;
    ++values;
    if (o_objargs != n_objargs || o_parallel != n_parallel) {
        ++fails;
        printf("FAIL caps extra: %s\n  ours objargs=%d parallel=%d (arg=%lld second=%lld failed=%d/%d)\n"
               "  native objargs=%d parallel=%d\n",
               src.c_str(), o_objargs ? 1 : 0, o_parallel ? 1 : 0, (long long) arg, (long long) second,
               failed ? 1 : 0, failed2 ? 1 : 0, n_objargs ? 1 : 0, n_parallel ? 1 : 0);
    }
}


static void check_caps_reasoning(const std::string & src) {
    bool n_preserve = false, n_effort = false;
    try {
        common_chat_template tmpl(src, "", "");
        auto c = tmpl.original_caps();
        n_preserve = c.supports_preserve_reasoning;
        n_effort = c.supports_reasoning_effort;
    } catch (const std::exception & e) {
        printf("FAIL caps reasoning oracle: %s: %s\n", src.c_str(), e.what());
        ++fails;
        return;
    }
    const std::string placeholder = "<REASONING_CONTENT_PLACEHOLDER>";
    const std::string keep_vp =
        "a[o{s\"role\"=s\"user\",s\"content\"=s\"User message\"},"
        "o{s\"role\"=s\"assistant\",s\"content\"=s\"Assistant message\",s\"reasoning_content\"=s\"" + placeholder + "\"},"
        "o{s\"role\"=s\"user\",s\"content\"=s\"User message\"},"
        "o{s\"role\"=s\"assistant\",s\"content\"=s\"Assistant message\",s\"reasoning_content\"=s\"Reasoning content\"},"
        "o{s\"role\"=s\"user\",s\"content\"=s\"User message\"}]";
    const std::string one_vp = "a[o{s\"role\"=s\"user\",s\"content\"=s\"User message\"}]";

    bool failed = false;
    int64_t mask = 0;
    std::string text;
    const std::string sink;
    const std::vector<std::pair<std::string, std::string>> vars_keep = {
        {"messages", keep_vp},
        {"enable_thinking", "bT"},
        {"preserve_thinking", "bT"},
        {"clear_thinking", "bF"},
        {"truncate_history_thinking", "bF"},
        {"drop_thinking", "bF"},
        {"add_generation_prompt", "bT"},
    };
    check_tpl(src, vars_keep, &sink, nullptr, "messages", false, &mask, &text, true, &failed);
    const bool o_preserve = !failed && text.find(placeholder) != std::string::npos;

    const std::vector<std::pair<std::string, std::string>> vars_effort = {
        {"messages", one_vp},
        {"enable_thinking", "bT"},
        {"reasoning_effort", "s\"low\""},
        {"add_generation_prompt", "bT"},
    };
    bool failed2 = false;
    int64_t var_mask = 0;
    check_tpl(src, vars_effort, &sink, nullptr, "reasoning_effort", false, &mask, &text, true, &failed2,
              nullptr, nullptr, nullptr, nullptr, &var_mask);
    const bool o_effort = !failed2 && var_mask > 0;

    ++cases;
    ++values;
    if (o_preserve != n_preserve || o_effort != n_effort) {
        ++fails;
        printf("FAIL caps reasoning: %s\n  ours preserve=%d effort=%d (text has placeholder=%d var_mask=%lld failed=%d/%d)\n"
               "  native preserve=%d effort=%d\n",
               src.c_str(), o_preserve ? 1 : 0, o_effort ? 1 : 0,
               text.find(placeholder) != std::string::npos ? 1 : 0, (long long) var_mask,
               failed ? 1 : 0, failed2 ? 1 : 0, n_preserve ? 1 : 0, n_effort ? 1 : 0);
    }
}


static void check_concat_parts(const std::vector<std::pair<std::string, std::string>> & parts) {
    common_chat_msg msg;
    msg.role = "user";
    for (const auto & pr : parts) {
        common_chat_msg_content_part p;
        p.type = pr.first;
        p.text = pr.second;
        msg.content_parts.push_back(p);
    }
    std::string want;
    try {
        auto j = common_chat_msgs_to_json_oaicompat(std::vector<common_chat_msg>{msg}, true);
        want = j.at(0).at("content").get<std::string>();
    } catch (const std::exception & e) {
        printf("FAIL concat oracle: %s\n", e.what());
        ++fails;
        return;
    }

    std::string blob;
    std::vector<int64_t> toff, tlen, xoff, xlen;
    for (const auto & pr : parts) {
        toff.push_back((int64_t) blob.size()); tlen.push_back((int64_t) pr.first.size());
        blob += pr.first;
        xoff.push_back((int64_t) blob.size()); xlen.push_back((int64_t) pr.second.size());
        blob += pr.second;
    }
    std::vector<uint8_t> out(1 << 16, 0);
    sl_u8 bs{(uint8_t *) blob.data(), (int64_t) blob.size()};
    sl_i64 to{toff.data(), (int64_t) toff.size()};
    sl_i64 tl{tlen.data(), (int64_t) tlen.size()};
    sl_i64 xo{xoff.data(), (int64_t) xoff.size()};
    sl_i64 xl{xlen.data(), (int64_t) xlen.size()};
    sl_u8 os{out.data(), (int64_t) out.size()};
    const int64_t n = chat_concat_parts(bs, to, tl, xo, xl, (int64_t) parts.size(), os);
    const std::string got((const char *) out.data(), (size_t) (n < 0 ? 0 : n));

    ++cases;
    ++values;
    if (got != want) {
        ++fails;
        printf("FAIL concat: got=[%s] want=[%s]\n", visible(got).c_str(), visible(want).c_str());
    }
}


static void check_content_shape(const std::vector<std::pair<std::string, std::string>> & parts) {
    common_chat_msg msg;
    msg.role = "user";
    for (const auto & pr : parts) {
        common_chat_msg_content_part p;
        p.type = pr.first;
        p.text = pr.second;
        msg.content_parts.push_back(p);
    }
    std::string want;
    try {
        auto j = common_chat_msgs_to_json_oaicompat(std::vector<common_chat_msg>{msg}, false);
        want = j.at(0).at("content").dump();
    } catch (const std::exception & e) {
        printf("FAIL content oracle: %s\n", e.what());
        ++fails;
        return;
    }

    std::string blob;
    std::vector<int64_t> toff, tlen, xoff, xlen;
    for (const auto & pr : parts) {
        toff.push_back((int64_t) blob.size()); tlen.push_back((int64_t) pr.first.size());
        blob += pr.first;
        xoff.push_back((int64_t) blob.size()); xlen.push_back((int64_t) pr.second.size());
        blob += pr.second;
    }
    std::vector<uint8_t> out(1 << 16, 0);
    sl_u8 bs{(uint8_t *) blob.data(), (int64_t) blob.size()};
    sl_i64 to{toff.data(), (int64_t) toff.size()};
    sl_i64 tl{tlen.data(), (int64_t) tlen.size()};
    sl_i64 xo{xoff.data(), (int64_t) xoff.size()};
    sl_i64 xl{xlen.data(), (int64_t) xlen.size()};
    sl_u8 os{out.data(), (int64_t) out.size()};
    const int64_t n = chat_content_json(bs, to, tl, xo, xl, (int64_t) parts.size(), os);
    const std::string raw((const char *) out.data(), (size_t) (n < 0 ? 0 : n));

    ++cases;
    ++values;
    bool same = false;
    try {
        same = common_json::parse(want).dump() == common_json::parse(raw).dump();
    } catch (const std::exception &) {
        same = false;
    }
    if (!same) {
        ++fails;
        printf("FAIL content shape: got=[%s] want=[%s]\n", visible(raw).c_str(), visible(want).c_str());
    }
}


static void check_message_fields(const common_chat_msg & msg) {
    std::string want;
    try {
        auto j = common_chat_msgs_to_json_oaicompat(std::vector<common_chat_msg>{msg}, false);
        want = j.at(0).dump();
    } catch (const std::exception & e) {
        printf("FAIL fields oracle: %s\n", e.what());
        ++fails;
        return;
    }

    std::string blob;
    auto put = [&blob](const std::string & s) {
        const int64_t off = (int64_t) blob.size();
        blob += s;
        return off;
    };
    const int64_t roff = put(msg.role), rlen = (int64_t) msg.role.size();
    const int64_t coff = put(msg.content), clen = (int64_t) msg.content.size();
    const int64_t eoff = put(msg.reasoning_content), elen = (int64_t) msg.reasoning_content.size();
    const int64_t noff = put(msg.tool_name), nlen = (int64_t) msg.tool_name.size();
    const int64_t ioff = put(msg.tool_call_id), ilen = (int64_t) msg.tool_call_id.size();
    std::vector<int64_t> tn_off, tn_len, ta_off, ta_len, ti_off, ti_len;
    for (const auto & tc : msg.tool_calls) {
        tn_off.push_back(put(tc.name)); tn_len.push_back((int64_t) tc.name.size());
        ta_off.push_back(put(tc.arguments)); ta_len.push_back((int64_t) tc.arguments.size());
        ti_off.push_back(put(tc.id)); ti_len.push_back((int64_t) tc.id.size());
    }
    std::vector<uint8_t> out(1 << 16, 0);
    sl_u8 bs{(uint8_t *) blob.data(), (int64_t) blob.size()};
    sl_i64 a1{tn_off.data(), (int64_t) tn_off.size()}; sl_i64 a2{tn_len.data(), (int64_t) tn_len.size()};
    sl_i64 a3{ta_off.data(), (int64_t) ta_off.size()}; sl_i64 a4{ta_len.data(), (int64_t) ta_len.size()};
    sl_i64 a5{ti_off.data(), (int64_t) ti_off.size()}; sl_i64 a6{ti_len.data(), (int64_t) ti_len.size()};
    sl_u8 os{out.data(), (int64_t) out.size()};
    const int64_t n = chat_message_full_json(bs, roff, rlen, coff, clen, eoff, elen, noff, nlen, ioff, ilen,
                                             a1, a2, a3, a4, a5, a6, (int64_t) msg.tool_calls.size(), os);
    const std::string raw((const char *) out.data(), (size_t) (n < 0 ? 0 : n));

    ++cases;
    ++values;
    bool same = false;
    try {
        same = common_json::parse(want).dump() == common_json::parse(raw).dump();
    } catch (const std::exception &) {
        same = false;
    }
    if (!same) {
        ++fails;
        printf("FAIL message fields: got=[%s]\n  want=[%s]\n", visible(raw).c_str(), visible(want).c_str());
    }
}


static bool json_equal(const std::string & a, const std::string & b) {
    try {
        return common_json::parse(a).dump() == common_json::parse(b).dump();
    } catch (const std::exception &) {
        return false;
    }
}

static void check_tools_json(const std::vector<common_chat_tool> & tools) {
    std::string want;
    try {
        want = common_chat_tools_to_json_oaicompat(tools).dump();
    } catch (const std::exception & e) {
        printf("FAIL tools oracle: %s\n", e.what());
        ++fails;
        return;
    }
    std::string blob;
    auto put = [&blob](const std::string & s) {
        const int64_t off = (int64_t) blob.size();
        blob += s;
        return off;
    };
    std::vector<int64_t> no, nl, dof, dl, po, pl;
    for (const auto & t : tools) {
        no.push_back(put(t.name)); nl.push_back((int64_t) t.name.size());
        dof.push_back(put(t.description)); dl.push_back((int64_t) t.description.size());
        po.push_back(put(t.parameters)); pl.push_back((int64_t) t.parameters.size());
    }
    std::vector<uint8_t> out(1 << 16, 0);
    sl_u8 bs{(uint8_t *) blob.data(), (int64_t) blob.size()};
    sl_i64 a1{no.data(), (int64_t) no.size()}, a2{nl.data(), (int64_t) nl.size()};
    sl_i64 a3{dof.data(), (int64_t) dof.size()}, a4{dl.data(), (int64_t) dl.size()};
    sl_i64 a5{po.data(), (int64_t) po.size()}, a6{pl.data(), (int64_t) pl.size()};
    sl_u8 os{out.data(), (int64_t) out.size()};
    const int64_t n = chat_tools_json(bs, a1, a2, a3, a4, a5, a6, (int64_t) tools.size(), os);
    const std::string raw((const char *) out.data(), (size_t) (n < 0 ? 0 : n));
    ++cases;
    ++values;
    if (!json_equal(raw, want)) {
        ++fails;
        printf("FAIL tools json: got=[%s] want=[%s]\n", visible(raw).c_str(), visible(want).c_str());
    }
}

static void check_tool_parameters(const std::string & param_json, bool has_param) {
    common_json fn = common_json::parse("{\"name\": \"x\"}");
    if (has_param) {
        fn["parameters"] = common_json::parse(param_json);
    }
    const std::string want = common_chat_tool_parameters(fn).dump();

    std::vector<uint8_t> out(1 << 16, 0);
    sl_u8 bs{(uint8_t *) param_json.data(), (int64_t) param_json.size()};
    sl_u8 os{out.data(), (int64_t) out.size()};
    const int64_t n = chat_tool_parameters_json(bs, 0, (int64_t) param_json.size(), os);
    const std::string raw((const char *) out.data(), (size_t) (n < 0 ? 0 : n));
    ++cases;
    ++values;
    if (!json_equal(raw, want)) {
        ++fails;
        printf("FAIL tool parameters: got=[%s] want=[%s]\n", visible(raw).c_str(), visible(want).c_str());
    }
}


static void check_tools_parse(const std::string & text, bool expect_ok) {
    std::vector<common_chat_tool> native;
    bool native_ok = true;
    std::string native_err;
    try {
        native = common_chat_tools_parse_oaicompat(common_json::parse(text));
    } catch (const std::exception & e) {
        native_ok = false;
        native_err = e.what();
    }

    const int64_t maxn = 8;
    std::vector<uint8_t> out(1 << 16, 0);
    std::vector<int64_t> offs((size_t) (3 * maxn), 0), lens((size_t) (3 * maxn), 0);
    std::vector<int64_t> failv(2, 0);
    sl_u8 bs{(uint8_t *) text.data(), (int64_t) text.size()};
    sl_u8 os{out.data(), (int64_t) out.size()};
    sl_i64 a1{offs.data(), (int64_t) offs.size()}, a2{lens.data(), (int64_t) lens.size()};
    sl_i64 a3{failv.data(), (int64_t) failv.size()};
    const int64_t n = chat_tools_parse(bs, maxn, os, a1, a2, a3);

    ++cases;
    ++values;
    (void) expect_ok;
    if (!native_ok) {
        if (n >= 0) {
            ++fails;
            printf("FAIL tools parse: %s native rejects but ours accepted (%lld tools)\n",
                   visible(text).c_str(), (long long) n);
            return;
        }
        if (native_err.rfind("Failed to parse tools: ", 0) == 0) {
            std::vector<uint8_t> errout(1 << 16, 0);
            sl_u8 eo{errout.data(), (int64_t) errout.size()};
            const int64_t m = chat_tools_error(bs, n, failv[0], failv[1], eo);
            const std::string got((const char *) errout.data(), (size_t) (m < 0 ? 0 : m));
            ++values;
            if (got != native_err) {
                ++fails;
                printf("FAIL tools error text: %s\n  got =[%s]\n  want=[%s]\n",
                       visible(text).c_str(), visible(got).c_str(), visible(native_err).c_str());
            }
        }
        return;
    }
    if (n < 0) {
        ++fails;
        printf("FAIL tools parse: %s native accepts but ours returned %lld\n", visible(text).c_str(), (long long) n);
        return;
    }
    if (n != (int64_t) native.size()) {
        ++fails;
        printf("FAIL tools parse count: %s ours=%lld native=%zu\n", visible(text).c_str(), (long long) n, native.size());
        return;
    }
    for (int64_t i = 0; i < n; ++i) {
        const std::string name((const char *) out.data() + offs[3 * i], (size_t) lens[3 * i]);
        const std::string desc((const char *) out.data() + offs[3 * i + 1], (size_t) lens[3 * i + 1]);
        const std::string parm((const char *) out.data() + offs[3 * i + 2], (size_t) lens[3 * i + 2]);
        bool same = name == native[i].name && desc == native[i].description;
        try {
            same = same && common_json::parse(parm).dump() == common_json::parse(native[i].parameters).dump();
        } catch (const std::exception &) {
            same = false;
        }
        if (!same) {
            ++fails;
            printf("FAIL tools parse field %lld: got name=[%s] desc=[%s] params=[%s]\n", (long long) i,
                   visible(name).c_str(), visible(desc).c_str(), visible(parm).c_str());
            return;
        }
    }
}

int main(void) {
    goose_init(0, 0);

    const std::vector<std::pair<std::string, std::string>> vars = {
        {"v", "a[i3,i4]"}, {"f", "f1.5"}, {"b", "bT"}, {"n", "n"}, {"u", "u\"u\""},
        {"w", "a[i1,i2,i3]"}, {"e", "a[]"}, {"one", "a[i1]"}, {"five", "i5"},
        {"t", "t[i1,i2]"}, {"o", "o{s\"k\"=i1,s\"j\"=i2}"},
        {"od", "o{s\"a\"=i2,s\"b\"=i1}"},
        {"o2", "o{s\"k\"=i3}"},
        {"vv", "a[t[i1,i2],t[i3,i4]]"},
    };
    const std::vector<std::string> tpls = {
        "{% for k in o %}{{ k }}{% endfor %}",
        "", "plain", "a{{ 1 }}b", "{{ v }}", "{{ f }}", "{{ b }}", "{{ n }}", "{{ u }}",
        "{{ [1,2] }}", "{% set x = 5 %}{{ x }}",
        "{% set x = 5 %}{% set y = x + 1 %}{{ y }}",
        "{% if 1 %}yes{% endif %}", "{% if 0 %}no{% else %}yes{% endif %}",
        "{% if 0 %}a{% elif 1 %}b{% else %}c{% endif %}",
        "{% set x = 2 %}{% if x > 1 %}big{% endif %}",
        "{% if 1 %}{% if 1 %}nested{% endif %}{% endif %}",
        "{% if 1 %}{% set z = 7 %}{% endif %}{{ z }}",
        "{% set s %}ab{{ 1 }}{% endset %}{{ s }}",
        "{% set x = 1 %}{% if x %}in{{ x }}{% endif %}out",
        "{% set a, b = [1, 2] %}{{ a }}{{ b }}",
        "{% set a, b = v %}{{ a }}|{{ b }}",
        "{% set b = 9 %}{{ b }}",
        "x\ny",
        "{% for x in w %}{{ x }}{% endfor %}",
        "{% for x in w %}{{ loop.index }}{{ loop.index0 }}{{ loop.revindex }}{{ loop.revindex0 }}{% endfor %}",
        "{% for x in w %}{{ loop.first }}{{ loop.last }}{% endfor %}",
        "{% for x in w %}{{ loop.length }}{% endfor %}",
        "{% for x in w %}{{ x }}{% if not loop.last %}{{ loop.nextitem }}{% endif %}{% endfor %}",
        "{% for x in w %}{% if not loop.first %}{{ loop.previtem }}{% endif %}{{ x }}{% endfor %}",
        "{% for x in e %}a{% else %}empty{% endfor %}",
        "{% for x in v %}i{% else %}E{% endfor %}",
        "{% for x in w %}{{ x }}{% if x == 2 %}{% break %}{% endif %}b{% endfor %}end",
        "{% for x in w %}{% if x == 2 %}{% continue %}{% endif %}{{ x }}{% endfor %}",
        "{% for x in w %}{% break %}{% else %}E{% endfor %}",
        "{% for x in v %}{% continue %}{% else %}E{% endfor %}",
        "{% for k, val in o %}{{ k }}={{ val }};{% endfor %}",
        "{% for x in t %}{{ x }}{% endfor %}",
        "{% for x in w if x > 1 %}{{ x }}{% endfor %}",
        "{% for x in w if x > 1 %}{{ loop.index }}/{{ loop.length }}{% endfor %}",
        "{% set y = 9 %}{% for x in v %}{% set y = x %}{{ y }}{% endfor %}{{ y }}",
        "{% for x in v %}{% for z in v %}{{ x }}{{ z }}{% endfor %}{% endfor %}",
        "{% for a, b in vv %}{{ a }}-{{ b }};{% endfor %}",
        "{% for x in missing %}{{ x }}{% else %}none{% endfor %}",
        "{% for k in o %}{{ loop.length }}{% endfor %}",
        "{% for k in o %}{% if k == \"k\" %}S{% else %}T{% endif %}{% endfor %}",
        "{% for k in o %}{{ k[0] }}{% endfor %}",
        // rejected
        "{% for a, b, c in v %}{% endfor %}",
        "{% for a, b, c in o %}{% endfor %}",
        "{% macro hi() %}hello{% endmacro %}{{ hi() }}",
        "{% macro hi() %}{{ 1 + 2 }}{% endmacro %}{{ hi() }}",
        "{% macro m() %}x{% endmacro %}{{ m() }}{{ m() }}",
        "{% set v = 7 %}{% macro m() %}{{ v }}{% endmacro %}{{ m() }}",
        "{% macro m() %}{% set z = 5 %}{{ z }}{% endmacro %}{{ m() }}",
        "{% macro a() %}A{% endmacro %}{% macro b() %}{{ a() }}B{% endmacro %}{{ b() }}",
        "{% macro m() %}t{% endmacro %}",
        "{% macro m() %}{% for x in v %}{{ x }}{% endfor %}{% endmacro %}{{ m() }}",
        "{{ 5() }}",
        "{{ nope() }}",
        "{% macro add(a, b) %}{{ a + b }}{% endmacro %}{{ add(2, 3) }}",
        "{% macro m(a, b=10) %}{{ a }}{{ b }}{% endmacro %}{{ m(1) }}",
        "{% macro m(a, b=2) %}{{ a }}{{ b }}{% endmacro %}{{ m(1, 9) }}",
        "{% macro m(a) %}{{ a }}{% endmacro %}{{ m(1, 2) }}",
        "{{ 'abc'|upper }}",
        "{{ 'ABC'|lower }}",
        "{{ 'hELLO'|capitalize }}",
        "{{ 'abc'|count }}",
        "{{ 123|upper }}",
        "{{ 'ab'|upper|lower }}",
        "{{ '  hi '|strip }}",
        "{{ '\t x \t'|strip }}",
        "{{ 'xxaxx'|strip('x') }}",
        "{{ 'x'|nope }}",
        "{{ 'a-b-a'|replace('-', '+') }}",
        "{{ 'ab'|replace('', '-') }}",
        "{{ [1,2,3]|count }}",
        "{{ o|length }}",
        "{{ t|count }}",
        "{{ 'a-b'|replace('a-b', '+') }}",
        "{{ 'xax'|replace('x', '--') }}",
        "{{ '  '|strip }}",
        "{{ 'hello wORLD'|title }}",
        "{{ 'a  b'|title }}",
        "{{ 5|title }}",
        "{{ '  x '|lstrip }}",
        "{{ '  x '|rstrip }}",
        "{{ 'xxax'|lstrip('x') }}",
        "{{ 'axxx'|rstrip('x') }}",
        "{{ 5|lstrip }}",
        "{{ v|first }}",
        "{{ v|last }}",
        "{{ e|first }}",
        "{{ t|last }}",
        "{{ w|join('-') }}",
        "{{ w|join }}",
        "{{ e|join(',') }}",
        "{{ [1, 'a']|join('-') }}",
        "{{ u|title }}",
        "{{ u|length }}",
        "{{ u|first }}",
        "{{ [u|first] }}",
        "{{ [e|first] }}",
        "{{ (u|first) == '' }}",
        "{{ 'a b'|wordcount }}",
        "{{ 'a\"b'|tojson }}",
        "{{ 5|tojson }}",
        "{{ 1.5|tojson }}",
        "{{ v|tojson }}",
        "{{ o|tojson }}",
        "{{ b|tojson }}",
        "{{ n|tojson }}",
        "{{ u|tojson }}",
        "{{ 'a'|string }}",
        "{{ 'a'|safe }}",
        "{{ 5|string }}",
        "{{ 1.5|string }}",
        "{{ b|string }}",
        "{{ n|string }}",
        "{{ u|string }}",
        "{{ v|string }}",
        "{{ o|string }}",
        "{{ v|safe }}",
        "{{ '42'|int }}",
        "{{ '  -7xyz'|int }}",
        "{{ 'x'|int }}",
        "{{ 'x'|int(9) }}",
        "{{ '4.9'|float }}",
        "{{ '1e3'|float }}",
        "{{ 'x'|float }}",
        "{{ 'x'|float(2.5) }}",
        "{{ 4.9|int }}",
        "{{ 5|float }}",
        "{{ b|int }}",
        "{{ b|float }}",
        "{{ n|int }}",
        "{{ 1 is defined }}",
        "{{ u is defined }}",
        "{{ u is undefined }}",
        "{{ n is none }}",
        "{{ n is defined }}",
        "{{ 'a' is none }}",
        "{{ 1 is none }}",
        "{{ b is boolean }}",
        "{{ b is true }}",
        "{{ b is false }}",
        "{{ 1 is number }}",
        "{{ 1.5 is number }}",
        "{{ 'a' is string }}",
        "{{ 1 is integer }}",
        "{{ 1.5 is float }}",
        "{{ v is iterable }}",
        "{{ o is mapping }}",
        "{{ v is sequence }}",
        "{{ 3 is odd }}",
        "{{ 4 is even }}",
        "{{ 9 is divisibleby(3) }}",
        "{{ 9 is divisibleby(2) }}",
        "{{ 'abc' is lower }}",
        "{{ 'ABC' is upper }}",
        "{{ 'Abc' is lower }}",
        "{{ 1 is eq(1) }}",
        "{{ 1 is equalto(2) }}",
        "{{ 1 is ne(2) }}",
        "{{ 2 is in(v) }}",
        "{{ 9 is in(v) }}",
        "{{ 'b' is in('abc') }}",
        "{{ 1 is not defined }}",
        "{{ u is not defined }}",
        "{{ 1 is nope }}",
        "{{ 1 is lt(2) }}",
        "{{ 2 is lt(1) }}",
        "{{ 1 is lessthan(2) }}",
        "{{ 2 is gt(1) }}",
        "{{ 2 is greaterthan(1) }}",
        "{{ 2 is greaterthan(2) }}",
        "{{ 2 is ge(2) }}",
        "{{ 2 is ge(3) }}",
        "{{ 1.5 is gt(1) }}",
        "{{ 'a' is lt('b') }}",
        "{{ '10' is gt(9) }}",
        "{{ 1 is not gt(2) }}",
        "{{ b is gt(false) }}",
        "{{ 'abc'|startswith('ab') }}",
        "{{ 'abc'|startswith('bc') }}",
        "{{ 'abc'|endswith('bc') }}",
        "{{ 'abc'|endswith('ab') }}",
        "{{ 'abc'|startswith('') }}",
        "{{ ''|startswith('') }}",
        "{{ 'a,b,c'|split(',') }}",
        "{{ ('a,b,c'|split(','))|length }}",
        "{{ ('a,b,c'|split(','))|first }}",
        "{{ 'a,b,c'|split(',', 1) }}",
        "{{ 'a,b,c'|split(',', 0) }}",
        "{{ 'a b'|split }}",
        "{{ 'abc'|split(',') }}",
        "{{ ''|split(',') }}",
        "{{ 'a,,b'|split(',') }}",
        "{{ 'a,b,c'|rsplit(',') }}",
        "{{ 'a,b,c'|rsplit(',', 1) }}",
        "{{ 'a b c'|rsplit }}",
        "{{ 'abc'|rsplit(',') }}",
        "{{ 'a,,b'|rsplit(',') }}",
        "{{ 'a'|split('') }}",
        "{{ 5|split(',') }}",
        "{{ u|split(',') }}",
        "{{ 'abc'|truncate(2) }}",
        "{{ u|truncate }}",
        "{{ o|keys }}",
        "{{ o|values }}",
        "{{ o|items }}",
        "{{ o|keys|length }}",
        "{{ o|get('k') }}",
        "{{ o|get('zz') }}",
        "{{ o|get('zz', 7) }}",
        "{{ v|sort }}",
        "{{ [o, o]|map(attribute='k')|tojson }}",
        "{{ ([o, v]|select('k'))|length }}",
        "{{ ([o, v]|reject('k'))|length }}",
        "{{ o|dictsort|keys }}",
        "{{ o|dictsort(none, 'value')|keys }}",
        "{{ o|dictsort(none, 'value', true)|keys }}",
        "{{ o|dictsort|tojson }}",
        "{{ od|dictsort(none, 'value')|keys }}",
        "{{ [1,2,3]|reverse }}",
        "{{ (1,2)|reverse|string }}",
        "{{ o|list }}",
        "{{ v|list|length }}",
        "{{ v|min }}",
        "{{ v|max }}",
        "{{ e|min }}",
        "{{ ([o,o]|max(attribute='k'))|tojson }}",
        "{{ [o, o2]|max(attribute='k')|tojson }}",
        "{{ [o, o2]|min(attribute='k')|tojson }}",
        "{{ [1,2]|append(3) }}",
        "{{ ([1,2]|append(3))|length }}",
        "{{ [1,2,3]|pop }}",
        "{{ [1,2,3]|pop(0) }}",
        "{{ [1,2,3]|pop(-2) }}",
        "{{ [o,o]|join('-', attribute='k') }}",
        "{{ u|sum }}",
        "{{ v|sum }}",
        "{{ range(3) }}",
        "{{ range(1, 4) }}",
        "{{ range(4, 0, -2) }}",
        "{{ range(0) }}",
        "{{ range(1, 2, 0) }}",
        "{% set f = range %}{{ f(3) }}",
        "{{ range is callable }}",
        "{{ raise_exception is callable }}",
        "{% set e = raise_exception %}{{ e('boom') }}",
        "{% macro m(x) %}{{ x }}{% endmacro %}{% set f = m %}{{ f(7) }}",
        "{% macro a() %}A{% endmacro %}{% macro b(f) %}{{ f() }}{% endmacro %}{{ b(a) }}",
        "{{ range is defined }}",
        "{% for i in range(3) %}{{ i }}{% endfor %}",
        "{{ (range(3))|length }}",
        "{{ namespace(a=1, b=2)|tojson }}",
        "{{ dict(a=1)|keys }}",
        "{{ dict(o)|keys }}",
        "{{ dict(o)|tojson }}",
        "{{ dict(o)|length }}",
        "{{ dict([['a', 1]])|tojson }}",
        "{{ namespace is callable }}",
        "{{ dict()|length }}",
        "{% set ns = namespace(a=1) %}{{ ns|tojson }}",
        "{{ dict(1) }}",
        "{{ dict(o, a=9)|keys }}",
        "{{ dict([['a', 1, 2]])|tojson }}",
        "{{ dict(o, o) }}",
        "{{ (v|select('gt', 3))|length }}",
        "{{ (v|reject('gt', 3))|length }}",
        "{{ ([o, o2]|select('k', 'eq', 1))|length }}",
        "{{ ([o, o2]|reject('k', 'eq', 1))|length }}",
        "{{ [o, v]|map(attribute='k', default=0)|tojson }}",
        "{{ (v|select('nope', 1))|length }}",
        "{{ 'ab'.upper() }}",
        "{{ 'AB'.lower() }}",
        "{{ '  x '.strip() }}",
        "{{ ('a,b'.split(','))|length }}",
        "{{ 'abc'.startswith('ab') }}",
        "{{ v.first() }}",
        "{{ v.length() }}",
        "{{ o.keys()|length }}",
        "{{ o.get('k') }}",
        "{{ 'abc'.replace('b', 'X') }}",
        "{{ v.sort() }}",
        "{{ 'ab'.upper }}",
        "{% set ox = namespace(keys=5) %}{{ (ox.keys())|length }}",
        "{% macro m(x) %}{{ x }}{% endmacro %}{{ m(1) }}{{ 'abc'.replace('b', 'X') }}",
        "{{ (5).abs() }}",
        "{{ (0-3).abs() }}",
        "{{ ((0-1.5)).abs() }}",
        "{{ (5).int() }}",
        "{{ (5).float() }}",
        "{{ (1.5).int() }}",
        "{{ (5).string() }}",
        "{{ (5).tojson() }}",
        "{{ true.int() }}",
        "{{ true.string() }}",
        "{{ true.abs() }}",
        "{{ (0-3)|abs }}",
        "{{ true|abs }}",
        "{{ true.tojson() }}",
        "{{ u|default('x') }}",
        "{{ n|default('x') }}",
        "{{ ''|default('x') }}",
        "{{ ''|default('x', true) }}",
        "{{ 0|default('x', true) }}",
        "{{ 0|default('x') }}",
        "{{ 'a'|default('x') }}",
        "{{ ''|default('x', boolean=true) }}",
        "{{ 'x'.default('y') }}",
        "{{ true.default('y') }}",
        "{{ (5).default(9) }}",
        "{{ v.default('y')|length }}",
        "{{ o|default('x')|tojson }}",
        "{{ (0-0)|default(7, true) }}",
        "{{ u|default(7) }}",
        "{{ 'a'.upper is callable }}",
        "{{ ('ab'.upper()).lower() }}",
        "{{ (o, o)|map(attribute='k')|string }}",
        "{{ ([o, v]|reject('k'))|first }}",
        "{{ ([o]|select('zz'))|length }}",
        "{{ [v]|map(attribute=1) }}",
        "{{ w|sort }}",
        "{{ v|sort(true) }}",
        "{{ [3,1,3,2,1]|unique }}",
        "{{ ['b','a']|sort }}",
        "{{ t|sort }}",
        "{{ 5|sort }}",
        "{{ o|sort }}",
        "{{ '{} and {}'|format('a', 1) }}",
        "{{ 'none here'|format }}",
        "{{ '{}{}'|format('x', 1.5) }}",
        "{{ 'bad {x}'|format('a') }}",
        "{{ '{'|format('a') }}",
        "{{ '{{}}'|format('a') }}",
        "{{ 5|format('a') }}",
        "{{ 'a\nb'|indent(2) }}",
        "{{ 'a\nb'|indent(2, true) }}",
        "{{ '\nab'|indent(2) }}",
        "{{ 'a\nb'|indent(2, true, true) }}",
        "{{ 'a\n\nb'|indent(2, false, true) }}",
        "{{ 'a\n'|indent(2) }}",
        "{{ ''|indent(2) }}",
        "{{ 'a\nb'|indent('> ') }}",
        "{{ 'a\nb'|indent }}",
        "{{ '\nab'|indent(2, true) }}",
        "{% macro m(b, a) %}{{ a }}{{ b }}{% endmacro %}{{ m(b=2, a=1) }}",
        "{% macro m(a, b=2) %}{{ a }}{{ b }}{% endmacro %}{{ m(b=9, a=1) }}",
        "{% macro m(x) %}{{ x }}{% endmacro %}{{ m(x=7) }}",
        "{% macro m(a, b) %}{{ a }}{{ b }}{% endmacro %}{{ m(1, b=2) }}",
        "{% macro wrap() %}[{{ caller() }}]{% endmacro %}{% call wrap() %}body{% endcall %}",
        "{% macro each() %}{{ caller(1) }}{{ caller(2) }}{% endmacro %}{% call(x) each() %}{{ x }}{% endcall %}",
        "{% macro tw() %}<{{ caller() }}>{% endmacro %}{% call tw() %}{% for x in v %}{{ x }}{% endfor %}{% endcall %}",
        "{% macro m(a) %}{{ a }}{% endmacro %}{{ m() }}",
        "{% macro m(a) %}[{{ a }}]{% endmacro %}{% macro n(a) %}{{ m(a) }}{% endmacro %}{{ n(7) }}",
        "{% set a = 9 %}{% macro m(a) %}{{ a }}{% endmacro %}{{ m(3) }}{{ a }}",
        "{% macro m(a, b, c=3) %}{{ a }}{{ b }}{{ c }}{% endmacro %}{{ m(1, 2) }}",
        "{% for a, 1 in v %}{% endfor %}",
        "{% for a, b in one %}{% endfor %}",
        "{% for x in five %}{% endfor %}",
        "{% set a, b = 1 %}",
        "{% set a, b = [1] %}",
        "{% set a, b, c = [1, 2] %}",
        "{% set a, 1 = [1, 2] %}",
    };
    for (const auto & t : tpls) check_tpl(t, vars);

    const std::string chat_tmpl =
        "{% for m in messages %}<{{ m['role'] }}>{{ m['content'] }}</{{ m['role'] }}>{% endfor %}"
        "{% if add_generation_prompt %}<assistant>{% endif %}";
    check_chat(chat_tmpl, {{"user", "U"}}, true);
    check_chat(chat_tmpl, {{"user", "U"}}, false);
    check_chat(chat_tmpl, {{"system", "S"}, {"user", "U"}}, true);
    check_chat("{% for m in messages %}{{ loop.index }}:{{ m['role'] }};{% endfor %}",
               {{"user", "U"}, {"assistant", "A"}}, false);
    check_chat("[{{ messages|length }}][{{ messages[1]['content']|default('x') }}]",
               {{"user", ""}, {"assistant", "A"}}, false);

    check_chat_shape("user", "U");
    check_chat_shape("user", "");
    check_chat_shape("system", "a\"b");
    check_chat_shape("assistant", "line1\nline2");
    check_chat_shape("user", "tab\there");
    check_chat_shape("user", "back\\slash");
    check_chat_shape("tool", "{}");
    check_chat_shape("user", "unicode: \xd1\x8e\xd0\xbd\xd0\xb8");

    const std::string chat_loop =
        "{% for m in messages %}<{{ m['role'] }}>{{ m['content'] }}</{{ m['role'] }}>{% endfor %}"
        "{% if add_generation_prompt %}<assistant>{% endif %}";
    const std::string role_only = "{% for m in messages %}{{ m['role'] }};{% endfor %}";

    check_chat_roles(chat_loop, {{"system", "S"}, {"user", "U"}}, true);
    check_chat_roles(role_only, {{"system", "S"}, {"user", "U"}}, false);
    check_chat_roles(role_only, {{"system", "S"}}, false);
    check_chat_roles(role_only, {{"developer", "D"}, {"user", "U"}}, false);
    check_chat_roles(chat_loop, {{"developer", "D"}, {"user", "U"}}, false);
    check_chat_roles(chat_loop, {{"system", ""}, {"user", "U"}}, false);
    check_chat_roles("{% for m in messages %}{% if m['role'] != 'system' %}[{{ m['content'] }}]{% endif %}{% endfor %}",
                     {{"system", "S"}, {"user", "U"}}, false);


    check_caps(chat_loop);
    check_caps(role_only);
    check_caps("{% for m in messages %}{% if m['role'] != 'system' %}[{{ m['content'] }}]{% endif %}{% endfor %}");
    check_caps("{{ messages[0]['content'] }}");

    {
        const int64_t valcap = 64, poolcap = 256;
        std::vector<int64_t> A((size_t) (32 + 11 * valcap + poolcap), 0);
        A[0] = valcap;
        A[5] = poolcap;
        std::vector<double> F((size_t) valcap, 0.0);
        std::vector<uint8_t> B(1 << 12, 0);
        std::vector<int64_t> st(64, 0);
        const int64_t fresh = chat_probe_fresh(sli(A), slf(F), slv(B), sli(st));
        ++cases;
        ++values;
        if (fresh != 0) {
            ++fails;
            printf("FAIL caps: a fresh value starts used (%lld)\n", (long long) fresh);
        }
    }

    check_caps_content("{{ messages[0]['content'] }}");
    check_caps_content("{{ messages[0]['content'][0] }}");
    check_caps_content("{{ messages[0]['content']|string }}");
    check_caps_content("{% for c in messages[0]['content'] %}{{ c }}{% endfor %}");
    check_caps_content("{{ (messages[0]['content']|select('gt', 0))|length }}");

    check_caps_tools("{% for t in tools %}{{ t['function']['name'] }}{% endfor %}");
    check_caps_tools("{% for m in messages %}{% if m['tool_calls'] %}{{ m['tool_calls'][0]['function']['name'] }}{% endif %}{% endfor %}");
    check_caps_tools("plain");

    check_caps_extra("{% for m in messages %}{% if m['tool_calls'] %}{{ m['tool_calls'][0]['function']['arguments']['arg'] }}{% endif %}{% endfor %}");
    check_caps_extra("{% for m in messages %}{% if m['tool_calls'] %}{{ m['tool_calls'][1]['function']['name'] }}{% endif %}{% endfor %}");
    check_caps_extra("plain");

    check_caps_reasoning("{% for m in messages %}{% if m['reasoning_content'] %}{{ m['reasoning_content'] }}{% endif %}{% endfor %}");
    check_caps_reasoning("{{ reasoning_effort }}");
    check_caps_reasoning("plain");

    check_concat_parts({{"text", "a"}, {"text", "b"}});
    check_concat_parts({{"text", "a"}, {"media_marker", "<media>"}, {"text", "b"}});
    check_concat_parts({{"text", ""}, {"text", "b"}});
    check_concat_parts({{"text", "a"}, {"image_url", "ignored"}, {"text", "b"}});
    check_concat_parts({{"text", "only"}});
    check_concat_parts({{"media_marker", "<m>"}, {"text", "b"}});
    check_concat_parts({{"text", "a"}, {"media_marker", ""}, {"media_marker", "<m2>"}, {"text", "b"}});
    check_concat_parts({{"text", "a\\nb"}, {"text", "c"}});

    check_content_shape({{"text", "a"}, {"text", "b"}});
    check_content_shape({{"text", "a"}, {"media_marker", "<m>"}, {"text", "b"}});
    check_content_shape({{"media_marker", "<m>"}});
    check_content_shape({{"text", "only"}});
    check_content_shape({{"image_url", "ignored"}});
    check_content_shape({{"text", "a"}, {"image_url", "x"}, {"text", "b"}});
    check_content_shape({{"text", "a"}, {"media_marker", "<m>"}, {"image_url", "x"}, {"text", "b"}});
    check_content_shape({});
    check_content_shape({{"text", "a\nb"}, {"text", "c"}});

    {
        common_chat_msg m; m.role = "user"; m.content = "hi";
        check_message_fields(m);
        m.reasoning_content = "why";
        check_message_fields(m);
        m.tool_name = "tn"; m.tool_call_id = "call1";
        check_message_fields(m);
        common_chat_msg a; a.role = "assistant"; a.content = "";
        common_chat_tool_call tc; tc.name = "tool1"; tc.arguments = "{\"arg\": \"value\"}";
        a.tool_calls.push_back(tc);
        check_message_fields(a);
        a.tool_calls[0].id = "call00001";
        check_message_fields(a);
        common_chat_tool_call tc2; tc2.name = "tool2"; tc2.arguments = "{}"; tc2.id = "call2";
        a.tool_calls.push_back(tc2);
        check_message_fields(a);
        common_chat_msg q; q.role = "user"; q.content = "line\nquote\"end";
        check_message_fields(q);
    }

    check_tools_json({});
    {
        common_chat_tool t; t.name = "tool1"; t.description = "d"; t.parameters = "{\"type\": \"object\"}";
        check_tools_json({t});
        t.description = "";
        check_tools_json({t});
        common_chat_tool t2; t2.name = "tool2"; t2.description = "e"; t2.parameters = "{\"a\": 1}";
        check_tools_json({t, t2});
    }
    check_tool_parameters("", false);
    check_tool_parameters("null", true);
    check_tool_parameters("{}", true);
    check_tool_parameters("{\"type\": \"object\", \"properties\": {\"x\": {\"type\": \"string\"}}}", true);

    check_tools_parse("[]", true);
    check_tools_parse("[{\"type\": \"function\", \"function\": {\"name\": \"tool1\", \"parameters\": {\"type\": \"object\"}}}]", true);
    check_tools_parse("[{\"type\": \"function\", \"function\": {\"name\": \"tool1\", \"description\": \"d\", \"parameters\": {\"a\": 1}}}]", true);
    check_tools_parse("[{\"type\": \"function\", \"function\": {\"name\": \"a\"}}, {\"type\": \"function\", \"function\": {\"name\": \"b\", \"description\": \"e\"}}]", true);
    check_tools_parse("null", true);
    check_tools_parse("{}", false);
    check_tools_parse("[{\"function\": {\"name\": \"x\"}}]", false);
    check_tools_parse("[{\"type\": \"tool\", \"function\": {\"name\": \"x\"}}]", false);
    check_tools_parse("[{\"type\": \"function\"}]", false);
    check_tools_parse("[{\"type\": \"function\", \"function\": {}}]", false);
    check_tools_parse("[not json", false);
    check_tools_parse("[{\"type\": \"function\", \"function\": {\"name\": \"a\\\"b\", \"description\": \"c\\\\d\"}}]", true);
    check_tools_parse("[{\"type\": \"function\", \"function\": {\"name\": \"a\\nb\", \"description\": \"t\\tu\"}}]", true);
    check_tools_parse("[{\"type\": \"function\", \"function\": {\"name\": \"\\u00e9\", \"description\": \"\\u00e9x\"}}]", true);
    check_tools_parse("[{\"type\": \"function\", \"function\": {\"name\": \"\\ud83d\\ude00\"}}]", true);
    check_tools_parse("[{\"type\": \"function\", \"function\": {\"name\": \"a\\/b\"}}]", true);

    printf("FG542_RUNTIME_TOOLMSG cases=%d values=%d failures=%d\n", cases, values, fails);
    return fails ? 1 : 0;
}
