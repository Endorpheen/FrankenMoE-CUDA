// Differential fixture for the JSON text parser (EXP493).
// Oracle: common_json::parse (nlohmann ordered_json) from common/json.cpp.
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <string>
#include <vector>
#include <map>
#include <functional>

#include "jsonparse-gen.h"
#include "json.h"

// the one C primitive the parser borrows: strtod over a NUL-terminated copy
extern "C" double gs_json_strtod(sl_u8 s) {
    std::string tmp((const char *) s.data, (size_t) s.len);
    return std::strtod(tmp.c_str(), nullptr);
}

static int cases = 0, values = 0, fails = 0;
static void bad(const char * what, const std::string & detail) {
    ++fails;
    if (fails <= 12) printf("FAIL %s: %s\n", what, detail.c_str());
}
static sl_u8 slv(std::vector<uint8_t> & v) { return sl_u8{ v.data(), (int64_t) v.size() }; }
static sl_i64 sli(std::vector<int64_t> & v) { return sl_i64{ v.data(), (int64_t) v.size() }; }
static sl_f64 slf(std::vector<double> & v) { return sl_f64{ v.data(), (int64_t) v.size() }; }

struct Interner {
    std::vector<std::string> strs;
    std::map<std::string, int> idx;
    int sid(const std::string & s) {
        auto it = idx.find(s);
        if (it != idx.end()) return it->second;
        int i = (int) strs.size();
        strs.push_back(s);
        idx[s] = i;
        return i;
    }
};

// native tree -> the flat layout the Goose parser emits
struct NativeFlat {
    Interner & in;
    std::vector<int64_t> kind, val, f, n, key, child, so, sl;
    std::vector<double> dbl;
    explicit NativeFlat(Interner & i) : in(i) {}
    int enc(const common_json & v) {
        const int me = (int) kind.size();
        kind.push_back(-1); val.push_back(0); f.push_back(0); n.push_back(0);
        so.push_back(0); sl.push_back(0);
        dbl.push_back(0.0);
        if (v.is_null()) {
            kind[me] = 0;
        } else if (v.is_boolean()) {
            kind[me] = 1; val[me] = v.get<bool>() ? 1 : 0;
        } else if (v.is_number_float()) {
            kind[me] = 4; dbl[me] = v.get<double>();
        } else if (v.is_number_integer()) {
            const std::string d = v.dump();
            if (!d.empty() && d[0] == '-') {
                kind[me] = 2; val[me] = (int64_t) v.get<long long>();
            } else {
                const unsigned long long u = v.get<unsigned long long>();
                if (u <= (unsigned long long) INT64_MAX) { kind[me] = 2; val[me] = (int64_t) u; }
                else { kind[me] = 3; val[me] = (int64_t) u; }
            }
        } else if (v.is_string()) {
            kind[me] = 5; val[me] = in.sid(v.get<std::string>());
        } else if (v.is_array()) {
            kind[me] = 6;
            std::vector<int> tmp;
            for (const auto & e : v) tmp.push_back(enc(e));
            f[me] = (int64_t) child.size();
            for (int ci : tmp) { key.push_back(-1); child.push_back(ci); }
            n[me] = (int64_t) tmp.size();
        } else {
            kind[me] = 7;
            std::vector<std::pair<int, int>> tmp;
            for (const auto & [k, e] : v.items()) {
                const int ci = enc(e);
                tmp.emplace_back(in.sid(k), ci);
            }
            f[me] = (int64_t) child.size();
            for (auto & [ks, ci] : tmp) { key.push_back(ks); child.push_back(ci); }
            n[me] = (int64_t) tmp.size();
        }
        return me;
    }
};

static bool extract(const std::string & msg, int64_t & code, int64_t & line, int64_t & col) {
    code = -1; line = -1; col = -1;
    const std::string key = "json.exception.";
    auto p = msg.find(key);
    if (p == std::string::npos) return false;
    auto dot = msg.find('.', p + key.size());
    if (dot == std::string::npos) return false;
    code = std::atoll(msg.c_str() + dot + 1);
    auto lp = msg.find("line ");
    auto cp = msg.find("column ");
    if (lp != std::string::npos) line = std::atoll(msg.c_str() + lp + 5);
    if (cp != std::string::npos) col = std::atoll(msg.c_str() + cp + 7);
    return true;
}

