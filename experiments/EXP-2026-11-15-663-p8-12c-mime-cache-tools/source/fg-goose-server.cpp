// Bridge between the five server call sites and the composed Goose unit.
//
// The unit is one translation unit with one goose_init. Every wrapper below is
// a slice adapter: it hands the unit an (uint8_t*, int64_t) out buffer, and the
// unit returns the number of bytes it wrote. A negative return means "no Goose
// answer", and then the call site keeps its native result.
//
// With FG_GOOSE_SERVER_RUNTIME off the unit does not exist in the link, so the
// wrappers are compiled to the constant -1 and no unit symbol is referenced.
// That is what makes the nm check 1/0: one definition in ON, none in OFF.

#include "fg-goose-server.h"

#include <mutex>
#include <stdint.h>

#if FG_GOOSE_SERVER_RUNTIME

extern "C" {

typedef struct {
    uint8_t * data;
    int64_t   len;
} fgs_slice;

// The composed unit's own exports (server.h), declared here with the same
// (data, len) layout so the bridge does not pull the generated header, which
// declares all 47 exports and every generated type, into C++ call sites.
int64_t mg_status_str  (int64_t status, fgs_slice out);
int64_t tool_index     (fgs_slice name);
int64_t mime_of        (fgs_slice name);
int64_t mime_text      (int64_t id, fgs_slice out);
int64_t web_cache_text (int64_t policy, fgs_slice out);
int64_t cors_is_localhost(fgs_slice host, int64_t parsed);

void goose_init(int argc, char ** argv);

} // extern "C"

#endif // FG_GOOSE_SERVER_RUNTIME

namespace {

#if FG_GOOSE_SERVER_RUNTIME
// One start per process. goose_init is idempotent on its side too, but the flag
// also makes the arguments of the first call the ones the unit keeps, and it
// keeps concurrent wrappers from racing into a half-built globals instance.
void goose_start(int argc, char ** argv) {
    static std::once_flag flag;
    std::call_once(flag, [argc, argv] { goose_init(argc, argv); });
}
#endif

#if FG_GOOSE_SERVER_RUNTIME
fgs_slice out_slice(char * out, int64_t cap, bool & ok) {
    ok = out != nullptr && cap > 0;
    fgs_slice s { reinterpret_cast<uint8_t *>(out), ok ? cap : 0 };
    return s;
}

int64_t text_result(int64_t n, int64_t cap) {
    return (n >= 0 && n <= cap) ? n : -1;
}
#endif

} // namespace

extern "C" void fgs_init_early(int argc, char ** argv) {
#if FG_GOOSE_SERVER_RUNTIME
    goose_start(argc, argv);
#else
    (void) argc; (void) argv;
#endif
}

extern "C" int64_t fgs_model_status_text(int64_t status, char * out, int64_t cap) {
#if FG_GOOSE_SERVER_RUNTIME
    goose_start(0, nullptr);
    bool ok = false;
    const fgs_slice s = out_slice(out, cap, ok);
    if (!ok) {
        return -1;
    }
    return text_result(mg_status_str(status, s), cap);
#else
    (void) status; (void) out; (void) cap;
    return -1;
#endif
}

extern "C" int64_t fgs_tool_index(const char * name, int64_t len) {
#if FG_GOOSE_SERVER_RUNTIME
    goose_start(0, nullptr);
    if (name == nullptr || len < 0) {
        return -1;
    }
    fgs_slice s { reinterpret_cast<uint8_t *>(const_cast<char *>(name)), len };
    return tool_index(s);
#else
    (void) name; (void) len;
    return -1;
#endif
}

extern "C" int64_t fgs_mime_text(const char * name, int64_t len, char * out, int64_t cap) {
#if FG_GOOSE_SERVER_RUNTIME
    goose_start(0, nullptr);
    if (name == nullptr || len < 0) {
        return -1;
    }
    fgs_slice in { reinterpret_cast<uint8_t *>(const_cast<char *>(name)), len };
    const int64_t id = mime_of(in);
    if (id < 0) {
        return -1;
    }
    bool ok = false;
    const fgs_slice s = out_slice(out, cap, ok);
    if (!ok) {
        return -1;
    }
    return text_result(mime_text(id, s), cap);
#else
    (void) name; (void) len; (void) out; (void) cap;
    return -1;
#endif
}

extern "C" int64_t fgs_cache_text(int64_t policy, char * out, int64_t cap) {
#if FG_GOOSE_SERVER_RUNTIME
    goose_start(0, nullptr);
    bool ok = false;
    const fgs_slice s = out_slice(out, cap, ok);
    if (!ok) {
        return -1;
    }
    return text_result(web_cache_text(policy, s), cap);
#else
    (void) policy; (void) out; (void) cap;
    return -1;
#endif
}

extern "C" int64_t fgs_cors_localhost(const char * host, int64_t len, int64_t parsed) {
#if FG_GOOSE_SERVER_RUNTIME
    goose_start(0, nullptr);
    if (host == nullptr && len > 0) {
        return -1;
    }
    fgs_slice s { reinterpret_cast<uint8_t *>(const_cast<char *>(host)), len };
    return cors_is_localhost(s, parsed);
#else
    (void) host; (void) len; (void) parsed;
    return -1;
#endif
}
