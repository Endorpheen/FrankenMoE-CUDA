// Differential gate for slice 2: reasoning and TAG_WITH_JSON.
// Expected strings are the verbatim native dump= lines from probe.out.
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <cmath>
#include <cstdlib>
#include <string>
#include <vector>

#include "runtime-gen.h"

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
    const std::string t = std::to_string(x);
    memcpy(out.data, t.data(), t.size());
    return (int64_t) t.size();
}
extern "C" int64_t gs_strftime_epoch(sl_u8 out, int64_t epoch, sl_u8 fmt) {
    const std::string f((const char *) fmt.data, (size_t) fmt.len);
    const std::time_t t = epoch < 0 ? std::time(nullptr) : (std::time_t) epoch;
    const std::tm * tmv = std::localtime(&t);
    if (!tmv) return -1;
    return (int64_t) std::strftime((char *) out.data, (size_t) out.len, f.c_str(), tmv);
}

static int cases = 0, values = 0, fails = 0;

struct Arena {
    std::vector<int64_t> kind, cfirst, ccount, children, sidx, dstart, dcount, mn, mx, st, vis, vis2;
    std::vector<uint8_t> pool, out;
    Arena()
        : kind(1024, 0), cfirst(1024, 0), ccount(1024, 0), children(8192, 0), sidx(1024, 0),
          dstart(1024, 0), dcount(1024, 0), mn(1024, 0), mx(1024, 0), st(128, 0),
          vis(1024, 0), vis2(1024, 0), pool(1 << 16, 0), out(1 << 18, 0) {}
    sl_i64 i(std::vector<int64_t> & v) { return sl_i64{v.data(), (int64_t) v.size()}; }
    sl_u8 b(std::vector<uint8_t> & v) { return sl_u8{v.data(), (int64_t) v.size()}; }
    sl_u8 str(const std::string & s) {
        buf.assign(s.begin(), s.end());
        return sl_u8{buf.data(), (int64_t) buf.size()};
    }
    std::vector<uint8_t> buf;
};

static sl_i64 li(std::vector<int64_t> & v) { return sl_i64{v.data(), (int64_t) v.size()}; }
static sl_u8 bu(std::vector<uint8_t> & v) { return sl_u8{v.data(), (int64_t) v.size()}; }

static std::string dump_of(int64_t root, Arena & a) {
    int64_t len = peg_dump_basic(root, li(a.vis), li(a.vis2), li(a.kind), li(a.cfirst), li(a.ccount),
        li(a.children), li(a.sidx), li(a.dstart), li(a.dcount), li(a.mn), li(a.mx), bu(a.pool), bu(a.out), li(a.st));
    return std::string((const char *) a.out.data(), (size_t) len);
}

static std::string content(const std::string & gp, const std::string & cs, const std::string & ce, int64_t wrapped) {
    Arena a;
    std::vector<uint8_t> g(gp.begin(), gp.end()), s(cs.begin(), cs.end()), e(ce.begin(), ce.end());
    int64_t root = autoparser_build_content(bu(g), bu(s), bu(e), wrapped, li(a.kind), li(a.cfirst),
        li(a.ccount), li(a.children), li(a.sidx), li(a.dstart), li(a.dcount), li(a.mn), li(a.mx),
        bu(a.pool), li(a.st));
    return dump_of(root, a);
}

static std::string reasoning(const std::string & gp, const std::string & rs, const std::string & re,
                             const std::string & cs, const std::string & ce, int64_t wrapped) {
    Arena a;
    std::vector<uint8_t> g(gp.begin(), gp.end()), r1(rs.begin(), rs.end()), r2(re.begin(), re.end());
    std::vector<uint8_t> c1(cs.begin(), cs.end()), c2(ce.begin(), ce.end());
    int64_t root = autoparser_build_reasoning(bu(g), bu(r1), bu(r2), bu(c1), bu(c2), wrapped,
        li(a.kind), li(a.cfirst), li(a.ccount), li(a.children), li(a.sidx), li(a.dstart),
        li(a.dcount), li(a.mn), li(a.mx), bu(a.pool), li(a.st));
    return dump_of(root, a);
}

