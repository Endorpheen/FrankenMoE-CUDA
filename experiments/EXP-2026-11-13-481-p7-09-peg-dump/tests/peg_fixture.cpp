// Differential fixture for the PEG parser dump (EXP481).
// Oracle: common_peg_arena::dump() from the real peg-parser.cpp.
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include <map>
#include <functional>
#include <variant>

#include "pegdump-gen.h"
#include "peg-parser.h"

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
void common_log_add(struct common_log *, enum ggml_log_level, const char *, ...) {}
struct common_log * common_log_main() { return nullptr; }
int common_log_get_verbosity_thold() { return 0; }

static int cases = 0, values = 0, fails = 0;
static void bad(const char * what, const std::string & detail) {
    ++fails;
    if (fails <= 12) printf("FAIL %s: %s\n", what, detail.c_str());
}

static sl_u8 slv(std::vector<uint8_t> & v) { return sl_u8{ v.data(), (int64_t) v.size() }; }
static sl_i64 sli(std::vector<int64_t> & v) { return sl_i64{ v.data(), (int64_t) v.size() }; }

enum K { K_EPS, K_START, K_END, K_LIT, K_SEQ, K_CHOICE, K_REP, K_AND, K_NOT, K_ANY,
         K_SPACE, K_CHARS, K_STRING, K_UNTIL, K_RULE, K_REF, K_ATOMIC, K_TAG, K_GBNF, K_AC };

struct Spec {
    int kind;
    std::vector<int> ch;
    std::string s;
    std::vector<std::string> delims;
    int mn = 0, mx = 0;
};

struct Interner {
    std::vector<std::string> strings;
    std::map<std::string, int> idx;
    int add(const std::string & s) {
        auto it = idx.find(s);
        if (it != idx.end()) return it->second;
        int i = (int) strings.size();
        strings.push_back(s);
        idx[s] = i;
        return i;
    }
};

static std::vector<Spec> specs;
static int add(const Spec & s) { specs.push_back(s); return (int) specs.size() - 1; }

