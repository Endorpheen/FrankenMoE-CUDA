#pragma once

#include "llama-model.h"
#include "llama-graph.h"
#include "llama-model-loader.h"

// note: almost all graphs require at least sqrtf, so include cmath globally
#include <cmath>
#include <map>

class llama_memory_hybrid_idx_context;

// ref: https://github.com/ggml-org/llama.cpp/pull/28068
static inline ggml_tensor * build_gdn_l2_norm(ggml_context * ctx, ggml_tensor * x, float eps) {
    const float n = x->ne[0];

    return ggml_scale(ctx, ggml_rms_norm(ctx, x, eps/n), 1.0f/sqrtf(n));
}

//
// base classes
//

struct llm_build_mamba_base : public llm_graph_context {
    llm_build_mamba_base(const llm_graph_params & params);

    virtual ~llm_build_mamba_base() = default;

    ggml_tensor * build_mamba_layer(llm_graph_input_rs * inp, ggml_tensor * cur, const llama_model & model, const llama_ubatch & ubatch, int il);
    ggml_tensor * build_mamba2_layer(llm_graph_input_rs * inp, ggml_tensor * cur, const llama_model & model, const llama_ubatch & ubatch, int il) const;

};

struct llm_build_delta_net_base : public llm_graph_context {
    llm_build_delta_net_base(const llm_graph_params & params);

    virtual ~llm_build_delta_net_base() = default;

    // returns pair of output and new state
    std::pair<ggml_tensor *, ggml_tensor *> build_delta_net_chunking(
                ggml_tensor * q,
                ggml_tensor * k,
                ggml_tensor * v,
                ggml_tensor * g,
                ggml_tensor * b,
                ggml_tensor * s,
                        int   il);

    // returns pair of output and new state
    std::pair<ggml_tensor *, ggml_tensor *> build_delta_net_autoregressive(
                ggml_tensor * q,
                ggml_tensor * k,
                ggml_tensor * v,
                ggml_tensor * g,
                ggml_tensor * b,
                ggml_tensor * s,
                int           il);

    // use the ggml_gated_delta_net fused operator (K=1; state has shape [S_v, S_v, H_v, n_seqs])
    std::pair<ggml_tensor *, ggml_tensor *> build_delta_net_fused(
                ggml_tensor * q,
                ggml_tensor * k,
                ggml_tensor * v,
                ggml_tensor * g,
                ggml_tensor * b,
                ggml_tensor * s,
                        int   il);

    // choose one of two implementations above based on the number of tokens
    std::pair<ggml_tensor *, ggml_tensor *> build_delta_net(
                ggml_tensor * q,
                ggml_tensor * k,
                ggml_tensor * v,
                ggml_tensor * g,
                ggml_tensor * b,
                ggml_tensor * s,
                        int   il);

    // read conv state from cache, concat with qkv_mixed, write back (single slot or per-token)
    // qkv_mixed: (qkv_dim, n_seq_tokens, n_seqs); returns conv_input: (kernel_size + n_seq_tokens - 1, channels, n_seqs)
    ggml_tensor * build_conv_state(
            llm_graph_input_rs * inp,
            ggml_tensor *        conv_states_all,
            ggml_tensor *        qkv_mixed,
            int64_t              conv_kernel_size,
            int64_t              conv_channels,
            int                  il);

    // run delta-net attention and write the new recurrent state(s) back to ssm_states_all
    // s: (head_v_dim, head_v_dim, num_v_heads, n_seqs); returns output: (head_v_dim, num_v_heads, n_seq_tokens, n_seqs)
    ggml_tensor * build_recurrent_attn(
            llm_graph_input_rs * inp,
            ggml_tensor *        ssm_states_all,
            ggml_tensor *        q,
            ggml_tensor *        k,
            ggml_tensor *        v,
            ggml_tensor *        g,
            ggml_tensor *        b,
            ggml_tensor *        s,
            int                  il);
};

struct llm_build_lfm_base : public llm_graph_context {
    llm_build_lfm_base(const llm_graph_params & params);

    virtual ~llm_build_lfm_base() = default;

    ggml_tensor * build_lfm_shortconv(const llama_model & model,
                                      const llama_memory_recurrent_context * mctx_recr,
                                      llm_graph_input_rs * inp_recr,
                                      ggml_tensor * cur,
                                      int il);

    ggml_tensor * build_lfm_dense_ffn(const llama_model & model,
                                      ggml_tensor * cur,
                                      int il);

    ggml_tensor * build_lfm_moe_ffn(const llama_model & model,
                                    ggml_tensor * cur,
                                    int il);
};

struct llm_build_rwkv6_base : public llm_graph_context {
    const llama_model & model;

    llm_build_rwkv6_base(const llama_model & model, const llm_graph_params & params);

    virtual ~llm_build_rwkv6_base() = default;

    ggml_tensor * build_rwkv6_channel_mix(const llama_layer * layer,
                                          ggml_tensor *       cur,
                                          ggml_tensor *       x_prev,
                                          llm_arch            arch) const;

