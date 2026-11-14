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
                      const std::vector<std::pair<std::string, std::string>> & vars) {
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
    const int64_t astcap = an;
    const int64_t astpoolcap = (int64_t) a.pool.size() + 64;
    std::vector<int64_t> A((size_t) (32 + 10 * valcap + poolcap + 12 * astcap + astpoolcap), 0);
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
    for (size_t i = 0; i < e.pool.size(); ++i) A[(size_t) (vb + 10 * valcap + i)] = e.pool[i];
    const int64_t ab = 32 + 10 * valcap + poolcap;
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
    const std::string got((const char *) out.data(), (size_t) len);
    if (got != want) bad("output", src + "\n  got =[" + visible(got) + "]\n  want=[" + visible(want) + "]");
}

int main(void) {
    goose_init(0, 0);

    const std::vector<std::pair<std::string, std::string>> vars = {
        {"v", "a[i3,i4]"}, {"f", "f1.5"}, {"b", "bT"}, {"n", "n"}, {"u", "u\"u\""},
        {"w", "a[i1,i2,i3]"}, {"e", "a[]"}, {"one", "a[i1]"}, {"five", "i5"},
        {"t", "t[i1,i2]"}, {"o", "o{s\"k\"=i1,s\"j\"=i2}"},
        {"od", "o{s\"a\"=i2,s\"b\"=i1}"},
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

    printf("FG520_RUNTIME_SEL cases=%d values=%d failures=%d\n", cases, values, fails);
    return fails ? 1 : 0;
}