static void check(int root, bool rule_root, const std::string & rule_name) {
    common_peg_parser_builder b;
    std::vector<common_peg_parser> ps;
    for (const auto & s : specs) {
        common_peg_parser p = b.eps();
        switch (s.kind) {
            case K_EPS: p = b.eps(); break;
            case K_START: p = b.start(); break;
            case K_END: p = b.end(); break;
            case K_LIT: p = b.literal(s.s); break;
            case K_SEQ: {
                auto q = b.sequence();
                for (int c : s.ch) q += ps[c];
                p = q;
                break;
            }
            case K_CHOICE: {
                auto q = b.choice();
                for (int c : s.ch) q |= ps[c];
                p = q;
                break;
            }
            case K_REP: p = b.repeat(ps[s.ch[0]], s.mn, s.mx); break;
            case K_AND: p = b.peek(ps[s.ch[0]]); break;
            case K_NOT: p = b.negate(ps[s.ch[0]]); break;
            case K_ANY: p = b.any(); break;
            case K_SPACE: p = b.space(); break;
            case K_CHARS: p = b.chars(s.s, s.mn, s.mx); break;
            case K_STRING: p = b.string_content(s.s.empty() ? '"' : s.s[0]); break;
            case K_UNTIL: p = b.until(s.s); break;
            case K_RULE: p = b.rule(s.s, ps[s.ch[0]]); break;
            case K_REF: p = b.ref(s.s); break;
            case K_ATOMIC: p = b.atomic(ps[s.ch[0]]); break;
            case K_TAG: p = b.tag(s.s, ps[s.ch[0]]); break;
            case K_GBNF: p = b.gbnf(ps[s.ch[0]], s.s); break;
            case K_AC: p = b.ac(ps[s.ch[0]], s.delims); break;
        }
        ps.push_back(p);
    }
    auto arena = b.build();
    for (size_t i = 0; i < specs.size(); ++i) {
        if (specs[i].kind == K_UNTIL && !specs[i].delims.empty()) {
            auto & u = std::get<common_peg_until_parser>(arena.get(ps[i].id()));
            for (const auto & d : specs[i].delims) u.delimiters.push_back(d);
        }
    }

    // native dump
    common_peg_parser_id nid = ps[root].id();
    if (rule_root) nid = arena.get_rule(rule_name);
    const std::string want = arena.dump(nid);

    // flat encoding
    Interner in;
    std::vector<int64_t> kind, cfirst, ccount, children, sidx, dstart, dcount, delims, mn, mx;
    for (const auto & s : specs) {
        kind.push_back(s.kind);
        cfirst.push_back((int64_t) children.size());
        ccount.push_back((int64_t) s.ch.size());
        for (int c : s.ch) children.push_back(c);
        sidx.push_back(s.s.empty() ? in.add("") : in.add(s.s));
        dstart.push_back((int64_t) delims.size());
        if (s.kind == K_UNTIL) {
            int64_t n = (s.s.empty() ? 0 : 1) + (int64_t) s.delims.size();
            dcount.push_back(n);
            if (!s.s.empty()) delims.push_back(in.add(s.s));
            for (const auto & d : s.delims) delims.push_back(in.add(d));
        } else {
            dcount.push_back((int64_t) s.delims.size());
            for (const auto & d : s.delims) delims.push_back(in.add(d));
        }
        mn.push_back(s.mn);
        mx.push_back(s.mx);
    }
    std::vector<uint8_t> strs(1 << 16, 0);
    std::vector<int64_t> sdir(2 * 1024, 0);
    int64_t off = 0;
    for (size_t i = 0; i < in.strings.size(); ++i) {
        sdir[2 * i + 0] = off;
        sdir[2 * i + 1] = (int64_t) in.strings[i].size();
        for (unsigned char c : in.strings[i]) strs[off++] = c;
    }
    std::vector<uint8_t> out(1 << 16, 0);
    std::vector<int64_t> vis(4096, 0), st(8, 0);
    ++cases;
    const int64_t len = peg_dump(root, sli(kind), sli(cfirst), sli(ccount), sli(children), sli(sidx),
        sli(dstart), sli(dcount), sli(delims), sli(mn), sli(mx), slv(strs), sli(sdir), slv(out), sli(vis), sli(st));
    const std::string got((const char *) out.data(), (size_t) len);
    ++values;
    if (got != want) bad("dump", "got =[" + got + "]\n  want=[" + want + "]");
}

