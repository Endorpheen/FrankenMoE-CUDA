// The two promoted server call sites of P8-12, as one function each.
//
// Both decisions live here and not in the call site, because the differential
// gate (gate/gate.cpp) includes this same header: what the gate measures is the
// code the server runs, not a copy of it.
//
// The composition of each function is exactly the native one, split at the
// point the Goose module owns:
//
//   status      the module owns the whole body. The 7-case native switch was
//               deleted after the differential gate went green, so this site has
//               no native arm and no runtime switch: model-manager.goose is the
//               vocabulary, and a build without the unit answers "unknown";
//   Origin host the module owns the host test only, because cors-policy.goose
//               never parses a URL: it takes the parsed host and a "parsed"
//               flag, and the native parser with its exception arm stays here.
//               This site keeps its native arm behind the runtime switch.

#pragma once

#include "fg-goose-server.h"
#include "fg-goose-switches.h"
#include "http.h"

#include <stdint.h>

#include <exception>
#include <string>

// server-models.h:55 server_model_status_to_string. status is one of
// SERVER_MODEL_STATUS_* (0..6; server-models.h asserts the numbering).
static inline std::string fg_goose_model_status_text(int64_t status) {
    char buf[32];
    const int64_t n = fgs_model_status_text(status, buf, (int64_t) sizeof(buf));
    if (n >= 0) {
        return std::string(buf, (size_t) n);
    }

    // The native switch that stood here is gone. Only a build without the unit
    // reaches this arm, and it has no vocabulary left to answer with; the gate
    // pins that arm to exactly this one value.
    return "unknown";
}

// server-http.cpp:77 origin_is_localhost: true when the Origin header's host is
// localhost / 127.0.0.1 / ::1 (any port, any scheme). The parse is the native
// one in both arms, so a parse failure is the module's parsed = 0 arm and the
// native false.
static inline bool fg_goose_origin_is_localhost(const std::string & origin) {
    if (!fg_goose_cors_native()) {
        try {
            const std::string host = common_http_parse_url(origin).host;
            const int64_t r = fgs_cors_localhost(host.data(), (int64_t) host.size(), 1);
            if (r >= 0) {
                return r != 0;
            }
        } catch (const std::exception &) {
            const int64_t r = fgs_cors_localhost(nullptr, 0, 0);
            if (r >= 0) {
                return r != 0;
            }
        }
    }

    // native body, kept: the switch can still select it
    try {
        const std::string host = common_http_parse_url(origin).host;
        return host == "localhost" || host == "127.0.0.1" || host == "::1";
    } catch (const std::exception &) {
        return false;
    }
}
