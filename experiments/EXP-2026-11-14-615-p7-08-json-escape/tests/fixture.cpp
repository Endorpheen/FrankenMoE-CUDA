// Gate for the runtime chat-JSON escaper: chat_content_json on one text part
// returns a quoted JSON string, compared with nlohmann's dump.
#include "g.h"
#include <nlohmann/json.hpp>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>

extern "C" double gs_json_strtod(sl_u8 s) {
    std::string t((const char *) s.data, (size_t) s.len);
    return std::strtod(t.c_str(), nullptr);
}
extern "C" int64_t gs_json_fmt_e(sl_u8 out, int64_t prec, double x) {
    return snprintf((char *) out.data, (size_t) out.len, "%.*e", (int) prec, x);
}
extern "C" int64_t gs_json_fmt_g(sl_u8 out, double x) {
    return snprintf((char *) out.data, (size_t) out.len, "%.17g", x);
}
extern "C" double gs_frexp_mant(double x) { int e = 0; return std::frexp(x, &e); }
extern "C" int64_t gs_strftime_epoch(sl_u8 out, int64_t epoch, sl_u8 fmt) {
    std::string f((const char *) fmt.data, (size_t) fmt.len);
    time_t t = epoch < 0 ? time(nullptr) : (time_t) epoch;
    const tm * v = std::localtime(&t);
    return v ? (int64_t) std::strftime((char *) out.data, (size_t) out.len, f.c_str(), v) : -1;
}
extern "C" int64_t gs_to_string_f64(sl_u8 out, double x) {
    std::string t = std::to_string(x);
    memcpy(out.data, t.data(), t.size());
    return (int64_t) t.size();
}

static int cases = 0, fails = 0;

static std::string content_json(const std::string & s) {
    std::string blob = "text" + s;
    int64_t to[1] = {0}, tl[1] = {4}, xo[1] = {4}, xl[1] = {(int64_t) s.size()};
    char out[4096] = {0};
    int64_t n = chat_content_json(
        sl_u8{(uint8_t *) (void *) blob.data(), (int64_t) blob.size()},
        sl_i64{to, 1}, sl_i64{tl, 1}, sl_i64{xo, 1}, sl_i64{xl, 1}, 1,
        sl_u8{(uint8_t *) out, sizeof out});
    return std::string(out, (size_t) n);
}

static void check(const char * name, const std::string & s) {
    std::string want = "[{\"type\": \"text\", \"text\": " + nlohmann::json(s).dump() + "}]";
    std::string got = content_json(s);
    cases++;
    if (want == got) { printf("OK   %s\n", name); return; }
    fails++;
    printf("FAIL %s\n  want: %s\n  got : %s\n", name, want.c_str(), got.c_str());
}

int main() {
    goose_init(0, nullptr);
    check("empty", "");
    check("plain", "hello");
    check("quote", "a\"b");
    check("backslash", "a\\b");
    check("newline", "a\nb");
    check("cr", "a\rb");
    check("tab", "a\tb");
    check("backspace", "a\bb");
    check("formfeed", "a\fb");
    check("ctrl01", std::string("a") + char(1) + "b");
    check("ctrl1f", std::string("a") + char(0x1f) + "b");
    check("utf8", "caf\xc3\xa9");
    check("emoji", "\xf0\x9f\x98\x80");
    check("mixed", "a\"b\\c\nd\x08\xc3\xa9");
    printf("FG_ESC cases=%d failures=%d\n", cases, fails);
    return fails ? 1 : 0;
}