static std::string tag_json(const std::string & gp, int64_t reasoning_on, int64_t parallel, int64_t required) {
    Arena a;
    const std::string prefix = "<function=";
    const std::string suffix = ">";
    const std::string close = "";
    const std::string astart = "", aend = "", sstart = "", send = "", pcstart = "", pcend = "";
    const std::string name_blob = "get_weatherget_time";
    const std::string schema_blob =
        "{\"type\":\"object\",\"properties\":{\"arg1\":{\"type\":\"string\"},\"arg2\":{\"type\":\"number\"}},\"required\":[\"arg1\"]}"
        "{\"type\":\"object\",\"properties\":{}}";
    std::vector<int64_t> noff{0, 11}, nlen{11, 8}, soff{0, 102}, slen{102, 33};

    std::vector<uint8_t> g(gp.begin(), gp.end());
    std::vector<uint8_t> rs{'<','t','h','i','n','k','>'};
    std::vector<uint8_t> re{'<','/','t','h','i','n','k','>'};
    std::vector<uint8_t> p(prefix.begin(), prefix.end()), s(suffix.begin(), suffix.end());
    std::vector<uint8_t> cl(close.begin(), close.end()), as(astart.begin(), astart.end()), ae(aend.begin(), aend.end());
    std::vector<uint8_t> ss(sstart.begin(), sstart.end()), se(send.begin(), send.end());
    std::vector<uint8_t> pcs(pcstart.begin(), pcstart.end()), pce(pcend.begin(), pcend.end());
    std::vector<uint8_t> nm(name_blob.begin(), name_blob.end());
    std::vector<uint8_t> sc(schema_blob.begin(), schema_blob.end());
    std::vector<uint8_t> cidp, cidq;

    int64_t root = autoparser_build_tag_json(bu(g), bu(rs), bu(re), reasoning_on, bu(p), bu(s), bu(cl),
        bu(as), bu(ae), bu(ss), bu(se), bu(pcs), bu(pce), bu(cidp), bu(cidq), 0, 0,
        bu(nm), li(noff), li(nlen), bu(sc),
        li(soff), li(slen), 2, parallel, required, li(a.kind), li(a.cfirst), li(a.ccount),
        li(a.children), li(a.sidx), li(a.dstart), li(a.dcount), li(a.mn), li(a.mx), bu(a.pool), li(a.st));
    return dump_of(root, a);
}

