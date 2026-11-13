// Deep and large corpus for the GBNF parser (EXP501).
// Oracle: llama_grammar_parser (src/llama-grammar.cpp); subject:
// experiments/EXP-2026-11-13-480-p7-09-gbnf-parser/source/gbnf.goose.
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include <map>
#include <unistd.h>

#include "gbnf-gen.h"
#include "llama-grammar.h"

static int cases = 0, values = 0, fails = 0;
static void bad(const char * what, const std::string & detail) {
    ++fails;
    if (fails <= 12) printf("FAIL %s: %s\n", what, detail.c_str());
}

static sl_u8 sl(const std::string & s) { return sl_u8{ (uint8_t *) s.data(), (int64_t) s.size() }; }
static sl_u8 slv(std::vector<uint8_t> & v) { return sl_u8{ v.data(), (int64_t) v.size() }; }
static sl_i64 sli(std::vector<int64_t> & v) { return sl_i64{ v.data(), (int64_t) v.size() }; }

static std::string capture_tail(const char * buf, const std::string & text) {
    const std::string s(buf);
    const std::string key = "error parsing grammar: ";
    auto p = s.find(key);
    if (p == std::string::npos) return s;
    auto e = s.find("\n\n" + text, p + key.size());
    return s.substr(p + key.size(), e == std::string::npos ? std::string::npos : e - p - key.size());
}

static bool native_parse(const std::string & text, llama_grammar_parser & p, std::string & err) {
    fflush(stderr);
    FILE * f = tmpfile();
    int saved = -1;
    if (f) { saved = dup(2); dup2(fileno(f), 2); }
    const bool ok = p.parse(text.c_str());
    fflush(stderr);
    if (f) {
        dup2(saved, 2);
        close(saved);
        rewind(f);
        std::string all;
        char buf[65536];
        size_t n;
        while ((n = fread(buf, 1, sizeof buf, f)) > 0) all.append(buf, n);
        fclose(f);
        err = capture_tail(all.c_str(), text);
    }
    return ok;
}

static void check(const std::string & text, const std::string & label, int max_rules) {
    llama_grammar_parser native;
    std::string nerr;
    const bool nok = native_parse(text, native, nerr);

    std::vector<int64_t> wtype(1 << 22, 0), wval(1 << 22, 0), rtype(1 << 22, 0), rval(1 << 22, 0);
    std::vector<int64_t> rstart(1 << 16, 0), rlen(1 << 16, 0), soff(1 << 16, 0), slen(1 << 16, 0), st(16, 0);
    std::vector<uint8_t> sbuf(1 << 24, 0), err(1 << 16, 0);
    const int64_t gerr = parse_grammar(sl(text), sli(wtype), sli(wval), sli(rtype), sli(rval),
        sli(rstart), sli(rlen), slv(sbuf), sli(soff), sli(slen), slv(err), sli(st));
    const bool gok = gerr == 0;
    ++cases;

    ++values;
    if (nok != gok) {
        bad("acceptance", label + " native=" + std::to_string((int) nok) + " goose=" + std::to_string((int) gok) +
                          " err=[" + std::string((const char *) err.data(), (size_t) st[5]) + "]");
        return;
    }
    if (!nok) {
        const std::string gerrtext((const char *) err.data(), (size_t) st[5]);
        ++values;
        if (gerrtext != nerr) {
            std::string gv, nv;
            for (unsigned char c : gerrtext) { if (c==10) gv += "\\n"; else gv.push_back((char)c); }
            for (unsigned char c : nerr) { if (c==10) nv += "\\n"; else nv.push_back((char)c); }
            bad("error text", label + "\n  got (" + std::to_string(gerrtext.size()) + ")=[" + gv +
                "]\n  want(" + std::to_string(nerr.size()) + ")=[" + nv + "]");
        }
        return;
    }

    // symbols
    ++values;
    std::map<std::string, int> nsym;
    for (auto & kv : native.symbol_ids) nsym[kv.first] = (int) kv.second;
    std::map<std::string, int> gsym;
    for (int64_t i = 0; i < st[1]; ++i) {
        gsym[std::string((const char *) sbuf.data() + soff[i], (size_t) slen[i])] = (int) i;
    }
    if (nsym != gsym) {
        bad("symbols", label + " counts native=" + std::to_string(nsym.size()) + " goose=" + std::to_string(gsym.size()));
        return;
    }

    // every rule's elements
    ++values;
    if ((int64_t) native.rules.size() != st[2]) {
        bad("rule count", label + " native=" + std::to_string(native.rules.size()) + " goose=" + std::to_string(st[2]));
        return;
    }
    const int64_t nrules = st[2] < (int64_t) native.rules.size() ? st[2] : (int64_t) native.rules.size();
    if (max_rules > 0 && nrules > max_rules) {
        // still compare all of them; the argument only documents the scale
    }
    for (int64_t r = 0; r < nrules; ++r) {
        const auto & nr = native.rules[(size_t) r];
        ++values;
        if ((int64_t) nr.size() != rlen[r]) {
            bad("rule length", label + " rule " + std::to_string(r) +
                " native=" + std::to_string(nr.size()) + " goose=" + std::to_string(rlen[r]));
            return;
        }
        for (int64_t k = 0; k < rlen[r]; ++k) {
            ++values;
            if ((int64_t) nr[(size_t) k].type != rtype[rstart[r] + k] ||
                (int64_t) nr[(size_t) k].value != rval[rstart[r] + k]) {
                bad("element", label + " rule " + std::to_string(r) + " elem " + std::to_string(k));
                return;
            }
        }
    }
}