    ggml_tensor * build_rwkv6_time_mix(llm_graph_input_rs * inp,
                                       ggml_tensor *        cur,
                                       ggml_tensor *        x_prev,
                                       const llama_ubatch & ubatch,
                                       int                  il) const;
};

// Base class for RWKV7-related models
struct llm_build_rwkv7_base : public llm_graph_context {
    const llama_model & model;

    llm_build_rwkv7_base(const llama_model & model, const llm_graph_params & params);

    virtual ~llm_build_rwkv7_base() = default;

    // RWKV7-specific graph building methods
    ggml_tensor * build_rwkv7_channel_mix(const llama_layer * layer,
                                          ggml_tensor *       cur,
                                          ggml_tensor *       x_prev,
                                          llm_arch            arch) const;
    ggml_tensor * build_rwkv7_time_mix(llm_graph_input_rs * inp,
                                       ggml_tensor *        cur,
                                       ggml_tensor *        x_prev,
                                       ggml_tensor *&       first_layer_value,
                                       const llama_ubatch & ubatch,
                                       int                  il) const;
};

//
// models
//











































// Quant-only stub for mmproj GGUFs
// none of these are ever called, they only exist to satisfy the llama_model_base interface
























// Qwen3 with the decision head of vllm-sr Decision 2.0: one score per option






















































































































































































































struct msa_params {
    int blk;
    int topk_blocks;
    int local;
};










// Qwen3.5 backbone evaluated without memory, with a joint decision head on top of its hidden states



struct llama_model_qwen35 : public llama_model_base {
    llama_model_qwen35(const struct llama_model_params & params) : llama_model_base(params) {}
    void load_arch_hparams(llama_model_loader & ml) override;
    void load_arch_tensors(llama_model_loader & ml) override;

    struct graph : public llm_build_delta_net_base {
        graph(const llama_model & model, const llm_graph_params & params);
    private:
        ggml_tensor * build_layer_attn(
        llm_graph_input_attn_kv * inp_attn,
                    ggml_tensor * cur,
                    ggml_tensor * inp_pos,
                            int * sections,
                            int   il);

        ggml_tensor * build_layer_attn_linear(
             llm_graph_input_rs * inp,
                    ggml_tensor * cur,
                            int   il);

        ggml_tensor * build_layer_ffn(
                    ggml_tensor * cur,
                            int   il);

        ggml_tensor * build_norm_gated(
                    ggml_tensor * input,
                    ggml_tensor * weights,
                    ggml_tensor * gate,
                            int   layer);

        // returns pair of qkv, z
        std::pair<ggml_tensor *, ggml_tensor *> build_qkvz(
                    ggml_tensor * input,
                            int   il);

        const llama_model & model;
    };

    struct graph_mtp : public llm_graph_context {
        graph_mtp(const llama_model & model, const llm_graph_params & params);
    };

    std::unique_ptr<llm_graph_context> build_arch_graph(const llm_graph_params & params) const override;
};
struct llama_model_qwen3 : public llama_model_base {
    llama_model_qwen3(const struct llama_model_params & params) : llama_model_base(params) {}
    void load_arch_hparams(llama_model_loader & ml) override;
    void load_arch_tensors(llama_model_loader & ml) override;

    struct graph : public llm_graph_context {
        graph(const llama_model & model, const llm_graph_params & params);
    protected:
        // the helpers alone, a derived graph builds its own body
        struct no_build {};
        graph(const llm_graph_params & params, no_build) : llm_graph_context(params) {}

        // the layers and the final norm: [n_embd, n_outputs]
        ggml_tensor * build_backbone(const llama_model & model);
    };

    std::unique_ptr<llm_graph_context> build_arch_graph(const llm_graph_params & params) const override;
};
struct llama_model_qwen35moe : public llama_model_base {
    llama_model_qwen35moe(const struct llama_model_params & params) : llama_model_base(params) {}
    void load_arch_hparams(llama_model_loader & ml) override;
    void load_arch_tensors(llama_model_loader & ml) override;

    struct graph : public llm_build_delta_net_base {
        graph(const llama_model & model, const llm_graph_params & params);
    private:
        ggml_tensor * build_layer_attn(
        llm_graph_input_attn_kv * inp_attn,
                    ggml_tensor * cur,
                    ggml_tensor * inp_pos,
                            int * sections,
                            int   il);

        ggml_tensor * build_layer_attn_linear(
             llm_graph_input_rs * inp,
                    ggml_tensor * cur,
                            int   il);

        ggml_tensor * build_layer_ffn(
                    ggml_tensor * cur,
                            int   il);

        ggml_tensor * build_norm_gated(
                    ggml_tensor * input,
                    ggml_tensor * weights,
                    ggml_tensor * gate,
                            int   layer);

        // returns pair of qkv, z
        std::pair<ggml_tensor *, ggml_tensor *> build_qkvz(
                    ggml_tensor * input,
                            int   il);

        const llama_model & model;
    };

    struct graph_mtp : public llm_graph_context {
        graph_mtp(const llama_model & model, const llm_graph_params & params);
    };

    std::unique_ptr<llm_graph_context> build_arch_graph(const llm_graph_params & params) const override;
};















