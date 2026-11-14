// Differential gate for the content-only generator draft.
// Builds an arena with autoparser_build_content() and compares the textual dump
// with the dump printed by the native peg_generator probe (probe.out).
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <ctime>
#include <cmath>
#include <cstdlib>

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

static int cases = 0;
static int values = 0;
static int fails = 0;

static sl_u8 slv(std::vector<uint8_t> & v) { return sl_u8{v.data(), (int64_t) v.size()}; }
static sl_i64 sli(std::vector<int64_t> & v) { return sl_i64{v.data(), (int64_t) v.size()}; }

static std::string run(
    const std::string & gp,
    const std::string & cstart,
    const std::string & cend,
    int64_t wrapped) {
    const size_t NODES = 128;
    const size_t STRS  = 128;

    std::vector<int64_t> kind(NODES, 0), cfirst(NODES, 0), ccount(NODES, 0);
    std::vector<int64_t> children(1024, 0), sidx(NODES, 0);
    std::vector<int64_t> dstart(STRS, 0), dcount(STRS, 0), mn(NODES, 0), mx(NODES, 0);
    std::vector<uint8_t> pool(1 << 16, 0);
    std::vector<int64_t> st(128, 0);
    std::vector<int64_t> vis(256, 0), vis2(256, 0);
    std::vector<uint8_t> out(1 << 16, 0);

    std::vector<uint8_t> gpb(gp.begin(), gp.end());
    std::vector<uint8_t> csb(cstart.begin(), cstart.end());
    std::vector<uint8_t> ceb(cend.begin(), cend.end());

    int64_t root = autoparser_build_content(
        slv(gpb), slv(csb), slv(ceb), wrapped,
        sli(kind), sli(cfirst), sli(ccount), sli(children), sli(sidx),
        sli(dstart), sli(dcount), sli(mn), sli(mx), slv(pool), sli(st));

    int64_t len = peg_dump_basic(root, sli(vis), sli(vis2),
        sli(kind), sli(cfirst), sli(ccount), sli(children), sli(sidx),
        sli(dstart), sli(dcount), sli(mn), sli(mx), slv(pool), slv(out), sli(st));

    return std::string((const char *) out.data(), (size_t) len);
}

static void check(const char * name, const std::string & got, const std::string & want) {
    ++cases;
    ++values;
    if (got != want) {
        ++fails;
        printf("FAIL %s\n  got =[%s]\n  want=[%s]\n", name, got.c_str(), want.c_str());
    } else {
        printf("ok %s: %s\n", name, got.c_str());
    }
}

int main() {
    goose_init(0, 0);

    check("plain_roles_none",
          run("assistant: ", "", "", 0),
          "Sequence(Literal(assistant: ), Epsilon, Tag(content, Until()), End)");

    check("wrapped_content_none",
          run("<assistant>", "<response>", "</response>", 1),
          "Sequence(Literal(<assistant>), Tag(content, Until(<response>)), Literal(<response>), "
          "Tag(content, Until(</response>)), Literal(</response>), End)");

    check("tag_content_only",
          run("<assistant>", "", "", 0),
          "Sequence(Literal(<assistant>), Epsilon, Tag(content, Until()), End)");

    printf("FG_DRAFT cases=%d values=%d failures=%d\n", cases, values, fails);
    return fails == 0 ? 0 : 1;
}
