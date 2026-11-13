// Differential fixture for PEG ref resolution (EXP499).
// Oracle: common_peg_arena::resolve_refs, reached through the public get()/
// set_root()/resolve_refs() surface: a ref parser is injected into a container
// child, then the arena is resolved natively and compared with the port.
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include <map>
#include <variant>

#include "resolve-gen.h"
#include "peg-parser.h"
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

struct Enc {
    std::vector<int64_t> kind, cf, cc, child, sidx;
};

static int64_t kind_of(const common_peg_parser_variant & v) {
    if (std::holds_alternative<common_peg_epsilon_parser>(v)) return 0;
    if (std::holds_alternative<common_peg_start_parser>(v)) return 1;
    if (std::holds_alternative<common_peg_end_parser>(v)) return 2;
    if (std::holds_alternative<common_peg_literal_parser>(v)) return 3;
    if (std::holds_alternative<common_peg_sequence_parser>(v)) return 4;
    if (std::holds_alternative<common_peg_choice_parser>(v)) return 5;
    if (std::holds_alternative<common_peg_repetition_parser>(v)) return 6;
    if (std::holds_alternative<common_peg_and_parser>(v)) return 7;
    if (std::holds_alternative<common_peg_not_parser>(v)) return 8;
    if (std::holds_alternative<common_peg_any_parser>(v)) return 9;
    if (std::holds_alternative<common_peg_space_parser>(v)) return 10;
    if (std::holds_alternative<common_peg_chars_parser>(v)) return 11;
    if (std::holds_alternative<common_peg_string_parser>(v)) return 12;
    if (std::holds_alternative<common_peg_until_parser>(v)) return 13;
    if (std::holds_alternative<common_peg_rule_parser>(v)) return 14;
    if (std::holds_alternative<common_peg_ref_parser>(v)) return 15;
    if (std::holds_alternative<common_peg_atomic_parser>(v)) return 16;
    if (std::holds_alternative<common_peg_tag_parser>(v)) return 17;
    if (std::holds_alternative<common_peg_gbnf_parser>(v)) return 18;
    if (std::holds_alternative<common_peg_ac_parser>(v)) return 19;
    return 20; // schema
}

static void encode(const common_peg_arena & a, Interner & in, Enc & e) {
    const size_t n = a.size();
    e.kind.assign(n, -1); e.cf.assign(n, 0); e.cc.assign(n, 0); e.sidx.assign(n, -1);
    e.child.clear();
    for (size_t i = 0; i < n; ++i) {
        const auto & v = a.get(i);
        e.kind[i] = kind_of(v);
        std::visit([&](auto & p) {
            using T = std::decay_t<decltype(p)>;
            if constexpr (std::is_same_v<T, common_peg_sequence_parser> || std::is_same_v<T, common_peg_choice_parser>) {
                e.cf[i] = (int64_t) e.child.size();
                for (auto c : p.children) e.child.push_back((int64_t) c);
                e.cc[i] = (int64_t) p.children.size();
            } else if constexpr (std::is_same_v<T, common_peg_rule_parser>) {
                e.cf[i] = (int64_t) e.child.size(); e.child.push_back((int64_t) p.child); e.cc[i] = 1;
                e.sidx[i] = in.sid(p.name);
            } else if constexpr (std::is_same_v<T, common_peg_ref_parser>) {
                e.sidx[i] = in.sid(p.name);
            } else if constexpr (std::is_same_v<T, common_peg_repetition_parser> ||
                                 std::is_same_v<T, common_peg_and_parser> ||
                                 std::is_same_v<T, common_peg_not_parser> ||
                                 std::is_same_v<T, common_peg_atomic_parser> ||
                                 std::is_same_v<T, common_peg_tag_parser> ||
                                 std::is_same_v<T, common_peg_gbnf_parser> ||
                                 std::is_same_v<T, common_peg_ac_parser> ||
                                 std::is_same_v<T, common_peg_schema_parser>) {
                e.cf[i] = (int64_t) e.child.size(); e.child.push_back((int64_t) p.child); e.cc[i] = 1;
            }
        }, v);
    }
}

