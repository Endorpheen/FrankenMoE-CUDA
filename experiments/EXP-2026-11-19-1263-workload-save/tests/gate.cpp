// The writer against the real save(): capture a workload natively, save it, and require the ported
// writer to produce the same bytes from the same field values.
#include "/home/end0/AI/FrankenMoE-CUDA/runtime/core/src/moe/deterministic_workload.cpp"
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <array>
extern "C" int64_t g_save_call(const char*,int64_t,int64_t,int64_t,const int64_t*,int64_t,
                               const int64_t*,int64_t,const int64_t*,int64_t,char*);

int main() {
    struct C { const char * arch; int n_layer; int top_k; std::vector<int32_t> prompt, output;
               std::vector<std::vector<int32_t>> route_ids; std::vector<std::array<int,5>> route_hdr; };
    std::vector<C> cases;
    { C c; c.arch="qwen3moe"; c.n_layer=2; c.top_k=4; c.prompt={10,11,12}; c.output={20,21};
      c.route_ids={{7,8}}; c.route_hdr={{0,0,1,0}}; cases.push_back(c); }
    { C c; c.arch="gemma4"; c.n_layer=1; c.top_k=2; cases.push_back(c); }
    { C c; c.arch="gpt-oss"; c.n_layer=40; c.top_k=3; c.prompt={1}; c.output={2,3,4};
      c.route_ids={{1,2,3},{4,5,6}}; c.route_hdr={{1,0,1,0},{2,5,2,39}}; cases.push_back(c); }
    { C c; c.arch="a"; c.n_layer=1; c.top_k=1; c.prompt={0}; c.output={-1};
      c.route_ids={{0}}; c.route_hdr={{0,0,1,0}}; cases.push_back(c); }

    int fails = 0;
    for (size_t i = 0; i < cases.size(); ++i) {
        const C & c = cases[i];
        auto wl = bmoe::DeterministicWorkload::capture();
        wl->set_static(c.arch, c.n_layer, c.top_k);
        std::vector<int32_t> pr = c.prompt;
        wl->prompt(pr);
        for (size_t t = 0; t < c.output.size(); ++t) { int32_t sel = -1; wl->token(t, c.output[t], sel); }
        for (size_t r = 0; r < c.route_ids.size(); ++r) {
            wl->begin_batch(c.route_hdr[r][1], c.route_hdr[r][2], c.route_hdr[r][0]);
            // capture_topk reads n_tokens * n_expert_used ids; passing fewer made the native read
            // past the vector and write garbage into the record (my harness defect, caught by the
            // byte comparison).
            std::vector<int32_t> ids;
            for (int32_t id : c.route_ids[r]) for (int k = 0; k < c.top_k; ++k) ids.push_back(id);
            wl->capture_topk(c.route_hdr[r][3], c.top_k, (int)c.route_ids[r].size(), ids.data());
        }
        const std::string path = std::string("/tmp/wl_save_") + std::to_string(i) + ".txt";
        if (!wl->save(path)) { printf("native save failed for case %zu\n", i); ++fails; continue; }
        std::string want;
        { FILE * f = fopen(path.c_str(), "rb"); char buf[4096]; size_t n = fread(buf,1,sizeof(buf),f); fclose(f); want.assign(buf, n); }

        std::vector<int64_t> p(c.prompt.begin(), c.prompt.end()), o(c.output.begin(), c.output.end());
        p.resize(16, 0); o.resize(16, 0);
        std::vector<int64_t> routes;
        for (size_t r = 0; r < c.route_ids.size(); ++r) {
            for (int k = 0; k < 4; ++k) routes.push_back(c.route_hdr[r][k]);
            routes.push_back(c.top_k);
            routes.push_back((int64_t) c.route_ids[r].size());
            routes.push_back((int64_t) c.route_ids[r].size() * c.top_k);
            // the same expansion the native captured: one id per expert slot
            for (int32_t id : c.route_ids[r]) for (int k = 0; k < c.top_k; ++k) routes.push_back(id);
        }
        routes.resize(64, 0);
        char out[512]; memset(out, 0, sizeof(out));
        const int64_t n = g_save_call(c.arch, (int64_t) strlen(c.arch), c.n_layer, c.top_k,
                                      p.data(), (int64_t) c.prompt.size(), o.data(), (int64_t) c.output.size(),
                                      routes.data(), (int64_t) c.route_ids.size(), out);
        if (n != (int64_t) want.size() || memcmp(out, want.data(), (size_t) n) != 0) {
            ++fails;
            printf("MISMATCH case %zu: port=%lld native=%zu\n  port:   '%.*s'\n  native: '%s'\n",
                   i, (long long) n, want.size(), (int) n, out, want.c_str());
        }
    }
    printf("workload save: cases=%zu fails=%d\n", cases.size(), fails);
    return fails ? 1 : 0;
}
