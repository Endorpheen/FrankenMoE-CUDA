// EXP958 differential gate: the Goose HTTP response serialiser must produce the
// same bytes as cpp-httplib, which is what the shipping server uses.
//
// The oracle is httplib's own writer, compiled from the vendored file. Fill the
// buffer out of order, because that is how a header map with a custom order is
// laid down; every responder does the same.
#include "runtime.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

// The two httplib functions under test. Both are internal to httplib.cpp, so the
// gate links an object built from the same vendored source with the definitions
// exported - see the build line in the README.
namespace httplib {
const char * status_message_ref(int status);
std::string response_line_ref(int status);
std::string header_line_ref(const std::string & name, const std::string & value);
}

static int checks = 0;
static int failures = 0;
static void check(bool cond, const std::string & what) {
    checks++;
    if (!cond) { failures++; std::printf("FAIL %s\n", what.c_str()); }
}

static sl_u8 S(void * p, long long n) { return sl_u8{ (uint8_t *) p, n }; }
static sl_u8 C(const char * s) { return sl_u8{ (uint8_t *) s, (long long) std::strlen(s) }; }
static sl_u8 CS(const std::string & s) { return sl_u8{ (uint8_t *) s.data(), (long long) s.size() }; }

int main() {
    goose_init(0, nullptr);

    // ---- every status code the httplib table knows ----
    const int codes[] = {
        100, 101, 102, 103, 200, 201, 202, 203, 204, 205, 206, 207, 208, 226,
        300, 301, 302, 303, 304, 305, 306, 307, 308, 400, 401, 402, 403, 404,
        405, 406, 407, 408, 409, 410, 411, 412, 413, 414, 415, 416, 417, 418,
        421, 422, 423, 424, 425, 426, 428, 429, 431, 451, 500, 501, 502, 503,
        504, 505, 506, 507, 508, 510, 511,
    };
    for (int code : codes) {
        char buf[256];
        const long long n = rs_status_line(code, S(buf, sizeof(buf)));
        const std::string want = httplib::response_line_ref(code);
        check(n == (long long) want.size(), "status length " + std::to_string(code));
        check(n > 0 && std::memcmp(buf, want.data(), want.size()) == 0,
              "status line " + std::to_string(code));
    }

    // An unknown code keeps the trailing space and the empty phrase.
    {
        char buf[256];
        const long long n = rs_status_line(599, S(buf, sizeof(buf)));
        const std::string want = httplib::response_line_ref(599);
        check(n == (long long) want.size() && std::memcmp(buf, want.data(), want.size()) == 0,
              "unknown status 599");
        // Measured: httplib's default arm returns "Internal Server Error", so an
        // unknown code is NOT an empty phrase. The module must match that.
        check(want == "HTTP/1.1 599 Internal Server Error\r\n",
              "httplib renders an unknown code through its default arm");
    }

    // ---- header lines ----
    const char * names[] = { "Content-Type", "X-Custom", "Access-Control-Allow-Origin" };
    const char * values[] = { "application/json", "a b c", "*" };
    for (int i = 0; i < 3; i++) {
        char buf[256];
        const long long n = rs_header(C(names[i]), C(values[i]), S(buf, sizeof(buf)), 0);
        const std::string want = httplib::header_line_ref(names[i], values[i]);
        check(n == (long long) want.size(), std::string("header length ") + names[i]);
        check(n > 0 && std::memcmp(buf, want.data(), want.size()) == 0,
              std::string("header line ") + names[i]);
    }

    // ---- empty header name is refused, as httplib's field check does ----
    {
        char buf[64];
        check(rs_header(S(buf, 0), C("v"), S(buf, sizeof(buf)), 0) == -1,
              "empty header name refused");
    }

    // ---- chunk framing ----
    {
        const char * body = "hello";
        char buf[128];
        const long long n = rs_chunk(CS(std::string(body)), S(buf, sizeof(buf)), 0);
        const std::string want = "5\r\nhello\r\n";
        check(n == (long long) want.size() && std::memcmp(buf, want.data(), want.size()) == 0,
              "chunk 5 bytes");
    }
    {
        char buf[64];
        const long long n = rs_chunk_end(S(buf, sizeof(buf)), 0);
        const std::string want = "0\r\n\r\n";
        check(n == (long long) want.size() && std::memcmp(buf, want.data(), want.size()) == 0,
              "terminator");
    }
    {
        // 4096 must be lower-case hex and multi-digit.
        std::vector<uint8_t> body(4096, 'x');
        std::vector<char> buf(4200);
        const long long n = rs_chunk(S(body.data(), (long long) body.size()), S(buf.data(), (long long) buf.size()), 0);
        const std::string want = "1000\r\n" + std::string(4096, 'x') + "\r\n";
        check(n == (long long) want.size() && std::memcmp(buf.data(), want.data(), want.size()) == 0,
              "chunk 4096 is 1000 hex");
    }

    // ---- a full response, assembled the way a route assembles it ----
    {
        std::string got;
        char buf[512];
        long long n = rs_status_line(200, S(buf, sizeof(buf)));
        got.append(buf, (size_t) n);
        n = rs_header(C("Content-Type"), C("text/plain"), S(buf, sizeof(buf)), 0);
        got.append(buf, (size_t) n);
        n = rs_header(C("Transfer-Encoding"), C("chunked"), S(buf, sizeof(buf)), 0);
        got.append(buf, (size_t) n);
        n = rs_end_headers(S(buf, sizeof(buf)), 0);
        got.append(buf, (size_t) n);
        n = rs_chunk(C("hi"), S(buf, sizeof(buf)), 0);
        got.append(buf, (size_t) n);
        n = rs_chunk_end(S(buf, sizeof(buf)), 0);
        got.append(buf, (size_t) n);

        const std::string want =
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: text/plain\r\n"
            "Transfer-Encoding: chunked\r\n"
            "\r\n"
            "2\r\nhi\r\n"
            "0\r\n\r\n";
        check(got == want, "full chunked response");
    }

    // ---- a short buffer is refused, not overrun ----
    {
        char small[8];
        check(rs_status_line(200, S(small, sizeof(small))) == -(64 + 1), "short buffer refused");
    }

    std::printf("%s checks=%d failures=%d\n", failures == 0 ? "PASS" : "BAD", checks, failures);
    return failures ? 1 : 0;
}
