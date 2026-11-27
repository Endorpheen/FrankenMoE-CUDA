enum { max_prefill_batch = INKLING_PREFILL_LIMIT };

struct prefill_token {
    float current[model_width];
    float prelude[model_width];
    float previous[model_width];
    float blocks[12 * model_width];
    float bank[12 * model_width];
    float messages[4][model_width];
    int count, prelude_count, bank_count;
};

static struct ggml_tensor *prefill_input(struct ggml_context *ctx, float *rows, int width, int count) {
    struct ggml_tensor *tensor = ggml_new_tensor_2d(ctx, GGML_TYPE_F32, width, count);
    assert(tensor);
    tensor->data = rows;
    return tensor;
}

static void prefill_projection(struct decoder *d, int64_t id, float *input, int width,
                               float *output, int rows, int count) {
    struct ggml_context *ctx = context();
    struct ggml_tensor *w = load(ctx, named(d, id));
    struct ggml_tensor *x = prefill_input(ctx, input, width, count);
    struct ggml_tensor *nodes[] = {ggml_mul_mat(ctx, w, x)};
    evaluate(ctx, nodes, 1, output, rows * count);
    ggml_free(ctx);
}

static void prefill_loop_inputs(struct decoder *d, struct prefill_token *token, int layer, int pass, bool device_loop, bool device_prelude) {
    if (layer == 6 && pass == 0) {
        if (!device_prelude) memcpy(token->prelude, token->current, sizeof token->prelude);
        token->prelude_count = token->count;
    } else if (layer == 6) {
        token->bank_count = token->count - token->prelude_count - 1;
        assert(token->bank_count >= 0 && token->bank_count < 11);
        if (!device_loop) {
            memcpy(token->bank, token->blocks + (token->prelude_count + 1) * model_width,
                   (size_t)token->bank_count * model_width * sizeof(float));
            memcpy(token->previous, token->current, sizeof token->previous);
        }
        if (!device_prelude) memcpy(token->current, token->prelude, sizeof token->current);
        token->count = token->prelude_count;
    }
    if (pass > 0 && !device_loop) {
        mix(token->bank, token->bank_count, token->previous,
            coefficients(d, d->aux[layer * 30 + 17], 4 * model_width), 4, token->messages[0]);
    }
}

struct prefill_workspace {
    float inputs[3][max_prefill_batch][model_width];
    float projections[max_prefill_batch * 8664];
    float attention[max_prefill_batch][attn_width];
    float ff[max_prefill_batch][model_width];
    float (*linear)[model_width];
    float linear_storage[max_prefill_batch][model_width];
    float residual[max_prefill_batch][model_width];
    float control_input[max_prefill_batch][2 * model_width];
    float control_hidden[max_prefill_batch][64];
    float forget[max_prefill_batch][model_width];
    float gain[max_prefill_batch][model_width];
    float combined[max_prefill_batch][2 * model_width];
};

static void prefill_control(struct decoder *d, const int64_t *aux, int which, int count,
                            struct prefill_token *tokens, float (*current)[model_width],
                            struct prefill_workspace *scratch) {
    const int at = 18 + which * 3;
    if (use_resident_controller()) {
        for (int t = 0; t < count; ++t) {
            memcpy(scratch->gain[t], tokens[t].messages[which], sizeof scratch->gain[t]);
        }
        control_resident(d, aux, at, current[0], scratch->gain[0], count);
        return;
    }
    for (int t = 0; t < count; ++t) {
        float *const input = scratch->control_input[t];
        assert(i2_rms_gain(vec(current[t], model_width), vec(NULL, 0), 1e-6f,
            vec(input, model_width)) == model_width);
        assert(i2_rms_gain(vec(tokens[t].messages[which], model_width), vec(NULL, 0), 1e-6f,
            vec(input + model_width, model_width)) == model_width);
    }
    if (uses_resident_plan()) {
        project_control_group(d, aux, at, scratch->control_input[0], count, scratch->combined[0]);
        for (int t = 0; t < count; ++t) {
            memcpy(scratch->gain[t], scratch->combined[t], sizeof scratch->gain[t]);
            memcpy(scratch->forget[t], scratch->combined[t] + model_width, sizeof scratch->forget[t]);
        }
    } else {
        prefill_projection(d, aux[at], scratch->control_input[0], 2 * model_width,
            scratch->control_hidden[0], 64, count);
        prefill_projection(d, aux[at + 1], scratch->control_hidden[0], 64,
            scratch->forget[0], model_width, count);
        prefill_projection(d, aux[at + 2], scratch->control_hidden[0], 64,
            scratch->gain[0], model_width, count);
    }
    for (int t = 0; t < count; ++t) {
        assert(i2_loop_blend(vec(current[t], model_width), vec(tokens[t].messages[which], model_width),
            vec(scratch->gain[t], model_width), vec(scratch->forget[t], model_width),
            vec(current[t], model_width)) == model_width);
    }
}

