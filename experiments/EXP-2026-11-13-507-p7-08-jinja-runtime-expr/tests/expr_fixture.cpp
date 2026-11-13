// Differential fixture for the scalar Jinja built-ins (EXP506).
// Oracle: jinja::value_int_t::get_builtins and value_float_t::get_builtins
// from common/jinja/value.cpp, invoked through func_args.
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
    if (fails <= 8) printf("FAIL %s: %s\n", what, detail.c_str());
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
    std::vector<int64_t> kind, pos, ival, soff, slen, c0, c1;
    std::vector<double> fval;
    std::vector<uint8_t> & B;
    explicit AstEnc(std::vector<uint8_t> & b) : B(b) {}
    int64_t put(const std::string & t) {
        const int64_t o = (int64_t) B.size();
        B.insert(B.end(), t.begin(), t.end());
        return o;
    }
    int enc(const jinja::statement * s) {
        const int id = (int) kind.size();
        kind.push_back(0); pos.push_back((int64_t) s->pos); ival.push_back(0);
        soff.push_back(0); slen.push_back(0); c0.push_back(-1); c1.push_back(-1);
        fval.push_back(0.0);
        if (auto x = dynamic_cast<const jinja::integer_literal *>(s)) {
            kind[id] = 13; ival[id] = x->val;
        } else if (auto x = dynamic_cast<const jinja::float_literal *>(s)) {
            kind[id] = 14; fval[id] = x->val;
        } else if (auto x = dynamic_cast<const jinja::string_literal *>(s)) {
            kind[id] = 15; soff[id] = put(x->val); slen[id] = (int64_t) x->val.size();
        } else if (auto x = dynamic_cast<const jinja::identifier *>(s)) {
            kind[id] = 12; soff[id] = put(x->val); slen[id] = (int64_t) x->val.size();
        } else if (auto x = dynamic_cast<const jinja::binary_expression *>(s)) {
            kind[id] = 19; soff[id] = put(x->op.value); slen[id] = (int64_t) x->op.value.size();
            c0[id] = enc(x->left.get()); c1[id] = enc(x->right.get());
        } else if (auto x = dynamic_cast<const jinja::unary_expression *>(s)) {
            kind[id] = 24; soff[id] = put(x->op.value); slen[id] = (int64_t) x->op.value.size();
            c0[id] = enc(x->argument.get());
        } else {
            throw std::runtime_error("unsupported AST node");
        }
        return id;
    }
};

