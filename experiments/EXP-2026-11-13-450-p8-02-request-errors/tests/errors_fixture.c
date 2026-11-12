// Differential fixture for the Goose request error policy (EXP450).
//
// Right arm: a transcription of ex_wrapper's catch chain and
// format_error_response's switch in server.cpp / server-common.cpp.
// Left arm: the Goose module emitted by the pinned FrankenGoose compiler.
#include <stdio.h>
#include <stdint.h>

int64_t fg450_classify(int64_t kind, uint64_t out);
int64_t fg450_code(int64_t type, uint64_t out);
int64_t fg450_status(int64_t has_code, int64_t payload_code, uint64_t out);

void fg450_st_i64(uint64_t p, uint64_t o, int64_t v) {
    *(int64_t *) ((char *) (uintptr_t) p + o) = v;
}

enum {
    T_INVALID_REQUEST = 0, T_AUTHENTICATION, T_SERVER, T_NOT_FOUND,
    T_PERMISSION, T_UNAVAILABLE, T_NOT_SUPPORTED, T_EXCEED_CONTEXT_SIZE,
};

// ex_wrapper: invalid_argument and common_json_error -> INVALID_REQUEST,
// every other std::exception and the catch-all -> SERVER.
enum { X_INVALID_ARGUMENT = 1, X_JSON_ERROR = 2, X_OTHER_EXCEPTION = 3, X_UNKNOWN = 4 };

static int native_classify(int kind) {
    switch (kind) {
        case X_INVALID_ARGUMENT: return T_INVALID_REQUEST;
        case X_JSON_ERROR:       return T_INVALID_REQUEST;
        default:                 return T_SERVER;
    }
}

// format_error_response
static int native_code(int t) {
    switch (t) {
        case T_INVALID_REQUEST:     return 400;
        case T_AUTHENTICATION:      return 401;
        case T_NOT_FOUND:           return 404;
        case T_SERVER:              return 500;
        case T_PERMISSION:          return 403;
        case T_NOT_SUPPORTED:       return 501;
        case T_UNAVAILABLE:         return 503;
        case T_EXCEED_CONTEXT_SIZE: return 400;
        default:                    return 500;
    }
}

static int native_status(int has_code, int payload_code) {
    return has_code ? payload_code : 500;
}

static int cases = 0, checks = 0, fails = 0;

static void compare(const char * what, int a, int b, int64_t got, int64_t want) {
    ++cases; ++checks;
    if (got != want) {
        ++fails;
        if (fails <= 10) printf("FAIL %s(%d,%d) native=%lld goose=%lld\n",
            what, a, b, (long long) want, (long long) got);
    }
}

int main(void) {
    // 1. exception classification, including classes the native catch chain
    //    does not name explicitly
    for (int kind = 0; kind <= 6; ++kind) {
        int64_t out[1] = {-99};
        fg450_classify(kind, (uint64_t) (uintptr_t) out);
        compare("classify", kind, 0, out[0], native_classify(kind));
    }

    // 2. error type -> code for every enum member plus out-of-range values
    for (int t = -2; t <= 10; ++t) {
        int64_t out[1] = {-99};
        fg450_code(t, (uint64_t) (uintptr_t) out);
        compare("code", t, 0, out[0], native_code(t));
    }

    // 3. status derivation from the formatted payload
    const int codes[] = {100, 200, 400, 401, 403, 404, 413, 415, 429, 500, 501, 503, 0, -1, 999};
    for (size_t i = 0; i < sizeof(codes) / sizeof(codes[0]); ++i)
    for (int has = 0; has < 2; ++has) {
        int64_t out[1] = {-99};
        fg450_status(has, codes[i], (uint64_t) (uintptr_t) out);
        compare("status", has, codes[i], out[0], native_status(has, codes[i]));
    }

    // 4. end-to-end chain: every exception class through format_error_response
    //    must yield the status the client sees.
    for (int kind = 0; kind <= 6; ++kind) {
        int64_t t[1] = {-99};
        fg450_classify(kind, (uint64_t) (uintptr_t) t);
        int64_t c[1] = {-99};
        fg450_code(t[0], (uint64_t) (uintptr_t) c);
        int64_t s[1] = {-99};
        fg450_status(1, c[0], (uint64_t) (uintptr_t) s);
        compare("chain", kind, 0, s[0], native_code(native_classify(kind)));
        compare("chain-type", kind, 0, t[0], native_classify(kind));
    }

    printf("FG450_REQUESTS cases=%d values=%d failures=%d\n", cases, checks, fails);
    return fails ? 1 : 0;
}
