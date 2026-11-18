// EXP977 differential gate: the Goose request parser against httplib's rules.
//
// Three comparisons:
//   1. rq_is_token / rq_is_field_valid against httplib's own validators, over a
//      corpus that includes every ASCII byte and a set of hostile names/values;
//   2. rq_parse_line against a transcription of Server::parse_request_line, which
//      is eight lines of rules and therefore transcribable exactly;
//   3. rq_parse_header on well-formed and malformed lines.
//
// No socket, no model.
#include "runtime.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace httplib {
bool oracle_is_token(const std::string & s);
bool oracle_is_field_valid(const std::string & name, const std::string & value);
bool oracle_is_request_target(const std::string & s);
}

static int checks = 0, fails = 0;
static void check(bool cond, const std::string & what) {
    checks++;
    if (!cond) { fails++; std::printf("FAIL %s\n", what.c_str()); }
}

static sl_u8 S(const void * p, long long n) { return sl_u8{ (uint8_t *) p, n }; }
static sl_u8 CS(const std::string & s) { return S(s.data(), (long long) s.size()); }
static sl_i64 I(std::vector<int64_t> & v) { return sl_i64{ v.data(), (long long) v.size() }; }

// Server::parse_request_line:9081-9098, the rule block verbatim in effect.
struct Parsed { std::string method, target, version; int vcode = 0; };
static bool native_parse_line(const std::string & line, Parsed * out) {
    if (line.size() < 2 || line[line.size() - 2] != '\r' || line[line.size() - 1] != '\n') return false;
    const size_t len = line.size() - 2;
    if (std::count(line.begin(), line.begin() + len, ' ') != 2) return false;
    const auto end = line.begin() + len;
    const auto sp1 = std::find(line.begin(), end, ' ');
    const auto sp2 = std::find(sp1 + 1, end, ' ');
    if (sp1 == line.begin() || sp2 == sp1 + 1 || sp2 + 1 == end) return false;
    out->method.assign(line.begin(), sp1);
    out->target.assign(sp1 + 1, sp2);
    out->version.assign(sp2 + 1, end);
    if (out->version != "HTTP/1.1" && out->version != "HTTP/1.0") return false;
    if (!httplib::oracle_is_request_target(out->target)) return false;
    if (out->target.find('#') != std::string::npos) return false;
    out->vcode = out->version == "HTTP/1.1" ? 2 : 1;
    return true;
}

int main() {
    goose_init(0, nullptr);

    // ---- 1. token and field-value rules, against httplib ----
    for (int c = 0; c < 256; c++) {
        std::string s(1, (char) c);
        const bool want = httplib::oracle_is_token(s);
        const bool got = rq_is_token(CS(s));
        check(got == want, "token char " + std::to_string(c));
        check(rq_is_token_char(c) == want, "token_char fn " + std::to_string(c));
    }
    const char * names[] = { "", "Host", "X-Custom", "bad name", "bad:name", "a\tb", "a~b", "a/b" };
    for (const char * n : names) {
        std::string s(n);
        check(rq_is_token(CS(s)) == httplib::oracle_is_token(s), std::string("name ") + n);
    }
    const char * values[] = { "", " ", "a", "a b", " a", "a ", "a\tb", "a\x7f", "ok" };
    for (const char * v : values) {
        std::string s(v);
        check(rq_is_field_content(CS(s)) == httplib::oracle_is_field_valid("X", s),
              std::string("value [") + v + "]");
    }

    // ---- 2. request line ----
    const char * lines[] = {
        "GET / HTTP/1.1\r\n",
        "GET /path?q=1 HTTP/1.0\r\n",
        "POST /a/b HTTP/1.1\r\n",
        "GET  /double HTTP/1.1\r\n",
        "GET / HTTP/1.1 \r\n",
        " GET / HTTP/1.1\r\n",
        "GET / HTTP/2.0\r\n",
        "GET\t/ HTTP/1.1\r\n",
        "GET /frag#x HTTP/1.1\r\n",
        "GET /\r\n",
        "",
        "GET / HTTP/1.1",           // no CRLF
        "GET / HTTP/1.1\n",         // LF only
        "GET /x y HTTP/1.1\r\n",    // three spaces
        "/ HTTP/1.1\r\n",           // no method
    };
    for (const char * raw : lines) {
        std::string line(raw);
        Parsed want;
        const bool ok_want = native_parse_line(line, &want);
        std::vector<int64_t> out(8, -1);
        const long long rc = rq_parse_line(CS(line), I(out));
        check((rc == 0) == ok_want, std::string("line verdict [") + line + "]");
        if (rc == 0 && ok_want) {
            check(line.compare((size_t) out[1], (size_t) out[2], want.method) == 0,
                  "method span");
            check(line.compare((size_t) out[3], (size_t) out[4], want.target) == 0,
                  "target span");
            check(line.compare((size_t) out[5], (size_t) out[6], want.version) == 0,
                  "version span");
            check(out[7] == want.vcode, "version code");
        }
    }

    // ---- 3. header lines ----
    {
        const char * hdrs[] = {
            "Host: example.com\r\n",
            "X-Empty: \r\n",
            "X-Spaces:   a b   \r\n",
            "NoColon\r\n",
            ": value\r\n",
            "Bad Name: v\r\n",
            "X: bad\x01value\r\n",
        };
        for (const char * raw : hdrs) {
            std::string line(raw);
            std::vector<int64_t> out(5, -1);
            const long long rc = rq_parse_header(CS(line), I(out));
            // The name/value must satisfy httplib's own field rule when accepted.
            if (rc == 0) {
                const std::string name = line.substr((size_t) out[1], (size_t) out[2]);
                const std::string value = line.substr((size_t) out[3], (size_t) out[4]);
                check(httplib::oracle_is_field_valid(name, value),
                      std::string("accepted header is valid: ") + line);
            } else {
                check(line.find(':') == std::string::npos || line.find(' ') < line.find(':') ||
                      line.find('\x01') != std::string::npos ||
                      line.rfind(':') == 0,
                      std::string("rejected header has a reason: ") + line);
            }
        }
        // A well-formed header must be accepted and trimmed.
        std::string line("X-Spaces:   a b   \r\n");
        std::vector<int64_t> out(5, -1);
        check(rq_parse_header(CS(line), I(out)) == 0, "well-formed header accepted");
        check(line.compare((size_t) out[1], (size_t) out[2], "X-Spaces") == 0, "header name");
        check(line.compare((size_t) out[3], (size_t) out[4], "a b") == 0, "header value trimmed");
    }

    std::printf("%s checks=%d failures=%d\n", fails == 0 ? "PASS" : "BAD", checks, fails);
    return fails ? 1 : 0;
}
