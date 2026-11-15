// Environment switches of the P8-12 server sites; see fg-goose-switches.h.

#include "fg-goose-switches.h"

#include <stddef.h>
#include <stdlib.h>

namespace {

// Case-insensitive compare of a value against a lowercase literal.
bool env_ieq(const char * a, const char * b) {
    for (size_t i = 0;; ++i) {
        char c = a[i];
        if (c >= 'A' && c <= 'Z') {
            c = (char) (c + ('a' - 'A'));
        }
        if (c != b[i]) {
            return false;
        }
        if (c == '\0') {
            return true;
        }
    }
}

// true when the variable is set to something other than the off spellings.
bool env_truthy(const char * name) {
    const char * v = getenv(name);
    if (v == nullptr || v[0] == '\0') {
        return false;
    }
    if (v[0] == '0' && v[1] == '\0') {
        return false;
    }
    if (env_ieq(v, "false") || env_ieq(v, "no") || env_ieq(v, "off")) {
        return false;
    }
    return true;
}

} // namespace

extern "C" int fg_goose_cors_native(void) {
    static const int native = env_truthy(FG_GOOSE_ENV_CORS_NATIVE) ? 1 : 0;
    return native;
}

extern "C" int fg_goose_tools_native(void) {
    static const int native = env_truthy(FG_GOOSE_ENV_TOOLS_NATIVE) ? 1 : 0;
    return native;
}