static const char * expected[] = {{R"XD(Sequence(Literal(assistant: ), Epsilon, Tag(content, Until()), End))XD"}, {R"XD(Sequence(Literal(<|assistant|>), Space, Repetition(Sequence(Epsilon, Literal(<think>), Tag(reasoning, Until(</think>)), Epsilon, Literal(</think>)), 0, 1), Tag(content, Until()), End))XD"}, {R"XD(Sequence(Literal(<|assistant|>), Space, Repetition(Sequence(Epsilon, Literal(<think>), Tag(reasoning, Until(</think>)), Epsilon, Literal(</think>)), 0, 1), Literal(<response>), Tag(content, Until(</response>)), Literal(</response>), End))XD"}, {R"XD(Sequence(Literal(<assistant>), Space, Epsilon, Repetition(Tag(content, Epsilon), 0, 1), Repetition(Rule(tool-call, Sequence(Literal(), Choice(Rule(tool-get-weather, Sequence(Atomic(Tag(tool-open, Sequence(Literal(<function=), Atomic(Tag(tool-name, Literal(get_weather))), Literal(>)))), Epsilon, Space, Tag(tool-args, Schema(Rule(json-value, Choice(Rule(json-object, Sequence(Literal({), Space, Choice(Literal(}), Sequence(Rule(json-string, Sequence(Literal("), String("), Literal("))), [cycle], Literal(:), [cycle], [cycle], Repetition(Sequence([cycle], Literal(,), [cycle], [cycle], [cycle], [cycle], [cycle], [cycle]), 0, unbounded), [cycle], Literal(}))))), Rule(json-array, Sequence(Literal([), Space, Choice(Literal(]), Sequence([cycle], Repetition(Sequence([cycle], Literal(,), [cycle], [cycle]), 0, unbounded), [cycle], Literal(]))))), [cycle], Rule(json-number, Sequence(Repetition(Literal(-), 0, 1), Choice(Literal(0), Sequence(CharRepeat([1-9], 1, 1), CharRepeat([0-9], 0, unbounded))), Repetition(Sequence(Literal(.), CharRepeat([0-9], 1, unbounded)), 0, 1), Repetition(Sequence(Choice(Literal(e), Literal(E)), Repetition(CharRepeat([+-], 1, 1), 0, 1), [cycle]), 0, 1), Not(CharRepeat([0-9.eE+-], 1, 1)))), Rule(json-bool, Choice(Literal(true), Literal(false))), Rule(json-null, Literal(null)))), {"type":"object","properties":{"arg1":{"type":"string"},"arg2":{"type":"number"}},"required":["arg1"]})), Atomic(Tag(tool-close, Space)))), Rule(tool-get-time, Sequence(Atomic(Tag(tool-open, Sequence(Literal(<function=), Atomic(Tag(tool-name, Literal(get_time))), Literal(>)))), Epsilon, Space, Tag(tool-args, Schema(Rule(json-value, Choice(Rule(json-object, Sequence(Literal({), Space, Choice(Literal(}), Sequence(Rule(json-string, Sequence(Literal("), String("), Literal("))), [cycle], Literal(:), [cycle], [cycle], Repetition(Sequence([cycle], Literal(,), [cycle], [cycle], [cycle], [cycle], [cycle], [cycle]), 0, unbounded), [cycle], Literal(}))))), Rule(json-array, Sequence(Literal([), Space, Choice(Literal(]), Sequence([cycle], Repetition(Sequence([cycle], Literal(,), [cycle], [cycle]), 0, unbounded), [cycle], Literal(]))))), [cycle], Rule(json-number, Sequence(Repetition(Literal(-), 0, 1), Choice(Literal(0), Sequence(CharRepeat([1-9], 1, 1), CharRepeat([0-9], 0, unbounded))), Repetition(Sequence(Literal(.), CharRepeat([0-9], 1, unbounded)), 0, 1), Repetition(Sequence(Choice(Literal(e), Literal(E)), Repetition(CharRepeat([+-], 1, 1), 0, 1), [cycle]), 0, 1), Not(CharRepeat([0-9.eE+-], 1, 1)))), Rule(json-bool, Choice(Literal(true), Literal(false))), Rule(json-null, Literal(null)))), {"type":"object","properties":{}})), Atomic(Tag(tool-close, Space))))), Repetition(Sequence(Literal(, ), [cycle]), 0, unbounded), Literal())), 0, 1), End))XD"}, {R"XD(Sequence(Literal(<assistant>), Space, Epsilon, Repetition(Tag(content, Epsilon), 0, 1), Repetition(Rule(tool-call, Sequence(Literal(), Choice(Rule(tool-get-weather, Sequence(Atomic(Tag(tool-open, Sequence(Literal(<function=), Atomic(Tag(tool-name, Literal(get_weather))), Literal(>)))), Epsilon, Space, Tag(tool-args, Schema(Rule(json-value, Choice(Rule(json-object, Sequence(Literal({), Space, Choice(Literal(}), Sequence(Rule(json-string, Sequence(Literal("), String("), Literal("))), [cycle], Literal(:), [cycle], [cycle], Repetition(Sequence([cycle], Literal(,), [cycle], [cycle], [cycle], [cycle], [cycle], [cycle]), 0, unbounded), [cycle], Literal(}))))), Rule(json-array, Sequence(Literal([), Space, Choice(Literal(]), Sequence([cycle], Repetition(Sequence([cycle], Literal(,), [cycle], [cycle]), 0, unbounded), [cycle], Literal(]))))), [cycle], Rule(json-number, Sequence(Repetition(Literal(-), 0, 1), Choice(Literal(0), Sequence(CharRepeat([1-9], 1, 1), CharRepeat([0-9], 0, unbounded))), Repetition(Sequence(Literal(.), CharRepeat([0-9], 1, unbounded)), 0, 1), Repetition(Sequence(Choice(Literal(e), Literal(E)), Repetition(CharRepeat([+-], 1, 1), 0, 1), [cycle]), 0, 1), Not(CharRepeat([0-9.eE+-], 1, 1)))), Rule(json-bool, Choice(Literal(true), Literal(false))), Rule(json-null, Literal(null)))), {"type":"object","properties":{"arg1":{"type":"string"},"arg2":{"type":"number"}},"required":["arg1"]})), Atomic(Tag(tool-close, Space)))), Rule(tool-get-time, Sequence(Atomic(Tag(tool-open, Sequence(Literal(<function=), Atomic(Tag(tool-name, Literal(get_time))), Literal(>)))), Epsilon, Space, Tag(tool-args, Schema(Rule(json-value, Choice(Rule(json-object, Sequence(Literal({), Space, Choice(Literal(}), Sequence(Rule(json-string, Sequence(Literal("), String("), Literal("))), [cycle], Literal(:), [cycle], [cycle], Repetition(Sequence([cycle], Literal(,), [cycle], [cycle], [cycle], [cycle], [cycle], [cycle]), 0, unbounded), [cycle], Literal(}))))), Rule(json-array, Sequence(Literal([), Space, Choice(Literal(]), Sequence([cycle], Repetition(Sequence([cycle], Literal(,), [cycle], [cycle]), 0, unbounded), [cycle], Literal(]))))), [cycle], Rule(json-number, Sequence(Repetition(Literal(-), 0, 1), Choice(Literal(0), Sequence(CharRepeat([1-9], 1, 1), CharRepeat([0-9], 0, unbounded))), Repetition(Sequence(Literal(.), CharRepeat([0-9], 1, unbounded)), 0, 1), Repetition(Sequence(Choice(Literal(e), Literal(E)), Repetition(CharRepeat([+-], 1, 1), 0, 1), [cycle]), 0, 1), Not(CharRepeat([0-9.eE+-], 1, 1)))), Rule(json-bool, Choice(Literal(true), Literal(false))), Rule(json-null, Literal(null)))), {"type":"object","properties":{}})), Atomic(Tag(tool-close, Space))))), Literal())), 0, 1), End))XD"}, {R"XD(Sequence(Literal(<assistant>), Space, Epsilon, Repetition(Tag(content, Epsilon), 0, 1), Rule(tool-call, Sequence(Literal(), Choice(Rule(tool-get-weather, Sequence(Atomic(Tag(tool-open, Sequence(Literal(<function=), Atomic(Tag(tool-name, Literal(get_weather))), Literal(>)))), Epsilon, Space, Tag(tool-args, Schema(Rule(json-value, Choice(Rule(json-object, Sequence(Literal({), Space, Choice(Literal(}), Sequence(Rule(json-string, Sequence(Literal("), String("), Literal("))), [cycle], Literal(:), [cycle], [cycle], Repetition(Sequence([cycle], Literal(,), [cycle], [cycle], [cycle], [cycle], [cycle], [cycle]), 0, unbounded), [cycle], Literal(}))))), Rule(json-array, Sequence(Literal([), Space, Choice(Literal(]), Sequence([cycle], Repetition(Sequence([cycle], Literal(,), [cycle], [cycle]), 0, unbounded), [cycle], Literal(]))))), [cycle], Rule(json-number, Sequence(Repetition(Literal(-), 0, 1), Choice(Literal(0), Sequence(CharRepeat([1-9], 1, 1), CharRepeat([0-9], 0, unbounded))), Repetition(Sequence(Literal(.), CharRepeat([0-9], 1, unbounded)), 0, 1), Repetition(Sequence(Choice(Literal(e), Literal(E)), Repetition(CharRepeat([+-], 1, 1), 0, 1), [cycle]), 0, 1), Not(CharRepeat([0-9.eE+-], 1, 1)))), Rule(json-bool, Choice(Literal(true), Literal(false))), Rule(json-null, Literal(null)))), {"type":"object","properties":{"arg1":{"type":"string"},"arg2":{"type":"number"}},"required":["arg1"]})), Atomic(Tag(tool-close, Space)))), Rule(tool-get-time, Sequence(Atomic(Tag(tool-open, Sequence(Literal(<function=), Atomic(Tag(tool-name, Literal(get_time))), Literal(>)))), Epsilon, Space, Tag(tool-args, Schema(Rule(json-value, Choice(Rule(json-object, Sequence(Literal({), Space, Choice(Literal(}), Sequence(Rule(json-string, Sequence(Literal("), String("), Literal("))), [cycle], Literal(:), [cycle], [cycle], Repetition(Sequence([cycle], Literal(,), [cycle], [cycle], [cycle], [cycle], [cycle], [cycle]), 0, unbounded), [cycle], Literal(}))))), Rule(json-array, Sequence(Literal([), Space, Choice(Literal(]), Sequence([cycle], Repetition(Sequence([cycle], Literal(,), [cycle], [cycle]), 0, unbounded), [cycle], Literal(]))))), [cycle], Rule(json-number, Sequence(Repetition(Literal(-), 0, 1), Choice(Literal(0), Sequence(CharRepeat([1-9], 1, 1), CharRepeat([0-9], 0, unbounded))), Repetition(Sequence(Literal(.), CharRepeat([0-9], 1, unbounded)), 0, 1), Repetition(Sequence(Choice(Literal(e), Literal(E)), Repetition(CharRepeat([+-], 1, 1), 0, 1), [cycle]), 0, 1), Not(CharRepeat([0-9.eE+-], 1, 1)))), Rule(json-bool, Choice(Literal(true), Literal(false))), Rule(json-null, Literal(null)))), {"type":"object","properties":{}})), Atomic(Tag(tool-close, Space))))), Repetition(Sequence(Literal(, ), [cycle]), 0, unbounded), Literal())), End))XD"}, {R"XD(Sequence(Literal(<assistant>), Space, Epsilon, Repetition(Tag(content, Epsilon), 0, 1), Rule(tool-call, Sequence(Literal(), Choice(Rule(tool-get-weather, Sequence(Atomic(Tag(tool-open, Sequence(Literal(<function=), Atomic(Tag(tool-name, Literal(get_weather))), Literal(>)))), Epsilon, Space, Tag(tool-args, Schema(Rule(json-value, Choice(Rule(json-object, Sequence(Literal({), Space, Choice(Literal(}), Sequence(Rule(json-string, Sequence(Literal("), String("), Literal("))), [cycle], Literal(:), [cycle], [cycle], Repetition(Sequence([cycle], Literal(,), [cycle], [cycle], [cycle], [cycle], [cycle], [cycle]), 0, unbounded), [cycle], Literal(}))))), Rule(json-array, Sequence(Literal([), Space, Choice(Literal(]), Sequence([cycle], Repetition(Sequence([cycle], Literal(,), [cycle], [cycle]), 0, unbounded), [cycle], Literal(]))))), [cycle], Rule(json-number, Sequence(Repetition(Literal(-), 0, 1), Choice(Literal(0), Sequence(CharRepeat([1-9], 1, 1), CharRepeat([0-9], 0, unbounded))), Repetition(Sequence(Literal(.), CharRepeat([0-9], 1, unbounded)), 0, 1), Repetition(Sequence(Choice(Literal(e), Literal(E)), Repetition(CharRepeat([+-], 1, 1), 0, 1), [cycle]), 0, 1), Not(CharRepeat([0-9.eE+-], 1, 1)))), Rule(json-bool, Choice(Literal(true), Literal(false))), Rule(json-null, Literal(null)))), {"type":"object","properties":{"arg1":{"type":"string"},"arg2":{"type":"number"}},"required":["arg1"]})), Atomic(Tag(tool-close, Space)))), Rule(tool-get-time, Sequence(Atomic(Tag(tool-open, Sequence(Literal(<function=), Atomic(Tag(tool-name, Literal(get_time))), Literal(>)))), Epsilon, Space, Tag(tool-args, Schema(Rule(json-value, Choice(Rule(json-object, Sequence(Literal({), Space, Choice(Literal(}), Sequence(Rule(json-string, Sequence(Literal("), String("), Literal("))), [cycle], Literal(:), [cycle], [cycle], Repetition(Sequence([cycle], Literal(,), [cycle], [cycle], [cycle], [cycle], [cycle], [cycle]), 0, unbounded), [cycle], Literal(}))))), Rule(json-array, Sequence(Literal([), Space, Choice(Literal(]), Sequence([cycle], Repetition(Sequence([cycle], Literal(,), [cycle], [cycle]), 0, unbounded), [cycle], Literal(]))))), [cycle], Rule(json-number, Sequence(Repetition(Literal(-), 0, 1), Choice(Literal(0), Sequence(CharRepeat([1-9], 1, 1), CharRepeat([0-9], 0, unbounded))), Repetition(Sequence(Literal(.), CharRepeat([0-9], 1, unbounded)), 0, 1), Repetition(Sequence(Choice(Literal(e), Literal(E)), Repetition(CharRepeat([+-], 1, 1), 0, 1), [cycle]), 0, 1), Not(CharRepeat([0-9.eE+-], 1, 1)))), Rule(json-bool, Choice(Literal(true), Literal(false))), Rule(json-null, Literal(null)))), {"type":"object","properties":{}})), Atomic(Tag(tool-close, Space))))), Literal())), End))XD"}, {R"XD(Sequence(Literal(<assistant>), Space, Repetition(Sequence(Epsilon, Literal(<think>), Tag(reasoning, Until(</think>)), Epsilon, Literal(</think>)), 0, 1), Repetition(Tag(content, Epsilon), 0, 1), Repetition(Rule(tool-call, Sequence(Literal(), Choice(Rule(tool-get-weather, Sequence(Atomic(Tag(tool-open, Sequence(Literal(<function=), Atomic(Tag(tool-name, Literal(get_weather))), Literal(>)))), Epsilon, Space, Tag(tool-args, Schema(Rule(json-value, Choice(Rule(json-object, Sequence(Literal({), Space, Choice(Literal(}), Sequence(Rule(json-string, Sequence(Literal("), String("), Literal("))), [cycle], Literal(:), [cycle], [cycle], Repetition(Sequence([cycle], Literal(,), [cycle], [cycle], [cycle], [cycle], [cycle], [cycle]), 0, unbounded), [cycle], Literal(}))))), Rule(json-array, Sequence(Literal([), Space, Choice(Literal(]), Sequence([cycle], Repetition(Sequence([cycle], Literal(,), [cycle], [cycle]), 0, unbounded), [cycle], Literal(]))))), [cycle], Rule(json-number, Sequence(Repetition(Literal(-), 0, 1), Choice(Literal(0), Sequence(CharRepeat([1-9], 1, 1), CharRepeat([0-9], 0, unbounded))), Repetition(Sequence(Literal(.), CharRepeat([0-9], 1, unbounded)), 0, 1), Repetition(Sequence(Choice(Literal(e), Literal(E)), Repetition(CharRepeat([+-], 1, 1), 0, 1), [cycle]), 0, 1), Not(CharRepeat([0-9.eE+-], 1, 1)))), Rule(json-bool, Choice(Literal(true), Literal(false))), Rule(json-null, Literal(null)))), {"type":"object","properties":{"arg1":{"type":"string"},"arg2":{"type":"number"}},"required":["arg1"]})), Atomic(Tag(tool-close, Space)))), Rule(tool-get-time, Sequence(Atomic(Tag(tool-open, Sequence(Literal(<function=), Atomic(Tag(tool-name, Literal(get_time))), Literal(>)))), Epsilon, Space, Tag(tool-args, Schema(Rule(json-value, Choice(Rule(json-object, Sequence(Literal({), Space, Choice(Literal(}), Sequence(Rule(json-string, Sequence(Literal("), String("), Literal("))), [cycle], Literal(:), [cycle], [cycle], Repetition(Sequence([cycle], Literal(,), [cycle], [cycle], [cycle], [cycle], [cycle], [cycle]), 0, unbounded), [cycle], Literal(}))))), Rule(json-array, Sequence(Literal([), Space, Choice(Literal(]), Sequence([cycle], Repetition(Sequence([cycle], Literal(,), [cycle], [cycle]), 0, unbounded), [cycle], Literal(]))))), [cycle], Rule(json-number, Sequence(Repetition(Literal(-), 0, 1), Choice(Literal(0), Sequence(CharRepeat([1-9], 1, 1), CharRepeat([0-9], 0, unbounded))), Repetition(Sequence(Literal(.), CharRepeat([0-9], 1, unbounded)), 0, 1), Repetition(Sequence(Choice(Literal(e), Literal(E)), Repetition(CharRepeat([+-], 1, 1), 0, 1), [cycle]), 0, 1), Not(CharRepeat([0-9.eE+-], 1, 1)))), Rule(json-bool, Choice(Literal(true), Literal(false))), Rule(json-null, Literal(null)))), {"type":"object","properties":{}})), Atomic(Tag(tool-close, Space))))), Repetition(Sequence(Literal(, ), [cycle]), 0, unbounded), Literal())), 0, 1), End))XD"}, {R"XD(Sequence(Literal(<assistant>), Space, Repetition(Sequence(Epsilon, Literal(<think>), Tag(reasoning, Until(</think>)), Epsilon, Literal(</think>)), 0, 1), Repetition(Tag(content, Epsilon), 0, 1), Repetition(Rule(tool-call, Sequence(Literal(), Choice(Rule(tool-get-weather, Sequence(Atomic(Tag(tool-open, Sequence(Literal(<function=), Atomic(Tag(tool-name, Literal(get_weather))), Literal(>)))), Epsilon, Space, Tag(tool-args, Schema(Rule(json-value, Choice(Rule(json-object, Sequence(Literal({), Space, Choice(Literal(}), Sequence(Rule(json-string, Sequence(Literal("), String("), Literal("))), [cycle], Literal(:), [cycle], [cycle], Repetition(Sequence([cycle], Literal(,), [cycle], [cycle], [cycle], [cycle], [cycle], [cycle]), 0, unbounded), [cycle], Literal(}))))), Rule(json-array, Sequence(Literal([), Space, Choice(Literal(]), Sequence([cycle], Repetition(Sequence([cycle], Literal(,), [cycle], [cycle]), 0, unbounded), [cycle], Literal(]))))), [cycle], Rule(json-number, Sequence(Repetition(Literal(-), 0, 1), Choice(Literal(0), Sequence(CharRepeat([1-9], 1, 1), CharRepeat([0-9], 0, unbounded))), Repetition(Sequence(Literal(.), CharRepeat([0-9], 1, unbounded)), 0, 1), Repetition(Sequence(Choice(Literal(e), Literal(E)), Repetition(CharRepeat([+-], 1, 1), 0, 1), [cycle]), 0, 1), Not(CharRepeat([0-9.eE+-], 1, 1)))), Rule(json-bool, Choice(Literal(true), Literal(false))), Rule(json-null, Literal(null)))), {"type":"object","properties":{"arg1":{"type":"string"},"arg2":{"type":"number"}},"required":["arg1"]})), Atomic(Tag(tool-close, Space)))), Rule(tool-get-time, Sequence(Atomic(Tag(tool-open, Sequence(Literal(<function=), Atomic(Tag(tool-name, Literal(get_time))), Literal(>)))), Epsilon, Space, Tag(tool-args, Schema(Rule(json-value, Choice(Rule(json-object, Sequence(Literal({), Space, Choice(Literal(}), Sequence(Rule(json-string, Sequence(Literal("), String("), Literal("))), [cycle], Literal(:), [cycle], [cycle], Repetition(Sequence([cycle], Literal(,), [cycle], [cycle], [cycle], [cycle], [cycle], [cycle]), 0, unbounded), [cycle], Literal(}))))), Rule(json-array, Sequence(Literal([), Space, Choice(Literal(]), Sequence([cycle], Repetition(Sequence([cycle], Literal(,), [cycle], [cycle]), 0, unbounded), [cycle], Literal(]))))), [cycle], Rule(json-number, Sequence(Repetition(Literal(-), 0, 1), Choice(Literal(0), Sequence(CharRepeat([1-9], 1, 1), CharRepeat([0-9], 0, unbounded))), Repetition(Sequence(Literal(.), CharRepeat([0-9], 1, unbounded)), 0, 1), Repetition(Sequence(Choice(Literal(e), Literal(E)), Repetition(CharRepeat([+-], 1, 1), 0, 1), [cycle]), 0, 1), Not(CharRepeat([0-9.eE+-], 1, 1)))), Rule(json-bool, Choice(Literal(true), Literal(false))), Rule(json-null, Literal(null)))), {"type":"object","properties":{}})), Atomic(Tag(tool-close, Space))))), Literal())), 0, 1), End))XD"}, {R"XD(Sequence(Literal(<assistant>), Epsilon, Tag(content, Until()), End))XD"}};
static const char * names[] = {"plain_roles_none", "reasoning_plain", "reasoning_wrapped", "tag_json_auto_parallel", "tag_json_auto_serial", "tag_json_req_parallel", "tag_json_req_serial", "tools_only_parallel", "tools_only_serial", "tag_json_content_only"};

static void check(int idx, const std::string & got) {
    ++cases;
    ++values;
    if (got != expected[idx]) {
        ++fails;
        printf("FAIL %s\n  got =[%s]\n  want=[%s]\n", names[idx], got.c_str(), expected[idx]);
    } else {
        printf("ok %s\n", names[idx]);
    }
}

int main() {
    goose_init(0, 0);

    check(0, content("assistant: ", "", "", 0));
    check(1, reasoning("<|assistant|>", "<think>", "</think>", "", "", 0));
    check(2, reasoning("<|assistant|>", "<think>", "</think>", "<response>", "</response>", 1));
    check(3, tag_json("<assistant>", 0, 1, 0));
    check(4, tag_json("<assistant>", 0, 0, 0));
    check(5, tag_json("<assistant>", 0, 1, 1));
    check(6, tag_json("<assistant>", 0, 0, 1));
    check(7, tag_json("<assistant>", 1, 1, 0));
    check(8, tag_json("<assistant>", 1, 0, 0));
    check(9, content("<assistant>", "", "", 0));

    printf("FG_DRAFT2 cases=%d values=%d failures=%d\n", cases, values, fails);
    return fails == 0 ? 0 : 1;
}
