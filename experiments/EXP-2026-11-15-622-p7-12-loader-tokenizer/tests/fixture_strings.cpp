// Differential gate for the common string slice (S3).
//
// The oracle is the real libllama-common: string_strip, string_split,
// string_join, string_replace_all, string_repeat, string_from, string_lcs,
// string_process_escapes and string_parse_kv_override are called from
// build-cuda/bin/libllama-common.so, the same binary the rest of the native
// tree links against. The port is source/common-strings.goose.
//
// The gate runs with LC_ALL=C, the locale native uses when nothing calls
// setlocale.
#include "common-strings.h"

#include <llama.h>

#include <clocale>
#include <cstdio>
#include <cstring>
#include <string>
#include <string_view>
#include <vector>

// The native entry points, exactly as libllama-common exports them.
std::string string_strip(const std::string &);
std::vector<std::string> string_split(const std::string &, const std::string &);
std::string string_join(const std::vector<std::string> &, const std::string &);
void string_replace_all(std::string &, const std::string &, const std::string &);
std::string string_repeat(const std::string &, size_t);
std::string string_from(bool);
std::string string_from(const std::vector<int> &);
std::string string_lcs(std::string_view, std::string_view);
void string_process_escapes(std::string &);
bool string_parse_kv_override(const char *, std::vector<llama_model_kv_override> &);

extern "C" double gs_json_strtod(sl_u8 s) {
    std::string t((const char *) s.data, (size_t) s.len);
    return std::strtod(t.c_str(), nullptr);
}