static void check_expr(const std::string & expr,
                       const std::vector<std::pair<std::string, std::string>> & vars) {
    const std::string src = "{{ " + expr + " }}";
    jinja::lexer lx;
    auto lr = lx.tokenize(src);
    auto prog = jinja::parse_from_tokens(lr);
    const jinja::statement * root = prog.body[0].get();

    // native context and execution
    jinja::context ctx("");
    std::vector<std::pair<std::string, jinja::value>> cvals;
    for (const auto & kv : vars) {
        VP p(kv.second);
        auto v = p.parse();
        ctx.set_val(kv.first, v);
        cvals.push_back({kv.first, v});
    }
    // the native context also defines these names
    cvals.push_back({"true",  jinja::mk_val<jinja::value_bool>(true)});
    cvals.push_back({"True",  jinja::mk_val<jinja::value_bool>(true)});
    cvals.push_back({"false", jinja::mk_val<jinja::value_bool>(false)});
    cvals.push_back({"False", jinja::mk_val<jinja::value_bool>(false)});
    cvals.push_back({"none",  jinja::mk_val<jinja::value_none>()});
    cvals.push_back({"None",  jinja::mk_val<jinja::value_none>()});

    bool nok = true;
    std::string want, nerr;
    try {
        auto res = root->execute(ctx);
        want = jinja::value_to_json(res);
    } catch (const std::exception & e) {
        nok = false;
        nerr = e.what();
    }

    // flat encoding
    Enc e;
    std::vector<int64_t> croot;
    for (const auto & kv : cvals) croot.push_back(e.enc(kv.second));
    AstEnc a(e.B);
    const int64_t aroot = a.enc(root);

    const int64_t vn = (int64_t) e.kind.size();
    const int64_t valcap = vn + 32;
    const int64_t poolcap = (int64_t) e.pool.size() + 32;
    const int64_t an = (int64_t) a.kind.size();
    const int64_t astcap = an;
    std::vector<int64_t> A((size_t) (32 + 10 * valcap + poolcap + 7 * astcap), 0);
    A[0] = valcap; A[1] = vn; A[2] = (int64_t) e.pool.size(); A[3] = astcap; A[5] = poolcap;
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
    }

    // context names live after all strings
    std::vector<int64_t> noff, nlen;
    for (const auto & kv : cvals) {
        noff.push_back((int64_t) e.B.size());
        nlen.push_back((int64_t) kv.first.size());
        e.B.insert(e.B.end(), kv.first.begin(), kv.first.end());
    }
    const int64_t blen = (int64_t) e.B.size();

    std::vector<double> F((size_t) (valcap + astcap), 0.0);
    for (int64_t i = 0; i < vn; ++i) F[(size_t) i] = e.F[(size_t) i];
    for (int64_t i = 0; i < an; ++i) F[(size_t) (valcap + i)] = a.fval[(size_t) i];

    std::vector<uint8_t> out(1 << 20, 0), E(1 << 16, 0);
    std::vector<uint8_t> BB(1 << 20, 0);
    memcpy(BB.data(), e.B.data(), e.B.size());
    std::vector<int64_t> st(40, 0);
    ++cases;
    const int64_t len = eval_expr(aroot, (int64_t) cvals.size(), sli(noff), sli(nlen), sli(croot),
        blen, sli(A), slf(F), slv(BB), slv(E), slv(out), sli(st));

    if (!nok) {
        ++values;
        if (len >= 0 || st[1] == 0) { bad("expected rejection", expr); return; }
        ++values;
        const std::string gerr((const char *) E.data(), (size_t) st[2]);
        if (gerr != nerr) bad("error text", expr + "\n  got =[" + visible(gerr) + "]\n  want=[" + visible(nerr) + "]");
        return;
    }
    ++values;
    if (len < 0) {
        const std::string gerr((const char *) E.data(), (size_t) st[2]);
        bad("unexpected rejection", expr + ": " + visible(gerr));
        return;
    }
    ++values;
    const std::string got((const char *) out.data(), (size_t) len);
    if (got != want) bad("value", expr + "\n  got =[" + visible(got) + "]\n  want=[" + visible(want) + "]");
}

int main(void) {
    goose_init(0, 0);

    const std::vector<std::pair<std::string, std::string>> vars = {
        {"x", "i5"}, {"y", "i2"}, {"f", "f2.5"}, {"s", "s\"ab\""},
        {"a", "a[i1,i2]"}, {"o", "o{s\"k\"=i1}"}, {"u", "u"}, {"n", "n"}, {"b", "bT"}, {"z", "i0"},
    };
    const std::vector<std::string> exprs = {
        "1", "-1", "2.5", "\"abc\"", "x", "missing", "b", "n", "u",
        "-x", "+x", "-f", "+f", "not x", "not not x",
        "x+y", "x-y", "x*y", "x/y", "x%y", "f+x", "x+f", "7/2", "7%2", "-7%2", "7.5%2",
        "s+\"c\"", "s~\"c\"", "s*3", "3*s", "s*0", "s*-1",
        "a+a", "1 in a", "3 in a", "3 not in a", "\"a\" in s", "\"z\" in s",
        "\"k\" in o", "\"z\" in o", "1 in o", "5 in u", "5 not in u",
        "n~s", "u+s", "n+s", "u~s", "n+1", "u+1", "n-1", "u-1",
        "1<2", "1<=1", "2>1", "2>=3", "x==5", "x==f", "5==5.0", "\"a\"==\"a\"", "b==true",
        "n==n", "u==u", "u==n", "n!=u", "a==a", "missing==n", "missing==u",
        "true and 5", "false or 7", "5 or 7", "0 or 7", "5 and 0", "u or 7",
        "\"abc\"-\"d\"", "-s", "+s", "b+1", "\"a\"<\"b\"", "a+1", "\"x\" in 5", "1 in \"abc\"",
    };
    for (const auto & ex : exprs) check_expr(ex, vars);

    printf("FG507_RUNTIME cases=%d values=%d failures=%d\n", cases, values, fails);
    return fails ? 1 : 0;
}