static void check(const std::string & text, const std::string & label) {
    Interner in;
    bool nok = true;
    int64_t ncode = -1, nline = -1, ncol = -1;
    NativeFlat nf(in);
    int nroot = -1;
    try {
        common_json v = common_json::parse(text);
        nroot = nf.enc(v);
    } catch (const std::exception & e) {
        nok = false;
        if (!extract(e.what(), ncode, nline, ncol)) {
            bad("native error", label + ": " + e.what());
            return;
        }
    }

    const int maxn = 256 * 1024, maxe = 256 * 1024, maxk = 256 * 1024;
    std::vector<int64_t> jk(maxn, -1), jv(maxn, 0), js(maxn, 0), jl(maxn, 0), jf(maxn, 0), jn(maxn, 0),
        jh(maxn, -1), e_val(maxe, 0), e_koff(maxe, 0), e_klen(maxe, 0), e_next(maxe, 0),
        jkey(maxk, 0), jkl(maxk, 0), jchild(maxk, 0), st(16, 0);
    std::vector<double> jd(maxn, 0.0);
    std::vector<uint8_t> sbuf(1 << 22, 0);
    ++cases;
    const int64_t groot = json_parse(sl_u8{ (uint8_t *) text.data(), (int64_t) text.size() },
        sli(jk), sli(jv), slf(jd), sli(js), sli(jl), sli(jf), sli(jn), sli(jh),
        sli(e_val), sli(e_koff), sli(e_klen), sli(e_next), slv(sbuf), sli(jkey), sli(jkl), sli(jchild), sli(st));

    if (!nok) {
        ++values;
        if (st[1] == 0) { bad("expected rejection", label + " (native code " + std::to_string(ncode) + ")"); return; }
        ++values;
        if (st[1] != ncode) { bad("error class", label + " goose=" + std::to_string(st[1]) + " native=" + std::to_string(ncode)); return; }
        if (ncode == 101) {
            ++values;
            if (st[2] != nline || st[3] != ncol) {
                bad("error position", label + " goose=(" + std::to_string(st[2]) + "," + std::to_string(st[3]) +
                                       ") native=(" + std::to_string(nline) + "," + std::to_string(ncol) + ")");
            }
        }
        return;
    }
    ++values;
    if (groot < 0) { bad("unexpected rejection", label + " goose code " + std::to_string(st[1])); return; }

    // native node count is the flat size; the port's ids are creation order, so
    // compare structurally through a DFS renumbering of the port's tree
    std::map<int64_t, int64_t> m;
    std::vector<int64_t> order;
    std::function<void(int64_t)> dfs = [&](int64_t id) {
        if (id < 0 || m.count(id)) return;
        m[id] = (int64_t) order.size();
        order.push_back(id);
        for (int64_t j = 0; j < jn[id]; ++j) dfs(jchild[jf[id] + j]);
    };
    dfs(groot);
    ++values;
    if (order.size() != nf.kind.size()) {
        bad("node count", label + " goose=" + std::to_string(order.size()) + " native=" + std::to_string(nf.kind.size()));
        return;
    }
    for (size_t ni = 0; ni < order.size(); ++ni) {
        const int64_t o = order[ni];
        const size_t i = ni;
        const std::string ctx = label + " node " + std::to_string(i);
        ++values;
        if (jk[o] != nf.kind[i]) { bad("kind", ctx + " goose=" + std::to_string(jk[o]) + " native=" + std::to_string(nf.kind[i])); continue; }
        if (jk[o] == 1 || jk[o] == 2 || jk[o] == 3) {
            ++values;
            if (jv[o] != nf.val[i]) {
                bad("int value", ctx + " goose=" + std::to_string(jv[o]) + " native=" + std::to_string(nf.val[i]));
            }
            continue;
        }
        if (jk[o] == 4) {
            ++values;
            if (std::memcmp(&jd[o], &nf.dbl[i], sizeof(double)) != 0) {
                char a[64], b[64];
                snprintf(a, sizeof a, "%.17g", jd[o]);
                snprintf(b, sizeof b, "%.17g", nf.dbl[i]);
                bad("double value", ctx + " goose=" + a + " native=" + b);
            }
            continue;
        }
        if (jk[o] == 5) {
            ++values;
            const std::string got((const char *) sbuf.data() + js[o], (size_t) jl[o]);
            const std::string want = in.strs[nf.val[i]];
            if (got != want) bad("string", ctx + " got=[" + got + "] want=[" + want + "]");
            continue;
        }
        ++values;
        if (jn[o] != nf.n[i]) { bad("child count", ctx + " goose=" + std::to_string(jn[o]) + " native=" + std::to_string(nf.n[i])); continue; }
        for (int64_t k = 0; k < jn[o]; ++k) {
            ++values;
            const int64_t gc = jchild[jf[o] + k];
            const int64_t nc = nf.child[nf.f[i] + k];
            if (m[gc] != nc) bad("child order", ctx + " index " + std::to_string(k));
            if (jk[o] == 7) {
                ++values;
                const std::string gk((const char *) sbuf.data() + jkey[jf[o] + k], (size_t) jkl[jf[o] + k]);
                const std::string nk = in.strs[nf.key[nf.f[i] + k]];
                if (gk != nk) bad("key", ctx + " index " + std::to_string(k) + " got=[" + gk + "] want=[" + nk + "]");
            }
        }
    }
}

