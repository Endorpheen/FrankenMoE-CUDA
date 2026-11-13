// Differential fixture for the converter output (EXP496).
// Oracle: json_schema_to_grammar (the public entry) for the error text, and a
// transcription of check_errors/format_grammar for the aggregation and layout.
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include <map>

#include "schemafull-gen.h"
#include "json-schema-to-grammar.h"
#include "json.h"

std::string string_join(const std::vector<std::string> & v, const std::string & sep) {
    std::string r;
    for (size_t i = 0; i < v.size(); ++i) { if (i) r += sep; r += v[i]; }
    return r;
}
std::vector<std::string> string_split(const std::string & s, const std::string & d) {
    std::vector<std::string> o; size_t p = 0;
    while (true) { size_t q = s.find(d, p); if (q == std::string::npos) { o.push_back(s.substr(p)); break; } o.push_back(s.substr(p, q - p)); p = q + d.size(); }
    return o;
}
std::string string_repeat(const std::string & s, size_t n) { std::string r; while (n--) r += s; return r; }

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

struct JsonFlat {
    Interner & in;
    std::vector<int64_t> kind, val, first, cnt, key, child;
    std::vector<double> dbl;
    std::vector<const common_json *> node_ptr;
    explicit JsonFlat(Interner & i) : in(i) {}
    int enc(const common_json & v) {
        const int me = (int) kind.size();
        kind.push_back(0); val.push_back(0); first.push_back(0); cnt.push_back(0);
        dbl.push_back(0.0);
        node_ptr.push_back(&v);
        if (v.is_null()) {
            kind[me] = 0;
        } else if (v.is_boolean()) {
            kind[me] = 1; val[me] = v.get<bool>() ? 1 : 0;
        } else if (v.is_number_integer()) {
            kind[me] = 2; val[me] = v.get<int64_t>();
        } else if (v.is_number()) {
            kind[me] = 3; dbl[me] = v.get<double>();
        } else if (v.is_string()) {
            kind[me] = 4; val[me] = in.sid(v.get<std::string>());
        } else if (v.is_array()) {
            kind[me] = 5;
            std::vector<int> tmp;
            for (const auto & e : v) tmp.push_back(enc(e));
            first[me] = (int64_t) child.size();
            for (int ci : tmp) { key.push_back(-1); child.push_back(ci); }
            cnt[me] = (int64_t) tmp.size();
        } else {
            kind[me] = 6;
            std::vector<std::pair<int, int>> tmp;
            for (const auto & [k, e] : v.items()) {
                const int ci = enc(e);
                tmp.emplace_back(in.sid(k), ci);
            }
            first[me] = (int64_t) child.size();
            for (auto & [ks, ci] : tmp) { key.push_back(ks); child.push_back(ci); }
            cnt[me] = (int64_t) tmp.size();
        }
        return me;
    }
};

// the conversion error for a schema, through the public entry point and through
// the ported builder + wrapper
static void check_error(const std::string & text, const std::string & label) {
    Interner in;
    JsonFlat jf(in);
    common_json root;
    try {
        root = common_json::parse(text);
    } catch (const std::exception & e) {
        bad("json", label + ": " + e.what());
        return;
    }
    std::string nerr;
    bool nok = true;
    try {
        (void) json_schema_to_grammar(root);
    } catch (const std::exception & e) {
        nok = false;
        nerr = e.what();
    }
    if (nok) { bad("expected rejection", label); return; }
    const int rootj = jf.enc(root);
    std::vector<uint8_t> strs(1 << 16, 0);
    std::vector<int64_t> sdir(2 * in.strs.size(), 0);
    int64_t off = 0;
    for (size_t i = 0; i < in.strs.size(); ++i) {
        sdir[2 * i + 0] = off;
        sdir[2 * i + 1] = (int64_t) in.strs[i].size();
        for (unsigned char c : in.strs[i]) strs[off++] = c;
    }
    const int maxn = 1024, maxs = 8192, maxp = 512;
    std::vector<int64_t> nkind(maxn, -1), ncf(maxn, 0), ncc(maxn, 0), nchild(maxs, 0),
        npf(maxn, 0), npc(maxn, 0), pname(maxs, 0), pnode(maxs, 0), preq(maxs, 0), nadd(maxn, -1),
        nstr(maxn, -1), nfmt(maxn, 0), nminl(maxn, 0), nmaxl(maxn, -1),
        nmin(maxn, INT64_MIN), nmax(maxn, INT64_MAX), nitems(maxn, -1), nconst(maxn, -1),
        nef(maxn, 0), nec(maxn, 0), evalv(maxs, 0), ntarget(maxn, -1),
        rk(maxp, 0), rv(maxp, 0), pending(maxp, 0), st(16, 0);
    std::vector<uint8_t> pb(1 << 20, 0), mb(1 << 16, 0), err(1 << 16, 0);
    const int64_t docsid = in.sid("#");
    const int64_t r = schema_build(rootj, docsid, sli(jf.kind), sli(jf.val), slf(jf.dbl),
        sli(jf.first), sli(jf.cnt), sli(jf.key), sli(jf.child), slv(strs), sli(sdir),
        sli(nkind), sli(ncf), sli(ncc), sli(nchild), sli(npf), sli(npc), sli(pname), sli(pnode), sli(preq),
        sli(nadd), sli(nstr), sli(nfmt), sli(nminl), sli(nmaxl), sli(nmin), sli(nmax), sli(nitems),
        sli(nconst), sli(nef), sli(nec), sli(evalv), sli(ntarget), sli(rk), sli(rv), sli(pending),
        slv(pb), slv(mb), slv(err), sli(st));
    ++cases;
    ++values;
    if (r >= 0) { bad("goose accepted", label); return; }
    std::vector<uint8_t> out(1 << 16, 0);
    const int64_t n = conversion_failed(sl_u8{ err.data(), st[5] }, st[5], slv(out));
    const std::string got((const char *) out.data(), (size_t) n);
    ++values;
    if (got != nerr) bad("conversion_failed", label + "\n  got =[" + got + "]\n  want=[" + nerr + "]");
}