// does the container hold the injected ref?
static int64_t first_child(const common_peg_arena & a, size_t id) {
    const auto & v = a.get(id);
    return std::visit([&](auto & p) -> int64_t {
        using T = std::decay_t<decltype(p)>;
        if constexpr (std::is_same_v<T, common_peg_sequence_parser> || std::is_same_v<T, common_peg_choice_parser>) {
            return p.children.empty() ? -1 : (int64_t) p.children[0];
        } else if constexpr (std::is_same_v<T, common_peg_rule_parser> ||
                             std::is_same_v<T, common_peg_repetition_parser> ||
                             std::is_same_v<T, common_peg_and_parser> ||
                             std::is_same_v<T, common_peg_not_parser> ||
                             std::is_same_v<T, common_peg_atomic_parser> ||
                             std::is_same_v<T, common_peg_tag_parser> ||
                             std::is_same_v<T, common_peg_gbnf_parser> ||
                             std::is_same_v<T, common_peg_ac_parser> ||
                             std::is_same_v<T, common_peg_schema_parser>) {
            return (int64_t) p.child;
        }
        return -1;
    }, v);
}

static void inject(common_peg_arena & a, size_t id, int slot, size_t refid) {
    auto & v = a.get(id);
    std::visit([&](auto & p) {
        using T = std::decay_t<decltype(p)>;
        if constexpr (std::is_same_v<T, common_peg_sequence_parser> || std::is_same_v<T, common_peg_choice_parser>) {
            p.children[(size_t) slot] = refid;
        } else if constexpr (std::is_same_v<T, common_peg_rule_parser> ||
                             std::is_same_v<T, common_peg_repetition_parser> ||
                             std::is_same_v<T, common_peg_and_parser> ||
                             std::is_same_v<T, common_peg_not_parser> ||
                             std::is_same_v<T, common_peg_atomic_parser> ||
                             std::is_same_v<T, common_peg_tag_parser> ||
                             std::is_same_v<T, common_peg_gbnf_parser> ||
                             std::is_same_v<T, common_peg_ac_parser> ||
                             std::is_same_v<T, common_peg_schema_parser>) {
            p.child = refid;
        }
    }, v);
}

// build a tree with rule "A" plus an unattached ref to it, and report the first
// parser of the wanted kind as the injection target
struct Tree {
    common_peg_arena arena;
    size_t target = 0;
    size_t refid = 0;
    std::vector<std::string> names;
};

static Tree make_tree(int which) {
    common_peg_parser_builder b;
    auto rA = b.rule("A", b.literal("x"));
    const size_t refid = rA.id();
    common_peg_parser p = b.eps();
    switch (which) {
        case 0: { auto s = b.sequence(); s += b.literal("a"); s += b.literal("b"); p = s; break; }
        case 1: { auto c = b.choice();  c |= b.literal("a"); c |= b.literal("b"); p = c; break; }
        case 2: p = b.repeat(b.literal("a"), 0, -1); break;
        case 3: p = b.peek(b.literal("a")); break;
        case 4: p = b.negate(b.literal("a")); break;
        case 5: p = b.atomic(b.literal("a")); break;
        case 6: p = b.tag("t", b.literal("a")); break;
        case 7: p = b.gbnf(b.literal("a"), "g"); break;
        case 8: p = b.ac(b.literal("a"), std::vector<std::string>{"</a>"}); break;
        case 9: p = b.schema(b.literal("a"), "s", common_json::parse("{\"type\":\"string\"}"), true); break;
        case 10: p = b.rule("R", b.literal("a")); break; // the target is the rule parser itself
        default: break;
    }
    b.set_root(p);
    Tree t;
    t.arena = b.build();
    t.refid = refid;
    t.names = {"A", "R"};
    // the rule parser of "R" is added before the ref it returns; find it by kind
    t.target = SIZE_MAX;
    const int64_t want = (which == 10) ? 14 : kind_of(t.arena.get(p.id()));
    // the last parser of that kind is the container the call chain ended on
    for (size_t i = t.arena.size(); i > 0; --i) {
        if (kind_of(t.arena.get(i - 1)) == want) { t.target = i - 1; break; }
    }
    return t;
}