struct i2_prefill_work {
    struct decoder *decoder;
    struct prefill_token *tokens;
    struct prefill_workspace *scratch;
    const int64_t *aux;
    int layer, pass;
    bool sliding;
    float **message;
    bool after_attention;
    bool shared_attention_input;
    sl_f32 postconv_weights;
    float *postconv_history;
    int postconv_rows;
    float *logits;
    bool skip_host_mix, device_attention_mix, device_current, device_boundary, device_loop, device_prelude, poison_host;
};

extern int64_t inkling_row_major_i2_postconv_batch(sl_f32 input, sl_f32 weights,
    sl_f32 history, sl_f32 out, int64_t width, int64_t from_channel, int64_t to_channel);

void fg_i2_prefill_partition(uint64_t address, int64_t begin, int64_t end) {
    const struct i2_prefill_work *work = (const void *)(uintptr_t)address;
    if (work->logits) {
        float *values = work->logits + begin;
        const int64_t count = end - begin;
        assert(I2_CALL(i2_text_logits)(vec(values, count), count, 30.0f) == count);
        for (int64_t i = 0; i < count; ++i) assert(isfinite(values[i]));
        return;
    }
    struct decoder *d = work->decoder;
    struct prefill_token *tokens = work->tokens;
    struct prefill_workspace *scratch = work->scratch;
    const int64_t *aux = work->aux;
    int layer = work->layer, pass = work->pass;
    const bool sliding = work->sliding;
    float **message = work->message;
    if (work->postconv_rows) {
        const int64_t items = (int64_t)work->postconv_rows * model_width;
        assert(inkling_row_major_i2_postconv_batch(
            vec(scratch->linear[0], items),
            work->postconv_weights,
            vec(work->postconv_history, 3 * model_width),
            vec(scratch->residual[0], items), model_width, begin, end) == end - begin);
        return;
    }
    if (work->after_attention) {
        for (int t = begin; t < end; ++t) {
            struct prefill_token *token = tokens + t;
            float *next = scratch->residual[t];
            float *ffn = scratch->ff[t];
            if (layer % 6 == 0) {
                assert(token->count < 11);
                float *history = token->blocks + token->count++ * model_width;
                if (!work->device_boundary) save_history_row(history, token->current);
            } else if (!work->skip_host_mix) {
                for (int i = 0; i < model_width; ++i) {
                    next[i] += token->current[i];
                }
            }
            if (!work->skip_host_mix) {
                mix(token->blocks, token->count, next, coefficients(d, aux[9], model_width), 1, ffn);
            } else if (work->poison_host) {
                for (int i = 0; i < model_width; ++i) ffn[i] = NAN;
            }
        }
        return;
    }
    for (int t = begin; t < end; ++t) {
        struct prefill_token *token = tokens + t;
        prefill_loop_inputs(d, token, layer, pass, work->device_loop, work->device_prelude);
        if (work->shared_attention_input) {
            if (aux[10] >= 0 && !work->device_attention_mix) {
                mix(token->blocks, token->count, token->current,
                    coefficients(d, aux[10], model_width), 1, scratch->inputs[0][t]);
            } else if (!work->device_current) {
                memcpy(scratch->inputs[0][t], token->current, sizeof token->current);
            }
            if (message && !work->device_loop) {
                for (int i = 0; i < 3; ++i) {
                    memcpy(message[i] + t * model_width, token->messages[i], sizeof token->current);
                }
            }
            continue;
        }
        if (work->device_attention_mix) {
            if (!work->device_current) memcpy(scratch->inputs[0][t], token->current, sizeof token->current);
            if (message && !work->device_loop) {
                for (int i = 0; i < 3; ++i)
                    memcpy(message[i] + t * model_width, token->messages[i], sizeof token->current);
            }
            continue;
        }
        float selected[3][model_width];
        if (aux[10] >= 0) {
            const int projections = sliding ? 1 : 3;
            mix(token->blocks, token->count, token->current,
                coefficients(d, aux[10], projections * model_width), projections, selected[0]);
            if (sliding) {
                memcpy(selected[1], selected[0], sizeof selected[0]);
                memcpy(selected[2], selected[0], sizeof selected[0]);
            }
        } else {
            for (int i = 0; i < 3; ++i) {
                memcpy(selected[i], token->current, sizeof selected[i]);
            }
        }
        for (int i = 0; i < 3; ++i) {
            memcpy(scratch->inputs[i][t], selected[i], sizeof selected[i]);
            if (message && !work->device_loop) memcpy(message[i] + t * model_width, token->messages[i], sizeof selected[i]);
        }
    }
}

