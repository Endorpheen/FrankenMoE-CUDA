// Differential fixture for the Goose GCP AIP and /props decisions (EXP456).
//
// Native arms are transcriptions of gcp_params::getenv, path_to_gcp_format, the
// port override in server_http_context::init, the predict-body checks in
// register_gcp_compat and get_res_props' key list. The Goose arms are the two
// emitted modules.
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include "ffi-adapter.h"

static int cases = 0, checks = 0, fails = 0;

static void fail(const char * what, const std::string & detail) {
    ++fails;
    if (fails <= 10) printf("FAIL %s: %s\n", what, detail.c_str());
}

// ---- native arms -----------------------------------------------------------
static bool native_enabled(const std::string & mode) { return mode == "PREDICTION"; }

static std::string native_route(const std::string & value, const std::string & dflt, bool has) {
    std::string v = (has && !value.empty()) ? value : dflt;
    if (!v.empty() && v[0] != '/') v.insert(v.begin(), '/');
    return v;
}

// path_to_gcp_format
static std::string native_alias(const std::string & path) {
    std::string s = path;
    if (s.size() > 3 && s[0] == '/' && s[1] == 'v' && s[2] == '1') s = s.substr(3);
    if (!s.empty() && s[0] == '/') s = s.substr(1);
    std::string result;
    bool cap = false;
    for (unsigned char c : s) {
        if (c == ':') break;
        if (c == '/' || c == '-' || c == '_') cap = true;
        else { result += (char) (cap ? std::toupper(c) : c); cap = false; }
    }
    return result;
}

static const char * PROP_KEYS[] = {
    "default_generation_settings", "total_slots", "model_alias", "model_ftype", "model_path",
    "modalities", "media_marker", "endpoint_slots", "endpoint_props", "endpoint_metrics",
    "ui", "ui_settings", "chat_template", "chat_template_caps", "bos_token", "eos_token",
    "build_info", "is_sleeping", "cors_proxy_enabled",
};

int main(void) {
    goose_init(0, 0);
    goose_init_props(0, 0);
    // 1. AIP_MODE
    const std::string modes[] = {"PREDICTION", "prediction", "PREDICTION ", "PREDICTIO",
                                 "", "PREDICTIONS", "XPREDICTION", "PREDICTIONX"};
    for (const auto & m : modes) {
        int64_t out[1] = {-1};
        fg456_mode(m.data(), (int64_t) m.size(), out);
        ++cases; ++checks;
        if ((out[0] != 0) != native_enabled(m))
            fail("mode", m);
    }

    // 2. route normalisation
    const std::string values[] = {"", "/custom", "custom", "//double", "a/b", "x"};
    const std::string defaults[] = {"", "/predict", "predict"};
    for (const auto & v : values)
    for (const auto & d : defaults)
    for (int has = 0; has < 2; ++has) {
        char out[64];
        int64_t n = fg456_route(has, v.data(), (int64_t) v.size(), d.data(), (int64_t) d.size(), out, 64);
        std::string got(out, (size_t) n);
        std::string want = native_route(v, d, has != 0);
        ++cases; ++checks;
        if (got != want) fail("route", v + " | " + d + " -> " + got + " vs " + want);
    }

    // 3. port selection and override flag
    const int64_t ports[] = {8080, 8000, 9000};
    for (int en = 0; en < 2; ++en)
    for (int64_t sp : ports)
    for (int64_t ap : ports) {
        int64_t out[2] = {-1, -1};
        fg456_port(en, sp, ap, out);
        int64_t want_port = en ? ap : sp;
        int64_t want_ovr = (en && sp != ap) ? 1 : 0;
        ++cases; ++checks;
        if (out[0] != want_port) fail("port", std::to_string(sp) + "/" + std::to_string(ap));
        ++checks;
        if (out[1] != want_ovr) fail("port-override", std::to_string(sp) + "/" + std::to_string(ap));
    }

    // 4. camelCase alias
    const std::string paths[] = {
        "/v1/chat/completions", "/chat/completions", "/health", "/v1/models", "/models",
        "/slots/:id_slot", "/v1/audio/transcriptions", "/v1/messages/count_tokens",
        "/cors-proxy", "/v1/responses/input_tokens", "/v1/stream", "/v1/streams/lookup",
        "/v1/chat/completions/control", "/v1/x", "/v1", "/", "/v1/under_score-dash",
        "/v1/Already/Camel", "/v1/100/tokens",
    };
    for (const auto & p : paths) {
        char out[128];
        int64_t n = fg456_alias(p.data(), (int64_t) p.size(), out, 128);
        std::string got(out, (size_t) n);
        std::string want = native_alias(p);
        ++cases; ++checks;
        if (got != want) fail("alias", p + " -> " + got + " vs " + want);
    }

    // 5. predict body shape
    for (int obj = 0; obj < 2; ++obj)
    for (int has = 0; has < 2; ++has)
    for (int64_t count : {0, 1, 128, 129, 1000}) {
        int64_t out[1] = {-1};
        fg456_pshape(obj, has, count, out);
        int64_t want = !obj ? 1 : (!has ? 2 : (count > 128 ? 3 : 0));
        ++cases; ++checks;
        if (out[0] != want) fail("predict-shape", std::to_string(obj) + "/" + std::to_string(has) + "/" + std::to_string(count));
    }

    // 6. instance shape
    for (int obj = 0; obj < 2; ++obj)
    for (int fmt = 0; fmt < 2; ++fmt) {
        int64_t out[1] = {-1};
        fg456_ishape(obj, fmt, out);
        int64_t want = !obj ? 4 : (!fmt ? 5 : 0);
        ++cases; ++checks;
        if (out[0] != want) fail("instance-shape", std::to_string(obj) + "/" + std::to_string(fmt));
    }

    // 7. /props key list and the conditional key
    for (int has = 0; has < 2; ++has) {
        int64_t ids[32];
        for (int i = 0; i < 32; ++i) ids[i] = -1;
        int64_t n = fg456_keys(has, ids);
        int64_t want_n = has ? 20 : 19;
        ++cases; ++checks;
        if (n != want_n) fail("props-count", std::to_string(has));
        for (int64_t i = 0; i < want_n; ++i) {
            ++checks;
            if (ids[i] != i + 1) fail("props-order", std::to_string(has) + " at " + std::to_string(i));
        }
        ++checks;
        if (n < (int64_t) (sizeof(PROP_KEYS) / sizeof(PROP_KEYS[0])) && has == 0) fail("props-native-count", "");
    }

    // 8. props mode and POST /props decision
    for (int sl = 0; sl < 2; ++sl) {
        int64_t out[1] = {-1};
        fg456_pmode(sl, out);
        int64_t want = sl ? 1 : 2;
        ++cases; ++checks;
        if (out[0] != want) fail("props-mode", std::to_string(sl));
    }
    for (int en = 0; en < 2; ++en) {
        int64_t out[1] = {-1};
        fg456_post(en, out);
        int64_t want = en ? 0 : 1;
        ++cases; ++checks;
        if (out[0] != want) fail("post-props", std::to_string(en));
    }

    printf("FG456_GCP_PROPS cases=%d values=%d failures=%d\n", cases, checks, fails);
    return fails ? 1 : 0;
}