int main(void) {
    goose_init(0, 0);

    { // 500 nested groups
        std::string g = "root ::= ";
        for (int i = 0; i < 500; ++i) g += "(";
        g += "\"a\"";
        for (int i = 0; i < 500; ++i) g += ")";
        g += "\n";
        check(g, "deep 500 groups", 0);
    }
    { // 20000 rules
        std::string g;
        g.reserve(20000 * 12 + 16);
        for (int i = 0; i < 20000; ++i) { g += "r"; g += std::to_string(i); g += " ::= \"a\"\n"; }
        g += "root ::= r0\n";
        check(g, "20000 rules", 0);
    }
    { // a 100000-byte literal
        std::string g = "root ::= \"";
        g += std::string(100000, 'a');
        g += "\"\n";
        check(g, "100k literal", 0);
    }
    { // a 4000-byte literal repeated twice
        std::string g = "root ::= \"";
        g += std::string(4000, 'a');
        g += "\"{2}\n";
        check(g, "4k literal {2}", 0);
    }
    { // 10000 alternatives
        std::string g = "root ::= \"a\"";
        for (int i = 1; i < 10000; ++i) { g += " | \"b\""; }
        g += "\n";
        check(g, "10000 alternatives", 0);
    }
    { // 200 nested repetitions of {2}
        std::string g = "root ::= ";
        for (int i = 0; i < 200; ++i) g += "(";
        g += "\"a\"";
        for (int i = 0; i < 200; ++i) g += "){2}";
        g += "\n";
        check(g, "nested {2} x200", 0);
    }
    { // malformed: unterminated 100000-byte literal
        std::string g = "root ::= \"";
        g += std::string(100000, 'a');
        g += "\n";
        check(g, "unterminated 100k literal", 0);
    }
    { // malformed: missing ::= after a long name
        std::string g = "r";
        g += std::string(300, 'x');
        g += " \"a\"\n";
        check(g, "long name missing ::=", 0);
    }
    { // 1500 repetitions of a doubly grouped literal: the multiplied-rules guard
        std::string g = std::string("root ::= ((") + "\"a\"" + ")){1500}\n";
        check(g, "multiplied-rules guard", 0);
    }
    { // malformed: 2001 repetitions at the guard
        std::string g = "root ::= \"a\"{2001}\n";
        check(g, "repetition guard", 0);
    }

    printf("FG501_GBNF cases=%d values=%d failures=%d\n", cases, values, fails);
    return fails ? 1 : 0;
}