#include "prefill-team.h"

extern int64_t i2_parallel_prefill(uint64_t work, int64_t rows, int64_t tasks);

static int prefill_task_count(int count) {
    const char *setting = getenv("I2_PARALLEL_PREP");
    if (count < 64 || (setting && strcmp(setting, "0") == 0)) return 1;
    int64_t tasks = fg_standalone_cpu_batch_threads();
    if (tasks > 16) tasks = 16;
    if (tasks > count / 32) tasks = count / 32;
    return tasks > 1 ? tasks : 1;
}

static void prepare_prefill(struct i2_prefill_team *team, struct i2_prefill_work *work, int count) {
    if (team) {
        prefill_team_run(team, work, count);
        return;
    }
    const int tasks = prefill_task_count(count);
    if (tasks > 1) {
        (void)use_cached_history();
        assert(i2_parallel_prefill((uint64_t)(uintptr_t)work, count, tasks) == count);
    } else {
        fg_i2_prefill_partition((uint64_t)(uintptr_t)work, 0, count);
    }
}

static void prefill_layer(struct decoder *d, struct prefill_token *tokens, struct prefill_workspace *scratch,
                          int count, int begin, const int64_t *visit, struct i2_prefill_team *team,
                          bool *device_current, bool retain_current, uint64_t output_slot,
                          bool *boundary_history, bool *tail_history, bool device_loop, bool device_prelude) {
    const int layer = (int)visit[0], slot = (int)visit[1], pass = (int)visit[2];
    const bool sliding = visit[3] != 0;
    const int64_t *core = d->core + 10 * layer;
    const int64_t *aux = d->aux + 30 * layer;
    const bool resident_attention = use_attention_graph();
    const char *conv_setting = getenv("I2_PARALLEL_CUDA_POSTCONV");
    const bool device_postconv = resident_attention && count > 1 &&
        (!conv_setting || strcmp(conv_setting, "0") != 0);
    float *message[] = {scratch->control_input[0],
        scratch->control_input[0] + count * model_width, scratch->gain[0]};
    struct i2_prefill_work setup = {d, tokens, scratch, aux, layer, pass, sliding,
        resident_attention && pass > 0 ? message : NULL, false};
    const bool direct_depthmix = count == 4096 && device_postconv &&
        use_cached_history() && use_resident_moe() && prefill_direct_output;
    setup.shared_attention_input = resident_attention && share_attention_inputs(sliding, aux);
    const char *mix_setting = getenv("I2_SLIDING_DEVICE_DEPTHMIX");
    const char *full_setting = getenv("I2_FULL_DEVICE_DEPTHMIX");
    const bool mixed_path = sliding ? setup.shared_attention_input && (!mix_setting || strcmp(mix_setting, "0") != 0) :
                                      (!full_setting || strcmp(full_setting, "0") != 0);
    const bool device_attention_mix = direct_depthmix && layer != 0 && aux[10] >= 0 && mixed_path;
    setup.device_attention_mix = device_attention_mix;
    setup.device_current = *device_current;
    setup.device_loop = device_loop;
    setup.device_prelude = device_prelude;
    const bool device_boundary = *device_current && (layer == 12 || layer == 18 || (layer == 6 && device_prelude));
    setup.device_boundary = device_boundary;
    assert(!*device_current || (device_attention_mix && (layer % 6 != 0 || device_boundary)));
    if (device_boundary) {
        if (layer != 6) assert(tokens[0].count == layer / 6);
        if (layer == 12) *boundary_history = true;
        else if (layer == 18) *tail_history = true;
        fprintf(stderr, "Inkling resident block history begin=%d layer=%d pass=%d\n", begin, layer, pass);
    }
    retain_current = retain_current && direct_depthmix && (!mix_setting || strcmp(mix_setting, "0") != 0);
    const char *prune = getenv("I2_PRUNE_PREFILL_HOST_STAGING");
    const bool prune_host = !getenv("I2_CHECK_STATE") && (!prune || strcmp(prune, "0") != 0);
    setup.poison_host = !prune_host;
    const bool skip_attention_prep = prune_host && device_attention_mix && *device_current &&
        layer != 6 && (!setup.message || device_loop);
    if (!skip_attention_prep) prepare_prefill(team, &setup, count);
    if (device_attention_mix) {
        float *input = scratch->inputs[0][0];
        const size_t values = (size_t)count * model_width;
        const int queries = sliding ? 1 : 3;
        const uint64_t full_outputs = sliding ? 0 : inkling_prefill_full_scratch(queries * values * sizeof(float));
        assert(sliding || full_outputs);
        assert(inkling_arena_depthmix(*device_current ? NULL : input, NULL,
                                    coefficients(d, aux[10], queries * model_width).data,
                                    input, tokens[0].count, device_boundary, full_outputs));
        if (!prune_host)
            for (int q = 0; q < queries; ++q)
                for (int t = 0; t < count; ++t)
                    for (int c = 0; c < model_width; ++c) scratch->inputs[q][t][c] = NAN;
    }
    struct ggml_context *ctx;
    int64_t work[16] = {0};
    if (resident_attention) {
        float *input[] = {scratch->inputs[0][0], scratch->inputs[1][0], scratch->inputs[2][0]};
        float *output = device_postconv ? scratch->residual[0] : scratch->linear[0];
        float *const old_direct_output = prefill_direct_output;
        if (device_postconv) {
            fg_cuda_i2_prefill_conv(coefficients(d, aux[2], 4 * model_width).data,
                                    d->ha + slot * 3 * model_width);
            if (old_direct_output) prefill_direct_output = output;
            if (direct_depthmix) {
                if (!prune_host)
                    for (int t = 0; t < count; ++t)
                        for (int c = 0; c < model_width; ++c) scratch->residual[t][c] = NAN;
                inkling_arena_residual_open(output);
            }
        }
        const float *queries = device_loop ? coefficients(d, aux[17], 4 * model_width).data : NULL;
        if (device_loop) {
            if (!prune_host)
                for (int q = 0; q < 3; ++q)
                    for (int t = 0; t < count; ++t)
                        for (int c = 0; c < model_width; ++c) message[q][t * model_width + c] = NAN;
            inkling_loop_bind(queries, message[0], message[1], message[2], NULL);
        }
        attention_graph(d, core, aux, slot, begin, count, sliding, input, pass > 0 ? message : NULL,
                        output);
        if (device_loop) {
            assert(inkling_loop_generated() == 7);
            inkling_loop_bind(NULL, NULL, NULL, NULL, NULL);
        }
        if (device_attention_mix) {
            const unsigned reads = inkling_arena_depthmix_reads();
            assert(reads);
            fprintf(stderr, "Inkling %s DepthMix begin=%d layer=%d pass=%d reads=%u\n",
                    sliding ? "sliding" : "full", begin, layer, pass, reads);
        }
        if (device_postconv) {
            fg_cuda_i2_prefill_conv(NULL, NULL);
            prefill_direct_output = old_direct_output;
        }
    } else {
        for (int i = 0; i < 3; ++i) {
            if (pass > 0) {
                prefill_control(d, aux, i, count, tokens, scratch->inputs[i], scratch);
            }
            for (int t = 0; t < count; ++t) {
                float *row = scratch->inputs[i][t];
                assert(i2_rms_gain(vec(row, model_width), coefficients(d, aux[sliding ? 11 : 12 + i], model_width),
                    1e-6f, vec(row, model_width)) == model_width);
            }
        }

        if (use_device_attention_input()) {
            native_attention_resident(d, core, aux, slot, begin, count, sliding,
                scratch->inputs[0][0], scratch->inputs[1][0], scratch->inputs[2][0], scratch->linear[0]);
        } else {
            ctx = context();
            int64_t in[3], output[8] = {0}, weight[8] = {0};
            for (int i = 0; i < 3; ++i) {
                in[i] = (intptr_t)prefill_input(ctx, scratch->inputs[i][0], model_width, count);
            }
            const int64_t ids[] = {core[0], core[1], core[2], aux[5], aux[8], core[4], aux[15], aux[16]};
            const int widths[] = {3072, 1024, 1024, 384, 24, 3072, 32, 32};
            const int matrices = sliding ? 6 : 8;
            const int packed = sliding ? 8600 : 8664;
            for (int i = 0; i < matrices; ++i) {
                weight[i] = (intptr_t)load(ctx, named(d, ids[i]));
            }
            assert(i2_attention_projections((intptr_t)ctx, ints(in, 3), ints(weight, 8), sliding,
                                            ints(work, 16), ints(output, 8)) == matrices);
            struct ggml_tensor *roots[8];
            for (int i = 0; i < matrices; ++i) {
                roots[i] = (struct ggml_tensor *)(intptr_t)output[i];
            }
            evaluate(ctx, roots, matrices, scratch->projections, packed * count);
            ggml_free(ctx);

            struct native_attention_inputs args = native_attention_inputs(d,count);
            for (int t = 0; t < count; ++t) {
                float projected[8664];
                size_t src = 0, dst = 0;
                for (int i = 0; i < matrices; ++i) {
                    memcpy(projected + dst, scratch->projections + src + t * widths[i], widths[i] * sizeof(float));
                    src += (size_t)count * widths[i];
                    dst += widths[i];
                }
                native_attention_prepare(d, args, aux, slot, begin + t, t, sliding, projected, packed);
            }
            if (use_resident_attention_projection()) {
                native_attention_project(d, args, aux, slot, begin, count, sliding, named(d, core[3]), scratch->linear[0]);
            } else {
                native_attention_submit(d, args, aux, slot, begin, count, sliding, scratch->attention[0]);
                prefill_projection(d, core[3], scratch->attention[0], attn_width, scratch->linear[0], model_width, count);
            }
        }
    }
    const char *postconv_batch = getenv("I2_PREFILL_POSTCONV_BATCH");
    if (!device_postconv && team && (!postconv_batch || strcmp(postconv_batch, "0") != 0)) {
        setup.postconv_weights = coefficients(d, aux[2], 4 * model_width);
        setup.postconv_history = d->ha + slot * 3 * model_width;
        setup.postconv_rows = count;
        prefill_team_run(team, &setup, model_width);
        setup.postconv_rows = 0;
    } else if (!device_postconv) {
        for (int t = 0; t < count; ++t) {
            assert(i2_postconv_token(vec(scratch->linear[t], model_width), coefficients(d, aux[2], 4 * model_width),
                vec(d->ha + slot * 3 * model_width, 3 * model_width), vec(scratch->residual[t], model_width)) == model_width);
        }
    }
    setup.after_attention = true;
    setup.message = NULL;
    setup.skip_host_mix = direct_depthmix;
    const bool skip_moe_prep = prune_host && direct_depthmix && layer % 6 != 0;
    if (!skip_moe_prep) prepare_prefill(team, &setup, count);
    if (skip_attention_prep || skip_moe_prep)
        fprintf(stderr, "Inkling pruned host prep begin=%d layer=%d pass=%d attention=%d moe=%d\n",
                begin, layer, pass, skip_attention_prep, skip_moe_prep);
    if (begin == 4096 && count > 1 && slot == 0 && getenv("I2_DRAFT_PARITY")) {
        check_state("draft-postconv", count, scratch->residual[1], model_width);
        check_state("draft-moe-input", count, scratch->ff[1], model_width);
    }
    if (direct_depthmix) {
        float (*stage)[model_width] = scratch->inputs[0];
        const int rows = tokens[0].count;
        const bool save_history = layer % 6 == 0;
        for (int t = 0; t < count; ++t) {
            assert(tokens[t].count == rows);
            const float *source = save_history ? tokens[t].blocks + (rows - 1) * model_width : tokens[t].current;
            if (!*device_current) memcpy(stage[t], source, sizeof stage[t]);
        }
        assert(inkling_arena_depthmix(*device_current ? NULL : stage[0], scratch->residual[0],
                                    coefficients(d, aux[9], model_width).data, scratch->ff[0], rows, save_history, 0));
    }
    if (use_resident_moe()) {
        float *messages = NULL;
        if (pass > 0) {
            for (int t = 0; t < count; ++t) {
                if (device_loop) {
                    if (!prune_host)
                        for (int c = 0; c < model_width; ++c) scratch->gain[t][c] = NAN;
                } else {
                    memcpy(scratch->gain[t], tokens[t].messages[3], sizeof scratch->gain[t]);
                }
            }
            messages = scratch->gain[0];
            if (device_loop) inkling_loop_bind(coefficients(d, aux[17], 4 * model_width).data,
                                                NULL, NULL, NULL, messages);
        }
        if (retain_current) inkling_moe_retain_output(scratch->linear[0], output_slot);
        moe_resident(d, core, aux, slot, begin, count, scratch->ff[0], messages,
                     scratch->residual[0], scratch->linear[0]);
        inkling_moe_retain_output(NULL, 0);
        if (device_loop) {
            assert(inkling_loop_generated() == 8);
            inkling_loop_bind(NULL, NULL, NULL, NULL, NULL);
            fprintf(stderr, "Inkling device loop messages begin=%d layer=%d pass=%d\n", begin, layer, pass);
        }
        if (direct_depthmix) {
            const unsigned reads = inkling_arena_depthmix_close();
            assert(reads);
            fprintf(stderr, "Inkling direct DepthMix begin=%d layer=%d pass=%d rows=%d device_reads=%u\n", begin, layer, pass, count, reads);
        }
        if (retain_current) {
            fprintf(stderr, "Inkling resident MoE handoff begin=%d layer=%d pass=%d\n", begin, layer, pass);
        } else {
            for (int t = 0; t < count; ++t) {
                memcpy(tokens[t].current, scratch->linear[t], sizeof tokens[t].current);
            }
        }
        *device_current = retain_current;
        return;
    }
    if (pass > 0) {
        prefill_control(d, aux, 3, count, tokens, scratch->ff, scratch);
    }
    for (int t = 0; t < count; ++t) {
        float *ffn = scratch->ff[t];
        assert(i2_rms_gain(vec(ffn, model_width), coefficients(d, core[9], model_width),
            1e-6f, vec(ffn, model_width)) == model_width);
    }

    ctx = context();
    int64_t moe_weights[4];
    const int64_t moe_ids[] = {core[5], core[8], core[6], core[7]};
    for (int i = 0; i < 4; ++i) {
        moe_weights[i] = (intptr_t)load(ctx, named(d, moe_ids[i]));
    }
    int64_t nodes[8] = {0};
    struct ggml_tensor *result = (struct ggml_tensor *)(intptr_t)i2_moe_ffn((intptr_t)ctx,
        (intptr_t)prefill_input(ctx, scratch->ff[0], model_width, count), ints(moe_weights, 4),
        ints(work, 16), ints(nodes, 8));
    assert(result);
    struct ggml_tensor *moe_root[] = {result};
    evaluate(ctx, moe_root, 1, scratch->linear[0], count * model_width);
    ggml_free(ctx);
    for (int t = 0; t < count; ++t) {
        float *current = tokens[t].current;
        assert(i2_postconv_token(vec(scratch->linear[t], model_width), coefficients(d, aux[3], 4 * model_width),
            vec(d->hf + slot * 3 * model_width, 3 * model_width), vec(current, model_width)) == model_width);
        for (int i = 0; i < model_width; ++i) {
            current[i] += scratch->residual[t][i];
            assert(isfinite(current[i]));
        }
    }
}