int main(void) {
    goose_init(0, 0);

    const std::vector<std::pair<std::string, std::string>> ok = {
        {"null", "null"}, {"true", "true"}, {"false", "false"},
        {"0", "zero"}, {"-0", "minus zero"}, {"1", "one"}, {"-1", "minus one"},
        {"42", "42"}, {"9223372036854775807", "int64 max"}, {"9223372036854775808", "uint 2^63"},
        {"18446744073709551615", "uint64 max"}, {"-9223372036854775808", "int64 min"},
        {"18446744073709551616", "above uint64 -> float"}, {"-9223372036854775809", "below int64 -> float"},
        {"1.0", "float"}, {"0.1", "tenth"}, {"1e10", "exponent"}, {"1.5e-10", "small"}, {"1e-400", "underflow"},
        {"1e+5", "plus exponent"}, {"-1.25e+3", "signed exponent"},
        {"\"\"", "empty string"}, {"\"abc\"", "plain string"},
        {"\"a\\\"b\"", "quote"}, {"\"a\\\\b\"", "backslash"}, {"\"a\\/b\"", "slash"},
        {"\"\\b\\f\\n\\r\\t\"", "controls"}, {"\"\\u0000\"", "nul"},
        {"\"\\u00e9\"", "latin"}, {"\"caf\\u00e9\"", "accent"},
        {"\"\\ud83d\\ude00\"", "surrogate pair"},
        {"{\"a\":1}", "object"}, {"[1,2,3]", "array"}, {"{\"a\":[1,{\"b\":null}]}", "nested"},
        {"{\"b\":1,\"a\":2}", "key order"}, {"{\"a\":1,\"a\":2}", "duplicate key"},
        {"  [ 1 , 2 ]  ", "whitespace"}, {"\"\\u00e9\\u00e8\"", "two escapes"},
        {"[[[[[[[[[[1]]]]]]]]]]", "nested 10"},
    };
    for (const auto & [text, label] : ok) check(text, label);

    const std::vector<std::pair<std::string, std::string>> bad_inputs = {
        {"", "empty"}, {" ", "blank"}, {"{", "open object"}, {"[", "open array"},
        {"{\"a\"}", "missing colon"}, {"{\"a\":}", "missing value"}, {"[,]", "comma first"},
        {"[1,]", "trailing comma"}, {"[1 2]", "missing comma"}, {"{\"a\":1 \"b\":2}", "object missing comma"},
        {"tru", "short true"}, {"nul", "short null"}, {"fals", "short false"}, {"+1", "plus number"},
        {"01", "leading zero"}, {"1.", "no fraction"}, {"1.e5", "dot then exponent"},
        {"1e", "no exponent"}, {"1e+", "no exponent digit"}, {"-", "bare minus"},
        {"\"a", "unterminated"}, {"\"a\\q\"", "bad escape"}, {"\"\\u12\"", "short unicode"},
        {"\"\\ud83d\"", "lone high"}, {"\"\\udc00\"", "lone low"}, {"\"\\ud83dx\"", "high then bad"},
        {"{\"a\":1}x", "trailing garbage"}, {"[] []", "two values"},
        {"\"a\nb\"", "raw newline in string"}, {"\t\n", "only whitespace"},
        {"\v1", "vertical tab"},
        {"{\"a\" 1}", "key without colon"}, {"[1,,2]", "double comma"},
        {"1e400", "number overflow"}, {"-1e400", "negative overflow"},
    };
    for (const auto & [text, label] : bad_inputs) check(text, label);

    // deep and large documents, built here
    {
        std::string deep;
        for (int i = 0; i < 500; ++i) deep += "[";
        deep += "1";
        for (int i = 0; i < 500; ++i) deep += "]";
        check(deep, "deep 500 arrays");
    }
    {
        std::string large = "[";
        for (int i = 0; i < 20000; ++i) { if (i) large += ","; large += std::to_string(i); }
        large += "]";
        check(large, "large array 20000");
    }
    {
        std::string obj = "{";
        for (int i = 0; i < 5000; ++i) { if (i) obj += ","; obj += "\"k" + std::to_string(i) + "\":" + std::to_string(i); }
        obj += "}";
        check(obj, "large object 5000");
    }
    {
        std::string s = "\"";
        for (int i = 0; i < 20000; ++i) s += "ab";
        s += "\"";
        check(s, "long string 40000");
    }

    printf("FG493_JSON cases=%d values=%d failures=%d\n", cases, values, fails);
    return fails ? 1 : 0;
}
