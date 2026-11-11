// Gate: the Goose request-field decisions against a direct transcription of the server's handling.
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <nlohmann/json.hpp>
using json = nlohmann::json;
extern "C" void fg251s_decide(int64_t req, int64_t out[4]);

struct Ref { int64_t n_predict; int64_t top_k; float top_p; int present; };
static Ref ref_decide(const json & j) {
    Ref r{};
    r.present  = j.contains("n_predict") ? 1 : 0;
    r.n_predict = (j.contains("n_predict") && j["n_predict"].is_number_integer()) ? j["n_predict"].get<int64_t>() : -1;
    r.top_k    = (j.contains("top_k") && j["top_k"].is_number_integer()) ? j["top_k"].get<int64_t>() : 40;
    if (r.top_k < 0) r.top_k = 0;
    r.top_p    = (j.contains("top_p") && j["top_p"].is_number()) ? j["top_p"].get<float>() : 0.5f;
    return r;
}
int main() {
    std::vector<std::string> cases = {
        "{}", "{\"n_predict\":16}", "{\"n_predict\":-1}", "{\"n_predict\":0}",
        "{\"top_k\":0}", "{\"top_k\":-5}", "{\"top_k\":1000}", "{\"top_p\":0.95}",
        "{\"top_p\":0.0}", "{\"top_p\":1.0}", "{\"n_predict\":\"x\",\"top_k\":\"y\"}",
        "{\"n_predict\":7,\"top_k\":3,\"top_p\":0.7}", "{\"n_predict\":null}",
        "{\"top_k\":1.5}", "{\"top_p\":2}"
    };
    int failures = 0;
    for (const auto & c : cases) {
        json j = json::parse(c);
        int64_t out[4] = {0};
        fg251s_decide((int64_t)(uintptr_t) &j, out);
        Ref r = ref_decide(j);
        float gp; int32_t bits = (int32_t) out[2]; memcpy(&gp, &bits, 4);
        bool ok = out[0] == r.n_predict && out[1] == r.top_k &&
                  memcmp(&gp, &r.top_p, 4) == 0 && out[3] == r.present;
        if (!ok) {
            printf("DIFF %s: goose(n=%lld k=%lld p=%.6g present=%lld) ref_decide(n=%lld k=%lld p=%.6g present=%d)\n",
                   c.c_str(), (long long) out[0], (long long) out[1], gp, (long long) out[3],
                   (long long) r.n_predict, (long long) r.top_k, r.top_p, r.present);
            ++failures;
        }
    }
    printf("FG251S_GATE cases=%zu failures=%d\n", cases.size(), failures);
    return failures ? 1 : 0;
}