static void check_inject(int which, int slot, const std::string & label) {
    Tree t = make_tree(which);
    if (t.target == SIZE_MAX) { bad("target", label); return; }
    inject(t.arena, t.target, slot, t.refid);

    Interner in;
    Enc pre;
    encode(t.arena, in, pre);

    // the port: arrays sized from the encoding
    std::vector<int64_t> child = pre.child;
    std::vector<int64_t> rname, rid;
    for (const auto & nm : t.names) {
        if (!t.arena.has_rule(nm)) continue;
        rname.push_back(in.sid(nm));
        rid.push_back((int64_t) t.arena.get_rule(nm));
    }
    std::vector<uint8_t> strs, sdirb;
    std::vector<int64_t> sdir;
    for (size_t i = 0; i < in.strs.size(); ++i) {
        sdir.push_back((int64_t) strs.size());
        sdir.push_back((int64_t) in.strs[i].size());
        for (unsigned char c : in.strs[i]) strs.push_back(c);
    }
    std::vector<int64_t> out(4, -1);
    const int64_t root = (int64_t) t.arena.root();

    ++cases;
    const int64_t rc = resolve_refs(sli(pre.kind), sli(pre.cf), sli(pre.cc), sli(child), sli(pre.sidx),
        sli(rname), sli(rid), (int64_t) rname.size(), root, sli(out), slv(strs), sli(sdir));
    ++values;
    if (rc != 0) { bad("unexpected error", label); return; }

    // the native rewrite on the same arena
    t.arena.resolve_refs();
    Enc post;
    encode(t.arena, in, post);

    const std::string ctx = label + " slot " + std::to_string(slot);
    ++values;
    if (out[0] != (int64_t) t.arena.root()) {
        bad("root", ctx + " goose=" + std::to_string(out[0]) + " native=" + std::to_string(t.arena.root()));
    }
    ++values;
    if (child.size() != post.child.size()) {
        bad("child count", ctx);
        return;
    }
    for (size_t i = 0; i < child.size(); ++i) {
        ++values;
        if (child[i] != post.child[i]) {
            bad("child", ctx + " index " + std::to_string(i) +
                " goose=" + std::to_string(child[i]) + " native=" + std::to_string(post.child[i]));
        }
    }
    for (size_t i = 0; i < pre.kind.size(); ++i) {
        ++values;
        if (pre.kind[i] != post.kind[i]) bad("kind changed", ctx + " id " + std::to_string(i));
    }
}

static void check_root_ref(const std::string & label) {
    common_peg_parser_builder b;
    auto rA = b.rule("A", b.literal("x"));
    b.set_root(rA);
    auto arena = b.build(); // build already flattens the root

    Interner in;
    Enc pre;
    encode(arena, in, pre);
    std::vector<int64_t> child = pre.child;
    std::vector<int64_t> rname{in.sid("A")}, rid{(int64_t) arena.get_rule("A")};
    std::vector<uint8_t> strs, sdirb;
    std::vector<int64_t> sdir;
    for (size_t i = 0; i < in.strs.size(); ++i) {
        sdir.push_back((int64_t) strs.size());
        sdir.push_back((int64_t) in.strs[i].size());
        for (unsigned char c : in.strs[i]) strs.push_back(c);
    }
    // the root is already a rule id after build; feed the pre-resolution shape
    // by pointing the root back at the ref parser
    size_t refid = SIZE_MAX;
    for (size_t i = 0; i < arena.size(); ++i) {
        if (kind_of(arena.get(i)) == 15) { refid = i; break; }
    }
    ++cases;
    if (refid == SIZE_MAX) { bad("root ref", label + ": no ref parser"); return; }
    std::vector<int64_t> out(4, -1);
    const int64_t rc = resolve_refs(sli(pre.kind), sli(pre.cf), sli(pre.cc), sli(child), sli(pre.sidx),
        sli(rname), sli(rid), 1, (int64_t) refid, sli(out), slv(strs), sli(sdir));
    ++values;
    if (rc != 0) { bad("root resolve failed", label); return; }
    ++values;
    if (out[0] != (int64_t) rid[0]) bad("root flatten", label + " goose=" + std::to_string(out[0]) + " native=" + std::to_string(rid[0]));
    ++values;
    if ((int64_t) arena.root() != rid[0]) bad("native root", label);
}

