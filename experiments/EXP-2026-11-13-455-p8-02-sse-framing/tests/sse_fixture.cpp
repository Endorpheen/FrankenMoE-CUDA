// Differential fixture for the Goose SSE framing plan (EXP455).
//
// Right arm: the native formatters transcribed from server-common.cpp and
// server-context.cpp, building the actual strings.
// Left arm: the Goose step plan expanded with the same literals, plus the Goose
// length accounting. The fixture checks that the plan produces the native bytes
// and that frame_size equals the real length.
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

extern "C" {
int64_t fg455_frame(int64_t flavor, int64_t has_event, int64_t * out);
int64_t fg455_size(const int64_t * steps, int64_t n, int64_t payload, int64_t event, uint64_t out);
void    fg455_st_i64(uint64_t p, uint64_t o, int64_t v);
}

void fg455_st_i64(uint64_t p, uint64_t o, int64_t v) {
    *(int64_t *) ((char *) (uintptr_t) p + o) = v;
}

enum { F_OAI_CHAT = 1, F_OAI_RESP = 2, F_ANTHROPIC = 3, F_DONE = 4, F_PING = 5 };

// format_oai_sse / format_oai_resp_sse / format_anthropic_sse / terminator / ping
static std::string native_frame(int flavor, bool has_event, const std::string & payload,
                                const std::string & event) {
    switch (flavor) {
        case F_OAI_CHAT:    return "data: " + payload + "\n\n";
        case F_OAI_RESP:    return "event: " + event + "\n" + "data: " + payload + "\n\n";
        case F_ANTHROPIC:
            if (has_event) return "event: " + event + "\n" + "data: " + payload + "\n\n";
            return "data: " + payload + "\n\n";
        case F_DONE:        return "data: [DONE]\n\n";
        case F_PING:        return ":\n\n";
        default:            return "";
    }
}

static std::string expand(const int64_t * steps, int64_t n, const std::string & payload,
                          const std::string & event) {
    std::string out;
    for (int64_t i = 0; i < n; ++i) {
        switch (steps[i]) {
            case 1: out += "data: "; break;
            case 2: out += payload; break;
            case 3: out += "event: "; break;
            case 4: out += event; break;
            case 5: out += "\n"; break;
            case 7: out += "[DONE]"; break;
            case 8: out += ":"; break;
            default: out += "<?>"; break;
        }
    }
    return out;
}

static int cases = 0, checks = 0, fails = 0;

static void fail(const char * what, int flavor, bool has_event, size_t pn, size_t en) {
    ++fails;
    if (fails <= 10) printf("FAIL %s flavor=%d event=%d payload=%zu evname=%zu\n",
                            what, flavor, has_event, pn, en);
}

int main(void) {
    const std::string payloads[] = {"", "{}", "x", "{\"id\":1}", std::string(50, 'a')};
    const std::string events[] = {"", "message", "content_block_delta"};
    const int item_counts[] = {0, 1, 3};

    for (int flavor = 1; flavor <= 5; ++flavor)
    for (int he = 0; he < 2; ++he)
    for (const auto & payload : payloads)
    for (const auto & event : events)
    for (int ic = 0; ic < 3; ++ic) {
        const int items = item_counts[ic];
        // array form: the server emits one frame per item, or nothing at all
        std::string want;
        for (int k = 0; k < items; ++k) want += native_frame(flavor, he != 0, payload, event);

        std::string got;
        int64_t total = 0;
        for (int k = 0; k < items; ++k) {
            int64_t plan[16];
            for (int i = 0; i < 16; ++i) plan[i] = -1;
            int64_t n = fg455_frame(flavor, he, plan);
            ++cases;
            ++checks;
            if (n <= 0 || plan[0] != n) { fail("plan-count", flavor, he != 0, payload.size(), event.size()); break; }
            got += expand(plan + 1, n, payload, event);
            int64_t sz[1] = {-1};
            fg455_size(plan + 1, n, (int64_t) payload.size(), (int64_t) event.size(),
                       (uint64_t) (uintptr_t) sz);
            ++cases;
            ++checks;
            if (sz[0] != (int64_t) want.size() / (items ? items : 1)) {
                fail("plan-size", flavor, he != 0, payload.size(), event.size());
            }
            total += sz[0];
        }

        ++cases;
        ++checks;
        if (got != want) fail("plan-bytes", flavor, he != 0, payload.size(), event.size());
        ++checks;
        if (items > 0 && total != (int64_t) want.size()) fail("total-size", flavor, he != 0,
                                                              payload.size(), event.size());
        ++checks;
        if (items == 0 && !want.empty()) fail("empty-array", flavor, he != 0, payload.size(), event.size());
    }

    // the terminator is used only by the chat family; the other two response
    // types end with an empty string, which is why the [DONE] plan must exist
    // as its own flavor rather than being folded into flavor 1
    {
        int64_t plan[16];
        int64_t n = fg455_frame(F_DONE, 0, plan);
        ++cases; ++checks;
        if (n != 4 || plan[1] != 1 || plan[2] != 7 || plan[3] != 5 || plan[4] != 5)
            fail("done-plan-shape", F_DONE, false, 0, 0);
        ++cases; ++checks;
        if (expand(plan + 1, n, "", "") != "data: [DONE]\n\n") fail("done-bytes", F_DONE, false, 0, 0);
    }

    printf("FG455_SSE cases=%d values=%d failures=%d\n", cases, checks, fails);
    return fails ? 1 : 0;
}
