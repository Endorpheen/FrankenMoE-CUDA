// Differential fixture for the Jinja value model (EXP504).
// Oracle: common/jinja/value.h and value.cpp: conversions, flags,
// value_compare, operator==/!= and value_to_string_repr.
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <cmath>
#include <string>
#include <vector>
#include <map>

#include "values-gen.h"
#include "jinja/value.h"
#include "jinja/string.h"

extern "C" int64_t gs_json_fmt_g(sl_u8 out, double x) {
    return snprintf((char *) out.data, (size_t) out.len, "%.17g", x);
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

static std::string esc(const std::string & s) {
    std::string r = "\"";
    for (unsigned char c : s) {
        if (c == '"') r += "\\\"";
        else if (c == '\\') r += "\\\\";
        else if (c == '\n') r += "\\n";
        else if (c == '\r') r += "\\r";
        else if (c == '\t') r += "\\t";
        else if (c < 32 || c >= 127) {
            char b[8]; snprintf(b, sizeof b, "\\x%02x", c); r += b;
        } else r.push_back((char) c);
    }
    return r + "\"";
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
        if (c == 's') { expect('"'); return jinja::mk_val<jinja::value_string>(raw_until('"')); }
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
            while (peek() != ']' && peek() != '\0') {
                vals.push_back(parse());
                expect(',');
            }
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

// ---- native info text ------------------------------------------------------
static std::string native_info(const jinja::value & v) {
    std::string o;
    o += "type=" + esc(v->type()) + "\n";
    o += std::string("numeric=") + (v->is_numeric() ? "1" : "0") + "\n";
    o += std::string("hashable=") + (v->is_hashable() ? "1" : "0") + "\n";
    o += std::string("immutable=") + (v->is_immutable() ? "1" : "0") + "\n";
    o += std::string("none=") + (v->is_none() ? "1" : "0") + "\n";
    o += std::string("undefined=") + (v->is_undefined() ? "1" : "0") + "\n";
    try { o += std::string("BOOL=") + (v->as_bool() ? "1" : "0"); }
    catch (const std::exception & e) { o += "BOOL=ERR:" + esc(e.what()); }
    o += "\n";

    o += "INT=";
    try { o += std::to_string(v->as_int()); }
    catch (const std::exception & e) { o += "ERR:" + esc(e.what()); }
    o += "\n";

    o += "FLOAT=";
    try { char b[64]; snprintf(b, sizeof b, "%.17g", v->as_float()); o += b; }
    catch (const std::exception & e) { o += "ERR:" + esc(e.what()); }
    o += "\n";

    o += "STRING=";
    try { o += esc(v->as_string().str()); }
    catch (const std::exception & e) { o += "ERR:" + esc(e.what()); }
    o += "\n";

    o += "REPR=";
    try { o += esc(jinja::value_to_string_repr(v)); }
    catch (const std::exception & e) { o += "ERR:" + esc(e.what()); }
    o += "\n";
    return o;
}

static std::string native_cmp(const jinja::value & a, const jinja::value & b) {
    std::string o;
    const jinja::value_compare_op ops[5] = {
        jinja::value_compare_op::eq, jinja::value_compare_op::ge, jinja::value_compare_op::gt,
        jinja::value_compare_op::lt, jinja::value_compare_op::ne };
    const char * names[5] = { "EQ", "GE", "GT", "LT", "NE" };
    for (int k = 0; k < 5; ++k) {
        if (k) o += " ";
        o += names[k];
        o += "=";
        try { o += jinja::value_compare(a, b, ops[k]) ? "1" : "0"; }
        catch (const std::exception & e) { o += "ERR:" + esc(e.what()); }
    }
    o += "\n";
    o += std::string("SEQ=") + ((*a == *b) ? "1" : "0") + "\n";
    o += std::string("SNE=") + ((*a != *b) ? "1" : "0") + "\n";
    return o;
}

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
            // the hint is only reachable through type(); take it from the text
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

static void check_info(const std::string & spec) {
    VP p(spec);
    const auto v = p.parse();
    Enc e;
    const int64_t root = e.enc(v);
    const int64_t n = (int64_t) e.kind.size();

    std::vector<int64_t> A((size_t) (32 + 10 * n + e.pool.size()), 0);
    A[0] = n;
    const int64_t b = 32;
    for (int64_t i = 0; i < n; ++i) {
        A[(size_t) (b + i)] = e.kind[(size_t) i];
        A[(size_t) (b + n + i)] = e.ival[(size_t) i];
        A[(size_t) (b + 2 * n + i)] = e.foff[(size_t) i];
        A[(size_t) (b + 3 * n + i)] = e.flen[(size_t) i];
        A[(size_t) (b + 4 * n + i)] = e.cs[(size_t) i];
        A[(size_t) (b + 5 * n + i)] = e.cc[(size_t) i];
        A[(size_t) (b + 6 * n + i)] = e.ks[(size_t) i];
        A[(size_t) (b + 7 * n + i)] = e.kc[(size_t) i];
        A[(size_t) (b + 8 * n + i)] = e.vs[(size_t) i];
        A[(size_t) (b + 9 * n + i)] = e.vc[(size_t) i];
    }
    for (size_t i = 0; i < e.pool.size(); ++i) A[(size_t) (b + 10 * n + i)] = e.pool[i];

    std::vector<uint8_t> out(1 << 20, 0), E(1 << 16, 0);
    std::vector<int64_t> st(40, 0);
    ++cases;
    const int64_t len = value_info(root, sli(A), slf(e.F), slv(e.B), slv(E), slv(out), sli(st));
    const std::string got((const char *) out.data(), (size_t) len);
    const std::string want = native_info(v);
    ++values;
    if (got != want) bad("info", "spec=[" + spec + "]\n--- native ---\n" + esc(want) + "\n--- goose ---\n" + esc(got));
}

static void check_cmp(const std::string & sa, const std::string & sb) {
    VP pa(sa), pb(sb);
    const auto a = pa.parse();
    const auto b = pb.parse();
    Enc e;
    const int64_t ra = e.enc(a);
    const int64_t rb = e.enc(b);
    const int64_t n = (int64_t) e.kind.size();

    std::vector<int64_t> A((size_t) (32 + 10 * n + e.pool.size()), 0);
    A[0] = n;
    const int64_t base = 32;
    for (int64_t i = 0; i < n; ++i) {
        A[(size_t) (base + i)] = e.kind[(size_t) i];
        A[(size_t) (base + n + i)] = e.ival[(size_t) i];
        A[(size_t) (base + 2 * n + i)] = e.foff[(size_t) i];
        A[(size_t) (base + 3 * n + i)] = e.flen[(size_t) i];
        A[(size_t) (base + 4 * n + i)] = e.cs[(size_t) i];
        A[(size_t) (base + 5 * n + i)] = e.cc[(size_t) i];
        A[(size_t) (base + 6 * n + i)] = e.ks[(size_t) i];
        A[(size_t) (base + 7 * n + i)] = e.kc[(size_t) i];
        A[(size_t) (base + 8 * n + i)] = e.vs[(size_t) i];
        A[(size_t) (base + 9 * n + i)] = e.vc[(size_t) i];
    }
    for (size_t i = 0; i < e.pool.size(); ++i) A[(size_t) (base + 10 * n + i)] = e.pool[i];

    std::vector<uint8_t> out(1 << 20, 0), E(1 << 16, 0);
    std::vector<int64_t> st(40, 0);
    ++cases;
    const int64_t len = value_cmp(ra, rb, sli(A), slf(e.F), slv(e.B), slv(E), slv(out), sli(st));
    const std::string got((const char *) out.data(), (size_t) len);
    const std::string want = native_cmp(a, b);
    ++values;
    if (got != want) bad("cmp", "a=[" + sa + "] b=[" + sb + "]\n--- native ---\n" + esc(want) + "\n--- goose ---\n" + esc(got));
}

int main(void) {
    goose_init(0, 0);

    const std::vector<std::string> specs = {
        "i0", "i1", "i-1", "i42", "i9223372036854775807", "i-9223372036854775808",
        "f0.0", "f-0.0", "f1.0", "f1.5", "f-2.5", "f0.1", "f1e-5", "f1e-4", "f1e15", "f1e16",
        "f1e18", "f3.141592653589793", "f2.2250738585072014e-308", "f5e-324", "f123456789.123",
        "s\"\"", "s\"abc\"", "s\"a'b\"", "s\"a\\\"b\"", "s\"line\\nbreak\"", "s\"True\"", "s\"1\"",
        "bT", "bF", "n",
        "u", "u\"spam\"",
        "a[]", "a[i1]", "a[i1,i2]", "a[i1,f1.5,s\"x\"]", "a[a[i1],a[i2]]",
        "t[]", "t[i1]", "t[i1,i2]",
        "o{}", "o{s\"a\"=i1}", "o{s\"a\"=i1,s\"b\"=s\"x\"}", "o{i1=s\"x\",f1.5=bT}",
        "o{s\"n\"=a[i1,o{s\"k\"=n}]}",
    };
    for (const auto & s : specs) check_info(s);

    const std::vector<std::string> pair_specs = {
        "i1", "i2", "i-1", "f1.0", "f1.5", "s\"1\"", "s\"abc\"", "s\"\"", "bT", "bF", "n",
        "a[i1]", "a[i1,i2]", "t[i1]", "o{s\"a\"=i1}",
    };
    for (const auto & a : pair_specs) {
        for (const auto & b : pair_specs) check_cmp(a, b);
    }

    printf("FG504_VALUES cases=%d values=%d failures=%d\n", cases, values, fails);
    return fails ? 1 : 0;
}
