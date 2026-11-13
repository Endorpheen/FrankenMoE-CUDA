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

#include "builtins-gen.h"
#include "jinja/value.h"
#include "jinja/string.h"
#include "jinja/runtime.h"

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

struct ArgSpec {
    std::vector<std::string> pos;
    std::vector<std::pair<std::string, std::string>> kw;
};

static void check_names(int kind) {
    jinja::value base;
    if (kind == 0) base = jinja::mk_val<jinja::value_int>(0); else base = jinja::mk_val<jinja::value_float>(0.0);
    std::string want;
    for (const auto & kv : base->get_builtins()) {
        if (!want.empty()) want += " ";
        want += kv.first;
    }
    std::vector<uint8_t> out(1 << 16, 0);
    ++cases;
    const int64_t n = builtin_names((int64_t) kind, slv(out));
    const std::string got((const char *) out.data(), (size_t) n);
    ++values;
    if (got != want) bad("names", "kind " + std::to_string(kind) + " got=[" + got + "] want=[" + want + "]");
}

static void check_call(int kind, const std::string & name, const ArgSpec & as) {
    jinja::context ctx("");
    jinja::func_args args(ctx);
    args.func_name = name;
    std::vector<jinja::value> pos;
    for (const auto & spec : as.pos) { VP p(spec); pos.push_back(p.parse()); }
    for (const auto & v : pos) args.push_back(v);
    for (const auto & kv : as.kw) { VP p(kv.second); args.push_back(jinja::mk_val<jinja::value_kwarg>(kv.first, p.parse())); }

    // native
    bool nok = true;
    std::string want, nerr;
    try {
        jinja::value base;
    if (kind == 0) base = jinja::mk_val<jinja::value_int>(0); else base = jinja::mk_val<jinja::value_float>(0.0);
        const auto & b = base->get_builtins();
        auto it = b.find(name);
        if (it == b.end()) throw std::runtime_error("no such builtin");
        auto res = it->second(args);
        want = jinja::value_to_json(res);
    } catch (const std::exception & e) {
        nok = false;
        nerr = e.what();
    }

    // flat encoding: all args and kwargs share one arena
    Enc e;
    std::vector<int64_t> roots;
    for (const auto & v : pos) roots.push_back(e.enc(v));
    std::vector<int64_t> kwroots;
    for (const auto & kv : as.kw) {
        VP p(kv.second);
        kwroots.push_back(e.enc(p.parse()));
    }
    const int64_t n = (int64_t) e.kind.size();
    const int64_t cap = n + 64;
    const int64_t poolcap = (int64_t) e.pool.size() + 64;
    std::vector<int64_t> A((size_t) (32 + 10 * cap + poolcap), 0);
    A[0] = cap;
    A[1] = n;
    A[2] = (int64_t) e.pool.size();
    const int64_t base = 32;
    for (int64_t i = 0; i < n; ++i) {
        A[(size_t) (base + i)] = e.kind[(size_t) i];
        A[(size_t) (base + cap + i)] = e.ival[(size_t) i];
        A[(size_t) (base + 2 * cap + i)] = e.foff[(size_t) i];
        A[(size_t) (base + 3 * cap + i)] = e.flen[(size_t) i];
        A[(size_t) (base + 4 * cap + i)] = e.cs[(size_t) i];
        A[(size_t) (base + 5 * cap + i)] = e.cc[(size_t) i];
        A[(size_t) (base + 6 * cap + i)] = e.ks[(size_t) i];
        A[(size_t) (base + 7 * cap + i)] = e.kc[(size_t) i];
        A[(size_t) (base + 8 * cap + i)] = e.vs[(size_t) i];
        A[(size_t) (base + 9 * cap + i)] = e.vc[(size_t) i];
    }
    for (size_t i = 0; i < e.pool.size(); ++i) A[(size_t) (base + 10 * cap + i)] = e.pool[i];

    std::vector<int64_t> kwoff, kwlen;
    std::string kws;
    for (const auto & kv : as.kw) {
        kwoff.push_back((int64_t) kws.size());
        kwlen.push_back((int64_t) kv.first.size());
        kws += kv.first;
    }
    std::vector<uint8_t> B((size_t) (1 << 20), 0);
    memcpy(B.data(), e.B.data(), e.B.size());
    memcpy(B.data() + e.B.size(), kws.data(), kws.size());
    std::vector<uint8_t> out(1 << 20, 0), E(1 << 16, 0);
    std::vector<double> F((size_t) cap, 0.0);
    for (int64_t i = 0; i < n; ++i) F[(size_t) i] = e.F[(size_t) i];
    std::vector<int64_t> st(40, 0);

    // the port reads kwarg names as slices of B; they live after the value bytes
    std::vector<int64_t> koff_abs = kwoff;
    for (auto & o : koff_abs) o += (int64_t) e.B.size();

    ++cases;
    const int64_t len = builtin_call((int64_t) kind, sl(name), (int64_t) pos.size(), sli(roots),
        (int64_t) as.kw.size(), sli(koff_abs), sli(kwlen), sli(kwroots),
        (int64_t) (e.B.size() + kws.size()), sli(A), slf(F), slv(B), slv(E), slv(out), sli(st));
    const std::string ctxs = "kind " + std::to_string(kind) + " " + name + "(" + std::to_string(pos.size()) + " args," + std::to_string(as.kw.size()) + " kw)";

    if (!nok) {
        ++values;
        if (len >= 0 || st[1] == 0) { bad("expected rejection", ctxs); return; }
        ++values;
        const std::string gerr((const char *) E.data(), (size_t) st[2]);
        if (gerr != nerr) bad("error text", ctxs + "\n  got =[" + visible(gerr) + "]\n  want=[" + visible(nerr) + "]");
        return;
    }
    ++values;
    if (len < 0) {
        const std::string gerr((const char *) E.data(), (size_t) st[2]);
        bad("unexpected rejection", ctxs + ": " + visible(gerr));
        return;
    }
    ++values;
    const std::string got((const char *) out.data(), (size_t) len);
    if (got != want) bad("result", ctxs + "\n  got =[" + visible(got) + "]\n  want=[" + visible(want) + "]");
}