int main(void) {
    goose_init(0, 0);

    // 1. sequence of all leaves
    {
        specs.clear();
        int eps = add({K_EPS, {}, "", {}, 0, 0});
        int st = add({K_START, {}, "", {}, 0, 0});
        int en = add({K_END, {}, "", {}, 0, 0});
        int lit = add({K_LIT, {}, "ab", {}, 0, 0});
        int any = add({K_ANY, {}, "", {}, 0, 0});
        int sp = add({K_SPACE, {}, "", {}, 0, 0});
        int ch = add({K_CHARS, {}, "a-z", {}, 1, 3});
        int str = add({K_STRING, {}, "\"", {}, 0, 0});
        int un = add({K_UNTIL, {}, "END", {}, 0, 0});
        int rule = add({K_RULE, {lit}, "main", {}, 0, 0});
        int ref = add({K_REF, {}, "main", {}, 0, 0});
        int seq = add({K_SEQ, {eps, st, en, lit, any, sp, ch, str, un}, "", {}, 0, 0});
        check(seq, false, "");
    }
    // 2. shared subtree prints [cycle] the second time
    {
        specs.clear();
        int lit = add({K_LIT, {}, "x", {}, 0, 0});
        int seq = add({K_SEQ, {lit, lit}, "", {}, 0, 0});
        check(seq, false, "");
    }
    // 3. choice of sequences
    {
        specs.clear();
        int a = add({K_LIT, {}, "a", {}, 0, 0});
        int s1 = add({K_SEQ, {a}, "", {}, 0, 0});
        int b = add({K_LIT, {}, "b", {}, 0, 0});
        int c = add({K_LIT, {}, "c", {}, 0, 0});
        int s2 = add({K_SEQ, {b, c}, "", {}, 0, 0});
        int ch = add({K_CHOICE, {s1, s2}, "", {}, 0, 0});
        check(ch, false, "");
    }
    // 4. repetitions
    {
        specs.clear();
        int any = add({K_ANY, {}, "", {}, 0, 0});
        int r1 = add({K_REP, {any}, "", {}, 0, -1});
        int lit = add({K_LIT, {}, "ab", {}, 0, 0});
        int r2 = add({K_REP, {lit}, "", {}, 2, 4});
        int seq = add({K_SEQ, {r1, r2}, "", {}, 0, 0});
        check(seq, false, "");
    }
    // 5. lookaheads and atomic
    {
        specs.clear();
        int lit = add({K_LIT, {}, "x", {}, 0, 0});
        int a = add({K_AND, {lit}, "", {}, 0, 0});
        int n = add({K_NOT, {lit}, "", {}, 0, 0});
        int at = add({K_ATOMIC, {lit}, "", {}, 0, 0});
        int seq = add({K_SEQ, {a, n, at}, "", {}, 0, 0});
        check(seq, false, "");
    }
    // 6. tag re-dumps with a fresh visited set
    {
        specs.clear();
        int lit = add({K_LIT, {}, "x", {}, 0, 0});
        int lit2 = add({K_LIT, {}, "y", {}, 0, 0});
        int tag = add({K_TAG, {lit2}, "t", {}, 0, 0});
        int seq = add({K_SEQ, {lit, tag, lit}, "", {}, 0, 0});
        check(seq, false, "");
    }
    // 7. gbnf
    {
        specs.clear();
        int lit = add({K_LIT, {}, "x", {}, 0, 0});
        int g = add({K_GBNF, {lit}, "root ::= \"x\"", {}, 0, 0});
        check(g, false, "");
    }
    // 8. ac with delimiters
    {
        specs.clear();
        int any = add({K_ANY, {}, "", {}, 0, 0});
        int ac = add({K_AC, {any}, "", {"</a>", "</b>", "</c>"}, 0, 0});
        check(ac, false, "");
    }
    // 9. nested choice/sequence
    {
        specs.clear();
        int a = add({K_LIT, {}, "a", {}, 0, 0});
        int b = add({K_LIT, {}, "b", {}, 0, 0});
        int c = add({K_LIT, {}, "c", {}, 0, 0});
        int ch = add({K_CHOICE, {a, b}, "", {}, 0, 0});
        int seq = add({K_SEQ, {ch, c}, "", {}, 0, 0});
        int rep = add({K_REP, {seq}, "", {}, 1, -1});
        check(rep, false, "");
    }
    // 10. a rule dumped by name
    {
        specs.clear();
        int lit = add({K_LIT, {}, "x", {}, 0, 0});
        int seq = add({K_SEQ, {lit}, "", {}, 0, 0});
        int rule = add({K_RULE, {seq}, "main", {}, 0, 0});
        check(rule, true, "main");
    }

    // 11. a ref dumped directly prints Ref(name)
    {
        specs.clear();
        int lit = add({K_LIT, {}, "x", {}, 0, 0});
        int rule = add({K_RULE, {lit}, "main", {}, 0, 0});
        int ref = add({K_REF, {}, "main", {}, 0, 0});
        check(ref, false, "");
    }

    // 12. until with several delimiters and unbounded chars
    {
        specs.clear();
        int un = add({K_UNTIL, {}, "END", {"STOP"}, 0, 0});
        int ch = add({K_CHARS, {}, "0-9", {}, 0, -1});
        int seq = add({K_SEQ, {un, ch}, "", {}, 0, 0});
        check(seq, false, "");
    }

    // 13. Tag re-dumps an already visited child with a fresh set
    {
        specs.clear();
        int lit = add({K_LIT, {}, "x", {}, 0, 0});
        int tag = add({K_TAG, {lit}, "t", {}, 0, 0});
        int any = add({K_ANY, {}, "", {}, 0, 0});
        int seq = add({K_SEQ, {lit, tag, any}, "", {}, 0, 0});
        check(seq, false, "");
    }

    printf("FG481_PEG cases=%d values=%d failures=%d\n", cases, values, fails);
    return fails ? 1 : 0;
}
