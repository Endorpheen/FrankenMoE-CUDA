// Differential fixture for the GBNF rule-text parser (EXP480).
// Oracle: the real llama_grammar_parser from src/llama-grammar.cpp.
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
    // the printed form is "<msg>\n\n<text>\n"; anchor on the known text
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
        char buf[16384];
        size_t n = fread(buf, 1, sizeof buf - 1, f);
        buf[n] = 0;
        fclose(f);
        err = capture_tail(buf, text);
    }
    return ok;
}

int main(void) {
    goose_init(0, 0);

    const std::vector<std::string> grammars = {
        "root ::= \"a\"\n",
        "root ::= \"a\" | \"b\"\n",
        "root ::= [a-z]+\n",
        "root ::= [^abc]\n",
        "root ::= x y\nx ::= \"1\"\ny ::= \"2\"\n",
        "root ::= (\"a\" | \"b\") \"c\"\n",
        "root ::= \"a\"*\n",
        "root ::= \"a\"+\n",
        "root ::= \"a\"?\n",
        "root ::= \"a\"{2}\n",
        "root ::= \"a\"{2,}\n",
        "root ::= \"a\"{2,4}\n",
        "root ::= \"a\"{0,1}\n",
        "root ::= \"\\n\" \"\\r\" \"\\t\"\n",
        "root ::= \"\\x41\" \"\\u0041\" \"\\U00000041\"\n",
        "root ::= \"\\\\\" \"\\\"\" \"\\[\" \"\\]\" \"\\-\"\n",
        "root ::= \"caf\xc3\xa9\"\n",
        "root ::= [a-zA-Z0-9_]\n",
        "root ::= [^\"\\\\]\n",
        "root ::= [\\x41-\\x5A]\n",
        "root ::= .\n",
        "# comment\nroot ::= \"a\" # trailing\n",
        "root ::= ((\"a\" | \"b\") \"c\")+\n",
        "root ::= a\na ::= \"x\"\n",
        "root ::= \"a\" |\n",
        "root ::= \"a\"\n\nroot2 ::= [b-c]{2,3}\n",
        "root ::= [\xc3\xa9\xc3\xa8]\n",
        // invalid / errors
        "root\n",
        "root ::=\n",
        "root ::= \"a\n",
        "root ::= [a\n",
        "root ::= \\q\n",
        "root ::= \"\\x4\"\n",
        "root ::= a\n",
        "root ::= *\n",
        "root ::= \"a\"{x}\n",
        "root ::= \"a\"{2\n",
        "root ::= \"a\"{2,\n",
        "root ::= \"a\"{2,3\n",
        "root ::= (\"a\"\n",
        "root ::= \"a\"\nextra\n",
        "root ::= [a-]\n",
        "root ::= \"a\"{2001}\n",
        "root ::= \"\\q\"\n",
        "root ::= [\\q]\n",
        "root ::\n",
    };

    for (const auto & text : grammars) {
        llama_grammar_parser native;
        std::string nerr;
        const bool nok = native_parse(text, native, nerr);

        std::vector<int64_t> wtype(1 << 16, 0), wval(1 << 16, 0), rtype(1 << 16, 0), rval(1 << 16, 0);
        std::vector<int64_t> rstart(4096, 0), rlen(4096, 0), soff(4096, 0), slen(4096, 0), st(16, 0);
        std::vector<uint8_t> sbuf(1 << 16, 0), err(1 << 16, 0);
        const int64_t gerr = parse_grammar(sl(text), sli(wtype), sli(wval), sli(rtype), sli(rval),
            sli(rstart), sli(rlen), slv(sbuf), sli(soff), sli(slen), slv(err), sli(st));
        const bool gok = gerr == 0;
        ++cases;

        ++values;
        if (nok != gok) {
            bad("acceptance", "grammar=[" + text + "] native=" + std::to_string((int) nok) +
                              " goose=" + std::to_string((int) gok) +
                              " err=[" + std::string((const char *) err.data(), (size_t) st[5]) + "]");
            continue;
        }
        if (!nok) {
            const std::string gerrtext((const char *) err.data(), (size_t) st[5]);
            ++values;
            if (gerrtext != nerr) bad("error text", "native=[" + nerr + "] goose=[" + gerrtext + "]");
            continue;
        }

        // symbols
        std::map<std::string, int> nsym;
        for (auto & kv : native.symbol_ids) nsym[kv.first] = (int) kv.second;
        std::map<std::string, int> gsym;
        for (int64_t i = 0; i < st[1]; ++i) {
            gsym[std::string((const char *) sbuf.data() + soff[i], (size_t) slen[i])] = (int) i;
        }
        ++values;
        if (nsym != gsym) {
            std::string diff;
            for (auto & kv : nsym) if (!gsym.count(kv.first)) diff += " missing " + kv.first;
            for (auto & kv : gsym) if (!nsym.count(kv.first)) diff += " extra " + kv.first;
            bad("symbols", "grammar=[" + text + "]" + diff);
            continue;
        }

        // rules and elements
        ++values;
        if ((int64_t) native.rules.size() != st[2]) {
            bad("rule count", "grammar=[" + text + "] native=" + std::to_string(native.rules.size()) +
                              " goose=" + std::to_string(st[2]));
            continue;
        }
        bool ok = true;
        for (int64_t r = 0; r < st[2] && ok; ++r) {
            const auto & nr = native.rules[r];
            ++values;
            if ((int64_t) nr.size() != rlen[r]) {
                bad("rule length", "grammar=[" + text + "] rule=" + std::to_string(r) +
                                   " native=" + std::to_string(nr.size()) + " goose=" + std::to_string(rlen[r]));
                ok = false;
                break;
            }
            for (int64_t k = 0; k < rlen[r]; ++k) {
                ++values;
                if ((int64_t) nr[k].type != rtype[rstart[r] + k] ||
                    (int64_t) nr[k].value != rval[rstart[r] + k]) {
                    bad("element", "grammar=[" + text + "] rule=" + std::to_string(r) +
                                   " elem=" + std::to_string(k) +
                                   " native=(" + std::to_string((int) nr[k].type) + "," + std::to_string(nr[k].value) + ")" +
                                   " goose=(" + std::to_string(rtype[rstart[r] + k]) + "," + std::to_string(rval[rstart[r] + k]) + ")");
                    ok = false;
                    break;
                }
            }
        }
    }

    printf("FG480_GBNF cases=%d values=%d failures=%d\n", cases, values, fails);
    return fails ? 1 : 0;
}
