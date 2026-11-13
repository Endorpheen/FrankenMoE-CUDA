#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace bmoe {

// Captures and replays the two sources of run-to-run workload drift: generated token IDs and
// selected expert IDs. Pure data: the hook gathers the ids backend-aware and hands them over, so
// this class never touches a tensor, and file I/O happens before or after generation, never while a
// graph is running.
class DeterministicWorkload {
public:
    enum class Mode { Capture, Replay };

    static std::unique_ptr<DeterministicWorkload> capture();
    static std::unique_ptr<DeterministicWorkload> load(const std::string & path, std::string & error);

    Mode mode() const { return mode_; }
    void set_static(std::string arch, int n_layer, int n_expert_used);
    bool check_static(const std::string & arch, int n_layer, int n_expert_used);
    bool prompt(std::vector<int32_t> & tokens);

    void begin_batch(int base_pos, int n_tokens, int phase);
    // Record one FINALIZED routing; `ids` is [n_expert_used, n_tokens] as gathered by the hook.
    bool capture_topk(int layer, int n_expert_used, int n_tokens, const int32_t * ids);
    // Validate the next route's shape and return its captured ids for the hook to write back.
    bool replay_topk(int layer, int n_expert_used, int n_tokens, const int32_t *& ids);

    // Capture records `natural`; replay replaces it with the token at `index`.
    bool token(size_t index, int32_t natural, int32_t & selected);
    bool finish(size_t emitted_tokens);
    bool save(const std::string & path);

    const std::string & error() const { return error_; }
    size_t route_count() const { return routes_.size(); }
    size_t token_count() const { return output_tokens_.size(); }

private:
    explicit DeterministicWorkload(Mode mode) : mode_(mode) {}

    struct Route {
        int phase = 0;
        int base_pos = 0;
        int batch_n = 0;
        int layer = 0;
        int top_k = 0;
        int token_n = 0;
        std::vector<int32_t> ids;
    };

    bool fail(std::string message);

    Mode mode_;
    std::string arch_;
    int n_layer_ = 0;
    int n_expert_used_ = 0;
    std::vector<int32_t> prompt_tokens_;
    std::vector<int32_t> output_tokens_;
    std::vector<Route> routes_;
    size_t route_cursor_ = 0;
    int batch_phase_ = 0;
    int batch_base_pos_ = 0;
    int batch_n_ = 0;
    std::string error_;
};

} // namespace bmoe
