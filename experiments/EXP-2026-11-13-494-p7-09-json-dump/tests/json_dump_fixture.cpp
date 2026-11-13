// Differential fixture for JSON text rendering (EXP494).
// Oracle: common_json::dump(indent) from common/json.cpp (nlohmann serializer).
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <string>
#include <vector>
#include <map>
#include <functional>

#include "jsondump-gen.h"
#include "json.h"

extern "C" double gs_json_strtod(sl_u8 s) {
    std::string tmp((const char *) s.data, (size_t) s.len);
    return std::strtod(tmp.c_str(), nullptr);
}
extern "C" int64_t gs_json_fmt_e(sl_u8 out, int64_t prec, double x) {
    const int n = snprintf((char *) out.data, (size_t) out.len, "%.*e", (int) prec, x);
    return n;
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

struct Flat {
    Interner & in;
    std::vector<int64_t> kind, val, f, n, key, klen, child, so, sl;
    std::vector<double> dbl;
    std::vector<std::string> store;
    explicit Flat(Interner & i) : in(i) {}
    int enc(const common_json & v) {
        const int me = (int) kind.size();
        kind.push_back(-1); val.push_back(0); f.push_back(0); n.push_back(0);
        so.push_back(0); sl.push_back(0); dbl.push_back(0.0);
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
            kind[me] = 5;
            val[me] = in.sid(v.get<std::string>());
        } else if (v.is_array()) {
            kind[me] = 6;
            std::vector<int> tmp;
            for (const auto & e : v) tmp.push_back(enc(e));
            f[me] = (int64_t) child.size();
            for (int ci : tmp) { key.push_back(-1); klen.push_back(0); child.push_back(ci); }
            n[me] = (int64_t) tmp.size();
        } else {
            kind[me] = 7;
            std::vector<std::pair<int, std::pair<int, int>>> tmp;
            for (const auto & [k, e] : v.items()) {
                const int ci = enc(e);
                const int ki = in.sid(k);
                tmp.push_back({ki, {ci, (int) in.strs[ki].size()}});
            }
            f[me] = (int64_t) child.size();
            for (auto & e2 : tmp) {
                key.push_back(e2.first);
                klen.push_back(e2.second.second);
                child.push_back(e2.second.first);
            }
            n[me] = (int64_t) tmp.size();
        }
        return me;
    }
};

static void run_dump(const common_json & v, const std::string & label, bool safe) {
    Interner in;
    Flat fl(in);
    const int root = fl.enc(v);

    // one string arena; ids are stored per node
    std::string arena;
    std::vector<int64_t> node_so(fl.kind.size(), 0);
    for (size_t i = 0; i < fl.kind.size(); ++i) {
        if (fl.kind[i] == 5) {
            node_so[i] = (int64_t) arena.size();
            arena += in.strs[fl.val[i]];
        }
    }
    std::vector<int64_t> key_off(fl.key.size(), 0), key_len(fl.key.size(), 0);
    for (size_t i = 0; i < fl.key.size(); ++i) {
        if (fl.kind.size() && fl.key[i] >= 0) {
            key_off[i] = (int64_t) arena.size();
            key_len[i] = (int64_t) in.strs[fl.key[i]].size();
            arena += in.strs[fl.key[i]];
        }
    }
    std::vector<int64_t> js(fl.kind.size(), 0), jl(fl.kind.size(), 0);
    for (size_t i = 0; i < fl.kind.size(); ++i) {
        if (fl.kind[i] == 5) { js[i] = node_so[i]; jl[i] = (int64_t) in.strs[fl.val[i]].size(); }
    }

    const std::vector<int> indents = { -1, 0, 2, 4 };
    for (int ind : indents) {
        std::string want;
        try {
            want = safe ? v.dump_safe(ind) : v.dump(ind);
        } catch (const std::exception & e) {
            bad("native dump", label + ": " + e.what());
            return;
        }
        std::vector<uint8_t> out(1 << 22, 0);
        std::vector<uint8_t> sbuf(arena.begin(), arena.end());
        std::vector<double> jd = fl.dbl;
        ++cases;
        const int64_t len = json_dump(root, sli(fl.kind), sli(fl.val), slf(jd), sli(js), sli(jl),
            sli(fl.f), sli(fl.n), sli(key_off), sli(key_len), sli(fl.child), slv(sbuf), slv(out), (int64_t) ind, safe ? 1 : 0);
        const std::string got((const char *) out.data(), (size_t) len);
        ++values;
        if (got != want) {
            bad(safe ? "dump_safe" : "dump", label + " indent=" + std::to_string(ind) +
                "\n  got =[" + got + "]\n  want=[" + want + "]");
        }
    }
}