static void prefill(struct decoder *d, const int32_t *ids, int begin, int count, bool need_logits) {
    assert(count > 0 && count <= max_prefill_batch);
    struct prefill_token *tokens = calloc((size_t)count, sizeof *tokens);
    struct prefill_workspace *scratch = malloc(sizeof *scratch);
    assert(tokens && scratch);
    scratch->linear = scratch->linear_storage;
    const char *readback = getenv("I2_DIRECT_PREFILL_READBACK");
    if (count >= 512 && use_attention_graph() && use_resident_moe() &&
        (!readback || strcmp(readback, "0") != 0)) {
        const size_t elements = (size_t)count * model_width;
        if (readback && strcmp(readback, "pinned") == 0) {
            float (*pinned)[model_width] = fg_cuda_inkling_prefill_readback(elements * sizeof(float));
            if (pinned) scratch->linear = pinned;
        }
        prefill_direct_output = scratch->linear[0];
        prefill_direct_output_size = elements;
    }
    weight embedding = named(d, d->globals[0]);
    const int64_t row = ggml_row_size(GGML_TYPE_Q8_0, model_width);
    for (int t = 0; t < count; ++t) {
        assert(i2_embed_row((sl_u8){(uint8_t *)(uintptr_t)(embedding.data + row * ids[t]), row},
                            coefficients(d, d->globals[1], model_width), 1e-6f,
                            vec(tokens[t].current, model_width)) == model_width);
    }
    struct i2_prefill_team team = {0};
    struct i2_prefill_team *workers = NULL;
    const int tasks = prefill_task_count(count);
    const char *team_setting = getenv("I2_PREFILL_TEAM");
    if (tasks > 1 && (!team_setting || strcmp(team_setting, "0") != 0)) {
        (void)use_cached_history();
        if (prefill_team_open(&team, tasks, true)) workers = &team;
    }
    inkling_arena_window(count == 4096, begin);
    bool device_current = false, boundary_history = false, tail_history = false;
    bool device_loop = false;
    const char *loop_setting = getenv("I2_DEVICE_LOOP_MESSAGES");
    const bool loop_enabled = !loop_setting || strcmp(loop_setting, "0") != 0;
    const char *boundary_setting = getenv("I2_DEVICE_BLOCK_HISTORY");
    const char *sliding_setting = getenv("I2_SLIDING_DEVICE_DEPTHMIX");
    const char *full_setting = getenv("I2_FULL_DEVICE_DEPTHMIX");
    const bool boundary_enabled = (!boundary_setting || strcmp(boundary_setting, "0") != 0) &&
        (!sliding_setting || strcmp(sliding_setting, "0") != 0) &&
        (!full_setting || strcmp(full_setting, "0") != 0);
    const char *keep = getenv("I2_RETAIN_PREFILL_MOE");
    const char *full = getenv("I2_FULL_DEVICE_DEPTHMIX");
    const bool use_full = !full || strcmp(full, "0") != 0;
    const char *extend_setting = getenv("I2_DEVICE_REWIND_TAIL");
    const bool extend = (!extend_setting || strcmp(extend_setting, "0") != 0) && boundary_enabled && loop_enabled;
    const char *edges = getenv("I2_DEVICE_PREFILL_EDGES");
    const char *postconv = getenv("I2_PARALLEL_CUDA_POSTCONV");
    const bool use_edges = extend && count == 4096 && prefill_direct_output && use_resident_moe() &&
        use_cached_history() && (!edges || strcmp(edges, "0") != 0) &&
        (!postconv || strcmp(postconv, "0") != 0) && !getenv("I2_CHECK_STATE");
    bool device_prelude = false;
    inkling_prelude_clear();
    for (int i = 0; i < logical; ++i) {
        const int64_t *current = d->visits + i * 4;
        const int64_t *next = i + 1 < logical ? current + 4 : NULL;
        const bool rewind = next && next[0] == 6 && next[2] > 0;
        const bool tail = next && next[0] >= 18;
        const bool ordinary = current[0] < 18 && next && next[0] < 18 &&
            (next[0] % 6 != 0 || (boundary_enabled && next[0] == 12));
        const bool prelude_entry = use_edges && next && next[0] == 6 && next[2] == 0;
        const bool final = use_edges && !next;
        const bool retain = (!keep || strcmp(keep, "0") != 0) && count == 4096 &&
            (final || (next && (ordinary || prelude_entry || (extend && ((rewind && boundary_history) || tail))) &&
            (next[3] || use_full) && d->aux[next[0] * 30 + 10] >= 0));
        const uint64_t output_slot = retain && (tail || final) ? inkling_loop_storage() : 0;
        assert(!retain || !(tail || final) || output_slot);
        if (device_current && use_edges && current[0] == 6 && current[2] == 0) {
            assert(inkling_prelude_use(true));
            device_prelude = true;
        }
        if (current[2] < 0 && device_loop) {
            inkling_loop_release();
            device_loop = false;
        }
        if (boundary_history && current[0] == 6 && current[2] > 0) {
            if (loop_enabled) {
                if (!device_current) {
                    for (int t = 0; t < count; ++t)
                        memcpy(scratch->inputs[0][t], tokens[t].current, sizeof tokens[t].current);
                }
                assert(inkling_loop_save(inkling_loop_storage(), device_current ? NULL : scratch->inputs[0][0]));
                if (device_current) {
                    fprintf(stderr, "Inkling resident rewind begin=%d pass=%d\n", begin, (int)current[2]);
                    device_current = false;
                }
                if (device_prelude) {
                    assert(inkling_prelude_use(false));
                    device_current = true;
                }
                device_loop = true;
            } else {
                assert(inkling_arena_boundary_read(scratch->inputs[0][0], 2, false));
                for (int t = 0; t < count; ++t)
                    memcpy(tokens[t].blocks + 2 * model_width, scratch->inputs[0][t], sizeof tokens[t].current);
            }
            boundary_history = false;
        }
        prefill_layer(d, tokens, scratch, count, begin, current, workers,
                      &device_current, retain, output_slot, &boundary_history, &tail_history, device_loop, device_prelude);
        if (begin == 4096 && count > 1 && getenv("I2_DRAFT_PARITY"))
            check_state(count == 1 ? "draft-single-visit" : "draft-batch-visit", i,
                        tokens[1].current, model_width);
    }
    if (boundary_history && need_logits)
        assert(inkling_arena_boundary_read(tokens[count - 1].blocks + 2 * model_width, 2, true));
    if (tail_history && need_logits)
        assert(inkling_arena_boundary_read(tokens[count - 1].blocks + 3 * model_width, 3, true));
    if (need_logits && (device_current || device_prelude)) {
        struct prefill_token *last = tokens + count - 1;
        assert(inkling_prefill_final_rows(device_current ? last->current : NULL,
                                         device_prelude ? last->blocks + model_width : NULL));
    }
    if (device_prelude) fprintf(stderr, "Inkling resident prelude begin=%d\n", begin);
    if (device_current) fprintf(stderr, "Inkling final activation retained begin=%d logits=%d\n", begin, need_logits);
    inkling_prelude_clear();
    inkling_arena_window(false, begin);
    if (workers) prefill_team_close(workers);
    for (int t = 0; t < count; ++t) {
        check_state("residual", begin + t, tokens[t].current, model_width);
    }
    if (need_logits) {
        const bool draft = d->draft.count > 1 && d->draft.count == count;
        const int from = draft ? 0 : count - 1;
        for (int t = from; t < count; ++t) {
            struct prefill_token *row = tokens + t;
            read_token_logits(d, begin + t, row->current, row->blocks, row->count, draft);
            if (draft) memcpy(d->draft.scores + (size_t)t * vocabulary, d->logits,
                              (size_t)vocabulary * sizeof(float));
        }
    }
    prefill_direct_output = NULL;
    prefill_direct_output_size = 0;
    free(scratch);
    free(tokens);
}