namespace {

size_t g_cases = 0;
size_t g_bytes = 0;
size_t g_fail = 0;

void check(const char * fn, const std::string & label,
           const std::string & want, const char * got, size_t n) {
    g_cases += 1;
    g_bytes += want.size();
    if (want.size() != n || memcmp(want.data(), got, n) != 0) {
        g_fail += 1;
        if (g_fail <= 20) {
            printf("%s '%s'\n  want [%s] %zu bytes\n  got  [%.*s] %zu bytes\n",
                   fn, label.c_str(),
                   std::string(want).c_str(), want.size(),
                   (int) n, got, n);
        }
    }
}

void strip_cases() {
    const char * in[] = {
        "", " ", "   ", "abc", "  abc  ", "\t\nabc\r\v\f", "a b", "\t", " \t\r\n\v\f ",
        "\x01" "a\x01", "a  b", "  a", "a  ", "\x7f" "x\x7f",
    };
    for (const char * s : in) {
        std::string want = string_strip(s);
        char buf[256];
        sl_u8 i{(uint8_t *) s, (int64_t) strlen(s)};
        sl_u8 o{(uint8_t *) buf, (int64_t) sizeof(buf)};
        int64_t n = cs_strip(i, o);
        check("strip", s, want, buf, (size_t) n);
    }
}

void split_cases() {
    struct c { const char * s; const char * d; };
    const c in[] = {
        {"", ","}, {",", ","}, {",,", ","}, {"a,b", ","}, {"a,b,", ","}, {",a,b", ","},
        {"a", ","}, {"a::b::c", "::"}, {"::", "::"}, {"a:::b", "::"}, {"abc", "abcd"},
        {"a b  c", " "}, {"a\tb", "\t"}, {"xx--yy--", "--"}, {"--", "--"}, {"a,b,c,d,e", ","},
        {"a;b", ";"}, {"aaa", "aa"},
    };
    for (const c & t : in) {
        std::vector<std::string> want = string_split(t.s, t.d);

        std::string joined_expect;
        for (size_t i = 0; i < want.size(); i++) {
            if (i) {
                joined_expect.push_back('|');
            }
            joined_expect += want[i];
        }

        char out[512];
        int64_t plens[64];
        sl_u8 s{(uint8_t *) t.s, (int64_t) strlen(t.s)};
        sl_u8 d{(uint8_t *) t.d, (int64_t) strlen(t.d)};
        sl_u8 o{(uint8_t *) out, (int64_t) sizeof(out)};
        sl_i64 pl{plens, 64};
        int64_t n = cs_split(s, d, o, pl);

        // Rebuild the vector from the port and compare part by part.
        std::string got;
        int64_t off = 0;
        for (int64_t i = 0; i < n; i++) {
            if (i) {
                got.push_back('|');
            }
            got.append(out + off, (size_t) plens[i]);
            off += plens[i];
        }
        check("split", std::string(t.s) + " / " + t.d, joined_expect, got.data(), got.size());
    }
}

void join_cases() {
    struct c { std::vector<std::string> v; const char * sep; };
    const c in[] = {
        {{}, ","}, {{""}, ","}, {{"a"}, ","}, {{"a", "b"}, ","}, {{"a", "b", "c"}, "::"},
        {{"", "", ""}, "-"}, {{"a", "", "c"}, "+"},
    };
    for (const c & t : in) {
        std::string want = string_join(t.v, t.sep);

        std::string parts;
        int64_t plens[8];
        for (size_t i = 0; i < t.v.size(); i++) {
            plens[i] = (int64_t) t.v[i].size();
            parts += t.v[i];
        }
        char out[512];
        sl_u8 p{(uint8_t *) parts.data(), (int64_t) parts.size()};
        sl_i64 pl{plens, 8};
        sl_u8 s{(uint8_t *) t.sep, (int64_t) strlen(t.sep)};
        sl_u8 o{(uint8_t *) out, (int64_t) sizeof(out)};
        int64_t n = cs_join(p, pl, (int64_t) t.v.size(), s, o);
        check("join", t.sep, want, out, (size_t) n);
    }
}

void replace_cases() {
    struct c { const char * s; const char * search; const char * repl; };
    const c in[] = {
        {"", "a", "b"}, {"aaa", "aa", "X"}, {"aaa", "a", "aa"}, {"abc", "", "X"},
        {"abc", "abc", ""}, {"abcabc", "abc", "-"}, {"aaaa", "aa", "aa"},
        {"x.y.z", ".", "/"}, {"no match", "zz", "!"}, {"aaa", "a", ""},
        {"ababab", "ab", "ba"}, {"hello", "l", "LL"},
    };
    for (const c & t : in) {
        std::string want = t.s;
        string_replace_all(want, t.search, t.repl);

        char out[512];
        sl_u8 s{(uint8_t *) t.s, (int64_t) strlen(t.s)};
        sl_u8 se{(uint8_t *) t.search, (int64_t) strlen(t.search)};
        sl_u8 r{(uint8_t *) t.repl, (int64_t) strlen(t.repl)};
        sl_u8 o{(uint8_t *) out, (int64_t) sizeof(out)};
        int64_t n = cs_replace_all(s, se, r, o);
        check("replace", std::string(t.s) + " " + t.search + " -> " + t.repl,
              want, out, (size_t) n);
    }
}

void repeat_cases() {
    struct c { const char * s; int n; };
    const c in[] = {{"", 3}, {"a", 0}, {"a", 1}, {"ab", 5}, {"xy", 2}, {"", 0}};
    for (const c & t : in) {
        std::string want = string_repeat(t.s, (size_t) t.n);
        char out[256];
        sl_u8 s{(uint8_t *) t.s, (int64_t) strlen(t.s)};
        sl_u8 o{(uint8_t *) out, (int64_t) sizeof(out)};
        int64_t n = cs_repeat(s, t.n, o);
        check("repeat", std::string(t.s) + " x" + std::to_string(t.n), want, out, (size_t) n);
    }
}

void from_cases() {
    for (int v = 0; v < 2; v++) {
        std::string want = string_from(v != 0);
        char out[16];
        sl_u8 o{(uint8_t *) out, (int64_t) sizeof(out)};
        int64_t n = cs_from_bool(v, o);
        check("from_bool", std::to_string(v), want, out, (size_t) n);
    }

    const std::vector<std::vector<int>> in = {
        {}, {0}, {1}, {1, -2, 3}, {-2147483647 - 1, 2147483647}, {1000000, -1, 0},
    };
    for (const auto & v : in) {
        std::string want = string_from(v);
        int64_t vals[8];
        for (size_t i = 0; i < v.size(); i++) {
            vals[i] = v[i];
        }
        char out[256];
        sl_i64 vs{vals, (int64_t) v.size()};
        sl_u8 o{(uint8_t *) out, (int64_t) sizeof(out)};
        int64_t n = cs_from_ints(vs, o);
        check("from_ints", want, want, out, (size_t) n);
    }
}

void lcs_cases() {
    struct c { const char * a; const char * b; };
    const c in[] = {
        {"", ""}, {"", "abc"}, {"abc", ""}, {"abc", "abd"}, {"abcdef", "zabcf"},
        {"abcd", "bcde"}, {"aaaa", "aa"}, {"xyxy", "yxyx"}, {"hello", "yellow"},
        {"abcabc", "cabcx"}, {"1234567890", "5678901234"}, {"aXbXc", "bXcX"},
        {"kitten", "sitting"}, {"abcdefgh", "abcdefgh"}, {"zzz", "z"},
    };
    for (const c & t : in) {
        std::string want = string_lcs(t.a, t.b);

        size_t la = strlen(t.a), lb = strlen(t.b);
        std::vector<int64_t> dp((la + 1) * (lb + 1 + 1), -7);
        char out[256];
        sl_u8 a{(uint8_t *) t.a, (int64_t) la};
        sl_u8 b{(uint8_t *) t.b, (int64_t) lb};
        sl_u8 o{(uint8_t *) out, (int64_t) sizeof(out)};
        sl_i64 d{dp.data(), (int64_t) dp.size()};
        int64_t n = cs_lcs(a, b, o, d);
        check("lcs", std::string(t.a) + " / " + t.b, want, out, (size_t) n);
    }
}

void escape_cases() {
    const std::string in[] = {
        "", "plain", "a\\nb", "\\\\", "\\'\\\"", "\\x41", "\\x4", "\\xGG", "\\x 1",
        "\\x-1", "\\x+1", "\\x0x", "\\x4\\x1", "tail\\", "\\q", "\\x00z", "\\x7f",
        "\\n\\r\\t", "\\\\n", "a\\tb\\rc", "\\x", "x\\xy", "\\x  ", "\\x\tf",
        "\\xg1", "\\x1g", "\\X41",
    };
    for (const std::string & s : in) {
        std::string want = s;
        string_process_escapes(want);

        char out[256];
        sl_u8 i{(uint8_t *) s.data(), (int64_t) s.size()};
        sl_u8 o{(uint8_t *) out, (int64_t) sizeof(out)};
        int64_t n = cs_process_escapes(i, o);
        check("escapes", s, want, out, (size_t) n);
    }
}

void kv_cases() {
    std::string long_key(127, 'k');
    std::string too_long_key(128, 'k');
    std::string long_str(127, 'v');
    std::string too_long_str(128, 'v');

    const std::vector<std::string> in = {
        "k=int:42", "k=int:-7", "k=int:  -7", "k=int:", "k=int:abc",
        "k=int:99999999999999999999", "k=int:-99999999999999999999",
        "k=float:1.5", "k=float: 1e-3", "k=float:-0.0", "k=float:nan",
        "k=float:inf", "k=float:", "k=float:0x1p3",
        "k=bool:true", "k=bool:false", "k=bool:True", "k=bool:1", "k=bool:",
        "k=str:abc", "k=str:", "k=str:a=b", "k=str:" + long_str, "k=str:" + too_long_str,
        "noeq", "k=uint:5", "k=", "=int:1", "k=int:1=2", long_key + "=int:1",
        too_long_key + "=int:1", "k=str:true", "k=float:1e400",
    };
    for (const std::string & s : in) {
        std::vector<llama_model_kv_override> want;
        bool ok = string_parse_kv_override(s.c_str(), want);

        std::string want_rec;
        char buf[256];
        if (!ok) {
            want_rec = "reject";
        } else {
            const auto & k = want[0];
            char tmp[512];
            if (k.tag == LLAMA_KV_OVERRIDE_TYPE_INT) {
                snprintf(tmp, sizeof(tmp), "int %lld", (long long) k.val_i64);
            } else if (k.tag == LLAMA_KV_OVERRIDE_TYPE_FLOAT) {
                uint64_t bits;
                double d = k.val_f64;
                memcpy(&bits, &d, sizeof(bits));
                snprintf(tmp, sizeof(tmp), "float %016llx", (unsigned long long) bits);
            } else if (k.tag == LLAMA_KV_OVERRIDE_TYPE_BOOL) {
                snprintf(tmp, sizeof(tmp), "bool %d", k.val_bool ? 1 : 0);
            } else {
                snprintf(tmp, sizeof(tmp), "str %s", k.val_str);
            }
            want_rec = std::string(k.key) + " " + tmp;
        }

        char out_key[256];
        char out_str[256];
        int64_t res[8] = {0, 0, 0, 0, 0, 0, 0, 0};
        double out_f = 0.0;
        sl_u8 d{(uint8_t *) s.data(), (int64_t) s.size()};
        sl_u8 ok8{(uint8_t *) out_key, (int64_t) sizeof(out_key)};
        sl_u8 os{(uint8_t *) out_str, (int64_t) sizeof(out_str)};
        sl_i64 r{res, 8};
        sl_f64 of{&out_f, 1};
        int64_t st = cs_kv(d, ok8, os, r, of);

        std::string got;
        if (st != 0) {
            got = "reject";
        } else {
            char tmp[512];
            std::string key(out_key, (size_t) res[3]);
            if (res[0] == 0) {
                snprintf(tmp, sizeof(tmp), "int %lld", (long long) res[1]);
            } else if (res[0] == 1) {
                uint64_t bits;
                memcpy(&bits, &out_f, sizeof(bits));
                snprintf(tmp, sizeof(tmp), "float %016llx", (unsigned long long) bits);
            } else if (res[0] == 2) {
                snprintf(tmp, sizeof(tmp), "bool %d", (int) res[2]);
            } else {
                snprintf(tmp, sizeof(tmp), "str %s", std::string(out_str, (size_t) res[4]).c_str());
            }
            got = key + " " + tmp;
        }
        check("kv", s.substr(0, 40), want_rec, got.data(), got.size());
    }
}

} // namespace

int main() {
    setlocale(LC_ALL, "C");
    goose_init(0, nullptr);

    strip_cases();
    split_cases();
    join_cases();
    replace_cases();
    repeat_cases();
    from_cases();
    lcs_cases();
    escape_cases();
    kv_cases();

    printf("cases %zu, output bytes %zu, failures %zu\n", g_cases, g_bytes, g_fail);
    return g_fail == 0 ? 0 : 1;
}
