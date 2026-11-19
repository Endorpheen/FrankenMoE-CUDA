// Gate for the origin policy port: web_origin_is_localhost against the native decision.
//
// Oracle: the native body of origin_is_localhost in tools/server/server-http.cpp, transcribed here
// three lines long and using the *same* parser the production code uses (common_http_parse_url from
// common/http.h). A transcription of the parser would not be an oracle; calling the parser itself is.
//
// The port is web_origin_is_localhost of goose/server/prerouting.goose. Every case is compared, and a
// disagreement is printed with the input, so the gate names what differs instead of failing quietly.
//
// Why this matters beyond the port: the Goose host extraction accepts an authority without a scheme,
// while common_http_parse_url throws without one. If that difference is real, it must show up here
// before anything calls the door, because the caller is the CORS path of a live server.
#include "../../../vendor/flint/common/http.h"
#include "runtime-gen.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

static bool native_origin_is_localhost(const std::string & origin) {
    try {
        const std::string host = common_http_parse_url(origin).host;
        return host == "localhost" || host == "127.0.0.1" || host == "::1";
    } catch (const std::exception &) {
        return false;
    }
}

// Host primitives the module declares. preroute() writes its answer through st_i64; this gate never
// calls preroute, but the generated code links as one unit, so the symbols must exist. They are the
// obvious byte and word accessors, not a second implementation of anything under test.
// The signatures are the module's own (prerouting.goose:18-19), not guessed: ld_u8 takes the base and
// the offset. The first version of this fixture declared ld_u8 with one argument, so the offset was
// ignored and every comparison read byte 0 - the gate then reported eight differences that were all
// its own. A gate whose harness is wrong reports the harness, which is why the case list prints the
// input next to both answers.
extern "C" void fg446_st_i64(uint64_t p, uint64_t o, int64_t v) { *(int64_t *) (uintptr_t) (p + o) = v; }
extern "C" uint32_t fg446_ld_u8(uint64_t p, uint64_t o) { return *(const uint8_t *) (uintptr_t) (p + o); }

int main() {
    goose_init(0, nullptr);

    const std::vector<std::string> cases = {
        "http://localhost:8080",
        "https://localhost",
        "http://127.0.0.1",
        "http://127.0.0.1:9101",
        "http://[::1]:3000",
        "http://[::1]",
        "http://example.com",
        "https://example.com:443",
        "http://127.0.0.2",
        "http://127.0.0.1.example.com",
        "http://user:pass@localhost:1",
        "http://user@127.0.0.1",
        "http://localhost.example.com",
        "ftp://localhost",
        "localhost:8080",
        "127.0.0.1:9101",
        "",
        "://localhost",
        "http://",
        "http://LOCALHOST",
        "http://localhost:8080/path?x=1",
        "https://user:pass@[::1]:443/x",
        "http://127.0.0.1:0",
        "http://[::1]extra",
        "http:/localhost",
        "http://localhost.",
        "http://127.0.0.1.",
        "https://",
        "http://[127.0.0.1]",
    };

    int compared = 0, agree = 0, differ = 0, port_true = 0;
    for (const auto & c : cases) {
        const bool want = native_origin_is_localhost(c);
        const int64_t got = web_origin_is_localhost((uint64_t) (uintptr_t) c.data(), (int64_t) c.size());
        compared++;
        if (want) port_true++;
        if ((got != 0) == want) {
            agree++;
        } else {
            differ++;
            std::printf("DIFFER  %-32s native=%d port=%lld\n", c.c_str(), (int) want, (long long) got);
        }
    }

    std::printf("origin policy: %d compared, %d agree, %d differ (native true on %d cases)\n",
                compared, agree, differ, port_true);
    if (differ != 0) {
        std::printf("A difference is not automatically a defect: an input the native path rejects because the\n"
                    "parser throws, and the port accepts, is a behaviour change and must be decided, not\n"
                    "wired. Nothing calls the door until that decision is recorded.\n");
        return 1;
    }
    return 0;
}