// format_grammar over synthetic maps, against the transcription
static void check_format(const std::vector<std::pair<std::string, std::string>> & entries,
                         const std::string & label) {
    std::map<std::string, std::string> m;
    std::string buf;
    std::vector<int64_t> koff, klen, toff, tlen;
    for (const auto & [k, t] : entries) {
        m[k] = t;
        koff.push_back((int64_t) buf.size()); klen.push_back((int64_t) k.size()); buf += k;
        toff.push_back((int64_t) buf.size()); tlen.push_back((int64_t) t.size()); buf += t;
    }
    std::string want;
    for (const auto & kv : m) want += kv.first + " ::= " + kv.second + "\n";
    std::vector<uint8_t> b(buf.begin(), buf.end());
    std::vector<uint8_t> out(1 << 20, 0);
    ++cases;
    const int64_t n = grammar_text(sli(koff), sli(klen), sli(toff), sli(tlen), (int64_t) entries.size(), slv(b), slv(out));
    const std::string got((const char *) out.data(), (size_t) n);
    ++values;
    if (got != want) bad("format_grammar", label + "\n  got =[" + got + "]\n  want=[" + want + "]");
}

static void check_warning(const std::vector<std::string> & msgs, const std::string & label) {
    std::string buf;
    std::vector<int64_t> off, len;
    for (const auto & s : msgs) {
        off.push_back((int64_t) buf.size());
        len.push_back((int64_t) s.size());
        buf += s;
    }
    std::string want = "WARNING: JSON schema conversion was incomplete: " + string_join(msgs, "; ") + "\n";
    std::vector<uint8_t> b(buf.begin(), buf.end());
    std::vector<uint8_t> out(1 << 16, 0);
    ++cases;
    const int64_t n = conversion_warning(slv(b), sli(off), sli(len), (int64_t) msgs.size(), slv(out));
    const std::string got((const char *) out.data(), (size_t) n);
    ++values;
    if (got != want) bad("warning", label + "\n  got =[" + got + "]\n  want=[" + want + "]");
}

int main(void) {
    goose_init(0, 0);

    const std::vector<std::pair<std::string, std::string>> bad_schemas = {
        {"{\"type\":\"bogus\"}", "unrecognized type"},
        {"{\"enum\":[]}", "empty enum"},
        {"{\"$ref\":\"#/x\"}", "unresolved ref"},
        {"{\"type\":\"object\",\"properties\":{\"a\":{\"type\":\"bogus\"}}}", "nested error"},
        {"{\"anyOf\":[{\"type\":\"bogus\"}]}", "error in anyOf"},
        {"{\"allOf\":[{\"type\":\"string\"},{\"enum\":[]}]}", "error in allOf"},
        {"{\"type\":[\"string\",\"bogus\"]}", "error in type array"},
        {"{\"type\":\"array\",\"items\":[{\"type\":\"bogus\"}]}", "error in tuple"},
        {"{\"properties\":[]}", "properties not object"},
        {"{\"type\":\"integer\",\"minimum\":\"x\"}", "minimum not a number"},
    };
    for (const auto & [text, label] : bad_schemas) check_error(text, label);

    check_format({}, "empty map");
    check_format({{"root", "\"a\""}}, "single");
    check_format({{"root", "\"a\""}, {"space", "| \" \""}}, "unsorted pair");
    check_format({{"zeta", "1"}, {"alpha", "2"}, {"mid", "3"}}, "three unsorted");
    check_format({{"a", "1"}, {"ab", "2"}, {"aa", "3"}}, "prefix keys");
    check_format({{"root-10", "x"}, {"root-2", "y"}, {"root", "z"}}, "numeric suffixes");
    check_format({{"Z", "1"}, {"a", "2"}, {"_", "3"}, {"0", "4"}}, "byte order");
    check_format({{"caf\xc3\xa9", "1"}, {"cafe", "2"}}, "non-ascii key");
    check_format({{"k", ""}}, "empty text");
    check_format({{"k", "a ::= b"}}, "text with separator");
    {
        std::vector<std::pair<std::string, std::string>> many;
        for (int i = 0; i < 200; ++i) many.push_back({"rule" + std::to_string(i * 7 % 200), "t" + std::to_string(i)});
        check_format(many, "200 rules");
    }

    check_warning({}, "no warnings");
    check_warning({"one"}, "one warning");
    check_warning({"first", "second", "third"}, "three warnings");

    printf("FG496_FMT cases=%d values=%d failures=%d\n", cases, values, fails);
    return fails ? 1 : 0;
}
