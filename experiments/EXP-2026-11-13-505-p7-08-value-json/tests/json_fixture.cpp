// Differential fixture for Jinja JSON value output (EXP505).
// Oracle: jinja::value_to_json from common/jinja/value.cpp, including
// float_repr and float_json.
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <cmath>
#include <string>
#include <vector>

#include "valuesjson-gen.h"
#include "jinja/value.h"
#include "jinja/string.h"

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

static void check_json(const std::string & spec) {
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

    for (int indent : { -1, 0, 2 }) {
        bool nok = true;
        std::string want, nerr;
        try { want = jinja::value_to_json(v, indent); }
        catch (const std::exception & ex) { nok = false; nerr = ex.what(); }

        std::vector<uint8_t> out(1 << 22, 0), E(1 << 16, 0);
        std::vector<int64_t> st(40, 0);
        ++cases;
        const int64_t len = value_json(root, (int64_t) indent, sli(A), slf(e.F), slv(e.B), slv(E), slv(out), sli(st));
        const std::string ctx = "spec=[" + spec + "] indent=" + std::to_string(indent);

        if (!nok) {
            ++values;
            if (len >= 0 || st[1] == 0) { bad("expected rejection", ctx); continue; }
            ++values;
            const std::string gerr((const char *) E.data(), (size_t) st[2]);
            if (gerr != nerr) bad("error text", ctx + "\n  got =[" + visible(gerr) + "]\n  want=[" + visible(nerr) + "]");
            continue;
        }
        ++values;
        if (len < 0) {
            const std::string gerr((const char *) E.data(), (size_t) st[2]);
            bad("unexpected rejection", ctx + ": " + visible(gerr));
            continue;
        }
        ++values;
        const std::string got((const char *) out.data(), (size_t) len);
        if (got != want) bad("json", ctx + "\n  got =[" + visible(got) + "]\n  want=[" + visible(want) + "]");
    }
}

int main(void) {
    goose_init(0, 0);

    const std::vector<std::string> specs = {
        "i0", "i1", "i-1", "i42", "i9223372036854775807", "i-9223372036854775808",
        "f0.0", "f-0.0", "f1.0", "f1.5", "f-2.5", "f0.1", "f1e-7", "f1e-5", "f1e-4",
        "f1e15", "f1e16", "f1e18", "f3.141592653589793", "f2.2250738585072014e-308",
        "f5e-324", "f123456789.123", "f1234567890123456.0", "f12345678901234567.0",
        "fInf", "f-Inf", "fNaN",
        "s\"\"", "s\"abc\"", "s\"a'b\"", "s\"a\\\"b\"", "s\"back\\\\slash\"",
        "s\"tab\\there\"", "s\"nl\\nhere\"", "s\"cr\\rhere\"", "s\"ctrl\\x01here\"", "s\"ctrl\\x1fhere\"", "s\"vt\\x0bhere\"",
        "bT", "bF", "n", "u", "u\"spam\"",
        "a[]", "a[i1]", "a[i1,i2]", "a[i1,f1.5,s\"x\"]", "a[a[i1],a[i2]]",
        "t[]", "t[i1]", "t[i1,i2]",
        "o{}", "o{s\"a\"=i1}", "o{s\"a\"=i1,s\"b\"=s\"x\"}", "o{i1=s\"x\",f1.5=bT}",
        "o{s\"n\"=a[i1,o{s\"k\"=n}]}", "o{bT=i1}", "o{f1e16=i1}", "o{f0.1=i1}",
    };
    for (const auto & s : specs) check_json(s);

    { // 500 integers
        std::string s = "a[";
        for (int i = 0; i < 500; ++i) { if (i) s += ","; s += "i" + std::to_string(i); }
        s += "]";
        check_json(s);
    }
    { // 200 keys
        std::string s = "o{";
        for (int i = 0; i < 200; ++i) { if (i) s += ","; s += "s\"k" + std::to_string(i) + "\"=i" + std::to_string(i); }
        s += "}";
        check_json(s);
    }
    { // a 5000-byte string
        check_json("s\"" + std::string(5000, 'a') + "\"");
    }
    { // 50 nested arrays
        std::string s = "i1";
        for (int i = 0; i < 50; ++i) s = "a[" + s + "]";
        check_json(s);
    }
    { // 30 nested objects
        std::string s = "i1";
        for (int i = 0; i < 30; ++i) s = "o{s\"k" + std::to_string(i) + "\"=" + s + "}";
        check_json(s);
    }

    printf("FG505_VALUEJSON cases=%d values=%d failures=%d\n", cases, values, fails);
    return fails ? 1 : 0;
}