int main(void) {
    goose_init(0, 0);

    const char * labels[] = { "sequence", "choice", "repetition", "peek", "negate",
                              "atomic", "tag", "gbnf", "ac", "schema", "rule" };
    for (int which = 0; which < 11; ++which) {
        for (int slot = 0; slot < 2; ++slot) {
            check_inject(which, slot, labels[which]);
        }
    }
    check_root_ref("root ref");

    // an unknown rule name: the native build throws and the port reports it
    {
        common_peg_parser_builder b;
        auto rA = b.rule("A", b.literal("x"));
        auto missing = b.ref("missing");
        auto seq = b.sequence();
        seq += b.literal("a");
        seq += missing;
        b.set_root(seq);
        std::string nerr;
        bool threw = false;
        try {
            auto arena = b.build();
            (void) arena;
        } catch (const std::exception & e) {
            threw = true;
            nerr = e.what();
        }
        ++cases;
        ++values;
        if (!threw) { bad("unknown rule", "native did not throw"); }
        else {
            ++values;
            if (nerr.find("Rule not found: missing") == std::string::npos) bad("unknown rule text", "native=[" + nerr + "]");
        }
        (void) rA;

        // the same shape encoded by hand, with the ref name replaced by "missing"
        common_peg_parser_builder b2;
        auto rA2 = b2.rule("A", b2.literal("x"));
        auto ref2 = b2.ref("A");
        auto seq2 = b2.sequence();
        seq2 += b2.literal("a");
        seq2 += ref2;
        b2.set_root(seq2);
        auto arena2 = b2.build();
        (void) rA2;

        Interner in;
        Enc enc;
        encode(arena2, in, enc);
        const int miss = in.sid("missing");
        enc.sidx[(size_t) ref2.id()] = miss;
        // point the sequence's ref slot back at the ref parser: after build the
        // arena already holds the resolved rule id there
        for (size_t i = enc.kind.size(); i > 0; --i) {
            if (enc.kind[i - 1] == 4 && enc.cc[i - 1] >= 2) {
                enc.child[(size_t) enc.cf[i - 1] + 1] = (int64_t) ref2.id();
                break;
            }
        }

        std::vector<int64_t> child = enc.child;
        std::vector<int64_t> rname{in.sid("A")}, rid{(int64_t) arena2.get_rule("A")};
        std::vector<uint8_t> strs;
        std::vector<int64_t> sdir;
        for (size_t i = 0; i < in.strs.size(); ++i) {
            sdir.push_back((int64_t) strs.size());
            sdir.push_back((int64_t) in.strs[i].size());
            for (unsigned char c : in.strs[i]) strs.push_back(c);
        }
        std::vector<int64_t> out(4, -1);
        ++values;
        const int64_t rc = resolve_refs(sli(enc.kind), sli(enc.cf), sli(enc.cc), sli(child), sli(enc.sidx),
            sli(rname), sli(rid), 1, (int64_t) arena2.root(), sli(out), slv(strs), sli(sdir));
        if (rc != 1) bad("port unknown", "rc=" + std::to_string(rc));
        ++values;
        if (out[1] != miss) bad("port unknown name", "got=" + std::to_string(out[1]) + " want=" + std::to_string(miss));
    }

    printf("FG499_RESOLVE cases=%d values=%d failures=%d\n", cases, values, fails);
    return fails ? 1 : 0;
}
