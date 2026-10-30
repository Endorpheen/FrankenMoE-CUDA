#include "common.h"
#include "arg.h"
#include "llama.h"
#include "llama-ext.h"
#include "ggml-backend.h"
#include "nlohmann/json.hpp"
#include <openssl/evp.h>
#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <vector>

static void require(bool value, const char * message) {
    if (!value) throw std::runtime_error(message);
}

static nlohmann::json memory_report(llama_context * ctx) {
    nlohmann::json result = nlohmann::json::object();
    for (const auto & entry : llama_get_memory_breakdown(ctx)) {
        result[ggml_backend_buft_name(entry.first)] = {
            {"model", entry.second.model}, {"context", entry.second.context}, {"compute", entry.second.compute}};
    }
    return result;
}

static std::vector<uint8_t> state(llama_context * ctx, llama_state_seq_flags flags) {
    std::vector<uint8_t> result(llama_state_seq_get_size_ext(ctx, 0, flags));
    require(llama_state_seq_get_data_ext(ctx, result.data(), result.size(), 0, flags) == result.size(), "state read failed");
    return result;
}

static std::string digest(const std::vector<uint8_t> & data) {
    unsigned char hash[EVP_MAX_MD_SIZE];
    unsigned count = 0;
    require(EVP_Digest(data.data(), data.size(), hash, &count, EVP_sha256(), nullptr) == 1, "SHA-256 failed");
    const char digits[] = "0123456789abcdef";
    std::string result;
    for (unsigned i = 0; i < count; i++) { result += digits[hash[i] >> 4]; result += digits[hash[i] & 15]; }
    return result;
}

int main(int argc, char ** argv) {
    try {
        common_init();
        common_params params;
        require(common_params_parse(argc, argv, params, LLAMA_EXAMPLE_SERVER), "invalid arguments");
        const char * option = std::getenv("EXP087_NO_ALLOC");
        const bool metadata = option && std::string(option) == "1";
        params.no_alloc = metadata;
        if (metadata) params.load_mode = LLAMA_LOAD_MODE_NONE;
        params.fit_params = false;
        params.warmup = !metadata;
        params.n_outputs_max = 1;
        params.n_outputs_max_per_seq = 1;
        llama_backend_init();
        auto init = common_init_from_params(params);
        auto * ctx = init->context();
        require(ctx != nullptr, "context creation failed");
        nlohmann::json result = {{"no_alloc", metadata}, {"before", memory_report(ctx)}};
        require(result["before"].contains("CUDA0"), "target CUDA backend unavailable");
        if (!metadata) {
            require(llama_graph_reserve(ctx, params.n_ubatch, params.n_parallel, params.n_parallel) != nullptr,
                    "explicit physical reserve failed");
            result["after_reserve"] = memory_report(ctx);
            result["reserve_returned_graph"] = true;
            const char * path = std::getenv("GGML_PREFIX_TEST_REQUEST");
            require(path != nullptr, "missing state input fixture");
            std::ifstream file(path);
            require(file.good(), "state fixture unavailable");
            auto input = nlohmann::json::parse(file)["prompt"].get<std::vector<llama_token>>();
            const auto * vocab = llama_model_get_vocab(init->model());
            require(input.size() >= 32768, "state fixture too short");
            for (auto id : input) require(id >= 0 && id < llama_vocab_n_tokens(vocab), "invalid token id");
            std::vector<int8_t> outputs(params.n_batch, 0);
            auto prefill = [&](size_t begin, size_t end) {
                for (size_t i = begin; i < end; i += params.n_batch) {
                    auto count = std::min<size_t>(params.n_batch, end - i);
                    auto batch = llama_batch_get_one(input.data() + i, count);
                    batch.logits = outputs.data();
                    require(llama_decode(ctx, batch) == 0, "state prefill failed");
                }
            };
            constexpr size_t prefix = 24576;
            prefill(0, prefix);
            auto partial = state(ctx, LLAMA_STATE_SEQ_FLAGS_PARTIAL_ONLY);
            auto partial_before = digest(partial);
            auto full = state(ctx, LLAMA_STATE_SEQ_FLAGS_NONE);
            auto full_before = digest(full);
            const auto full_size = full.size();
            full.clear();
            full.shrink_to_fit();
            prefill(prefix, input.size());
            require(llama_state_seq_set_data_ext(ctx, partial.data(), partial.size(), 0, LLAMA_STATE_SEQ_FLAGS_PARTIAL_ONLY) == partial.size(), "partial restore failed");
            require(llama_memory_seq_rm(llama_get_memory(ctx), 0, prefix, -1), "rewind failed");
            auto partial_after = digest(state(ctx, LLAMA_STATE_SEQ_FLAGS_PARTIAL_ONLY));
            auto full_after = digest(state(ctx, LLAMA_STATE_SEQ_FLAGS_NONE));
            require(partial_before == partial_after && full_before == full_after, "state roundtrip differs");
            result["roundtrip"] = {{"prefix_tokens", prefix}, {"full_bytes", full_size}, {"partial_bytes", partial.size()},
                {"full_before", full_before}, {"full_after", full_after}, {"partial_before", partial_before},
                {"partial_after", partial_after}, {"full_match", true}, {"partial_match", true}};
        }
        std::cout << "PROBE_RESULT " << result.dump() << std::endl;
        init.reset();
        llama_backend_free();
        return 0;
    } catch (const std::exception & error) {
        std::cerr << "FAIL: " << error.what() << std::endl;
        return 1;
    }
}