static void check(const std::string & text, const std::string & label) {
    common_json v;
    try {
        v = common_json::parse(text);
    } catch (const std::exception & e) {
        bad("parse", label + ": " + e.what());
        return;
    }
    run_dump(v, label, false);
}

static void check_safe(const std::string & bytes, bool as_key, const std::string & label) {
    common_json v;
    if (as_key) {
        v = common_json::object();
        v[bytes] = common_json(1);
    } else {
        v = common_json(bytes.c_str());
    }
    run_dump(v, label, true);
}

int main(void) {
    goose_init(0, 0);

    const std::vector<std::pair<std::string, std::string>> docs = {
        {"null", "null"}, {"true", "true"}, {"0", "zero"}, {"-1", "minus"},
        {"9223372036854775807", "int64 max"}, {"18446744073709551615", "uint64 max"},
        {"-9223372036854775808", "int64 min"},
        {"1.0", "one float"}, {"-0.0", "minus zero"}, {"0.0", "zero float"},
        {"0.1", "tenth"}, {"1e10", "1e10"}, {"1e15", "1e15"}, {"1e16", "1e16"},
        {"1e-4", "1e-4"}, {"1e-5", "1e-5"}, {"1.5e-10", "small"},
        {"5e-324", "min subnormal"}, {"1.7976931348623157e308", "max"},
        {"3.141592653589793", "pi"}, {"2.2250738585072014e-308", "min normal"},
        {"123456789012345678901234567890.0", "big float"},
        {"\"\"", "empty string"}, {"\"abc\"", "plain"},
        {"\"a\\\"b\"", "quote"}, {"\"a\\\\b\"", "backslash"},
        {"\"\\b\\t\\n\\f\\r\"", "short escapes"},
        {"\"\\u0000\\u001f\"", "control escapes"}, {"\"\\u007f\"", "del"},
        {"\"caf\\u00e9\"", "unicode"}, {"\"\\ud83d\\ude00\"", "emoji"},
        {"{}", "empty object"}, {"[]", "empty array"},
        {"{\"a\":1}", "one object"}, {"[1,2,3]", "array"},
        {"{\"a\":[1,{\"b\":null}],\"c\":{}}", "nested"},
        {"{\"b\":1,\"a\":2}", "key order"},
        {"{\"a\":{\"b\":{\"c\":[1,2,3]}}}", "deep object"},
    };
    for (const auto & [text, label] : docs) check(text, label);

    // deep and large
    {
        std::string deep;
        for (int i = 0; i < 500; ++i) deep += "[";
        deep += "1";
        for (int i = 0; i < 500; ++i) deep += "]";
        check(deep, "deep 500");
    }
    {
        std::string large = "[";
        for (int i = 0; i < 20000; ++i) { if (i) large += ","; large += std::to_string(i); }
        large += "]";
        check(large, "large array 20000");
    }
    {
        std::string obj = "{";
        for (int i = 0; i < 5000; ++i) { if (i) obj += ","; obj += "\"k" + std::to_string(i) + "\":" + std::to_string(i * 3 + 1); }
        obj += "}";
        check(obj, "large object 5000");
    }
    {
        std::string s = "\"";
        for (int i = 0; i < 20000; ++i) s += "a\\tb";
        s += "\"";
        check(s, "long escaped string");
    }

    // dump_safe: values the text parser rejects but a program can still hold
    check_safe(std::string("\xff", 1), false, "safe lone 0xff");
    check_safe(std::string("a\xc3", 2), false, "safe truncated 2-byte");
    check_safe(std::string("a\xe4\xb8", 3), false, "safe truncated 3-byte");
    check_safe(std::string("ok\xffok", 5), false, "safe run inside text");
    check_safe(std::string("k\xff", 2), true, "safe invalid key");

    printf("FG494_JSONDUMP cases=%d values=%d failures=%d\n", cases, values, fails);
    return fails ? 1 : 0;
}