int main(void) {
    goose_init(0, 0);

    check_names(0);
    check_names(1);

    const std::vector<std::pair<std::string, ArgSpec>> ints = {
        {"default", {{"i5","i9"}, {}}},
        {"default", {{"u","i9"}, {}}},
        {"default", {{"n","i9"}, {}}},
        {"default", {{"i0","i9","bT"}, {}}},
        {"default", {{"i5","i9","bT"}, {}}},
        {"default", {{"i5","i9","bF"}, {}}},
        {"default", {{"i5"}, {}}},
        {"abs", {{"i5"}, {}}},
        {"abs", {{"i-5"}, {}}},
        {"abs", {{"i0"}, {}}},
        {"abs", {{"i5","i7"}, {}}},
        {"abs", {{}, {}}},
        {"abs", {{"s\"x\""}, {}}},
        {"int", {{"i7"}, {}}},
        {"int", {{"i-7"}, {}}},
        {"int", {{"s\"x\""}, {}}},
        {"float", {{"i7"}, {}}},
        {"float", {{"i-7"}, {}}},
        {"safe", {{"i42"}, {}}},
        {"string", {{"i42"}, {}}},
        {"tojson", {{"i42"}, {}}},
        {"tojson", {{"i42"}, {{"indent","i0"}}}},
        {"tojson", {{"i42"}, {{"indent","i2"}}}},
        {"tojson", {{"i42"}, {{"sort_keys","bT"}}}},
        {"tojson", {{"a[i1,i2]"}, {}}},
        {"tojson", {{"a[i1,i2]"}, {{"indent","i0"}}}},
        {"tojson", {{"a[i1,i2]"}, {{"indent","i2"}}}},
        {"tojson", {{"a[i1,i2]"}, {{"separators","a[s\",\",s\":\"]"}}}},
        {"tojson", {{"o{s\"a\"=i1,s\"b\"=i2}"}, {{"indent","i2"}}}},
        {"tojson", {{}, {}}},
    };
    for (const auto & c : ints) check_call(0, c.first, c.second);

    const std::vector<std::pair<std::string, ArgSpec>> floats = {
        {"default", {{"f1.5","i9"}, {}}},
        {"default", {{"u","i9"}, {}}},
        {"abs", {{"f-2.5"}, {}}},
        {"abs", {{"f0.0"}, {}}},
        {"abs", {{"f-0.0"}, {}}},
        {"abs", {{"s\"x\""}, {}}},
        {"int", {{"f2.9"}, {}}},
        {"int", {{"f-2.9"}, {}}},
        {"float", {{"f2.9"}, {}}},
        {"safe", {{"f1.0"}, {}}},
        {"string", {{"f1e16"}, {}}},
        {"string", {{"f0.1"}, {}}},
        {"string", {{"f-0.0"}, {}}},
        {"string", {{"fNaN"}, {}}},
        {"string", {{"fInf"}, {}}},
        {"tojson", {{"f1.5"}, {}}},
        {"tojson", {{"f1.5"}, {{"indent","i0"}}}},
        {"tojson", {{"f1e16"}, {{"indent","i2"}}}},
        {"tojson", {{"f1.5"}, {{"sort_keys","bT"}}}},
    };
    for (const auto & c : floats) check_call(1, c.first, c.second);

    printf("FG506_BUILTINS cases=%d values=%d failures=%d\n", cases, values, fails);
    return fails ? 1 : 0;
}
