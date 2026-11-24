enum { max_prefill_batch = 32 };

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

static void prefill_attend(struct decoder *d, const int64_t *aux, int slot, int position,
                           bool sliding, float *projected, int packed, float *attention) {
    float scratch[context_size];
    const size_t at = (size_t)slot * context_size * key_width;
    assert(i2_attention_token(vec(projected, packed), coefficients(d, aux[4], 3072),
        coefficients(d, aux[7], 384), coefficients(d, aux[0], 4096), coefficients(d, aux[1], 4096),
        coefficients(d, aux[6], 16384), position, context_size, sliding,
        ints(d->pos + slot * context_size, context_size),
        vec(d->keys + at, context_size * key_width), vec(d->values + at, context_size * key_width),
        vec(d->kh + slot * 3 * key_width, 3 * key_width),
        vec(d->vh + slot * 3 * key_width, 3 * key_width),
        vec(scratch, context_size), vec(attention, attn_width)) == attn_width);
}

static void prefill_loop_inputs(struct decoder *d, struct prefill_token *token, int layer, int pass) {
    if (layer == 6 && pass == 0) {
        memcpy(token->prelude, token->current, sizeof token->prelude);
        token->prelude_count = token->count;
    } else if (layer == 6) {
        token->bank_count = token->count - token->prelude_count - 1;
        assert(token->bank_count >= 0 && token->bank_count < 11);
        memcpy(token->bank, token->blocks + (token->prelude_count + 1) * model_width,
               (size_t)token->bank_count * model_width * sizeof(float));
        memcpy(token->previous, token->current, sizeof token->previous);
        memcpy(token->current, token->prelude, sizeof token->current);
        token->count = token->prelude_count;
    }
    if (pass > 0) {
        mix(token->bank, token->bank_count, token->previous,
            coefficients(d, d->aux[layer * 30 + 17], 4 * model_width), 4, token->messages[0]);
    }
}

struct prefill_workspace {
    float inputs[3][max_prefill_batch][model_width];
    float projections[max_prefill_batch * 8664];
    float attention[max_prefill_batch][attn_width];
    float ff[max_prefill_batch][model_width];
    float linear[max_prefill_batch][model_width];
    float residual[max_prefill_batch][model_width];
    float control_input[max_prefill_batch][2 * model_width];
    float control_hidden[max_prefill_batch][64];
    float forget[max_prefill_batch][model_width];
    float gain[max_prefill_batch][model_width];
};

static void prefill_control(struct decoder *d, const int64_t *aux, int which, int count,
                            struct prefill_token *tokens, float (*current)[model_width],
                            struct prefill_workspace *scratch) {
    for (int t = 0; t < count; ++t) {
        float *const input = scratch->control_input[t];
        assert(i2_rms_gain(vec(current[t], model_width), vec(NULL, 0), 1e-6f,
            vec(input, model_width)) == model_width);
        assert(i2_rms_gain(vec(tokens[t].messages[which], model_width), vec(NULL, 0), 1e-6f,
            vec(input + model_width, model_width)) == model_width);
    }
    const int at = 18 + which * 3;
    prefill_projection(d, aux[at], scratch->control_input[0], 2 * model_width,
        scratch->control_hidden[0], 64, count);
    prefill_projection(d, aux[at + 1], scratch->control_hidden[0], 64,
        scratch->forget[0], model_width, count);
    prefill_projection(d, aux[at + 2], scratch->control_hidden[0], 64,
        scratch->gain[0], model_width, count);
    for (int t = 0; t < count; ++t) {
        assert(i2_loop_blend(vec(current[t], model_width), vec(tokens[t].messages[which], model_width),
            vec(scratch->gain[t], model_width), vec(scratch->forget[t], model_width),
            vec(current[t], model_width)) == model_width);
    }
}

static void prefill_layer(struct decoder *d, struct prefill_token *tokens, struct prefill_workspace *scratch,
                          int count, int begin, const int64_t *visit) {
    const int layer = (int)visit[0], slot = (int)visit[1], pass = (int)visit[2];
    const bool sliding = visit[3] != 0;
    const int64_t *core = d->core + 10 * layer;
    const int64_t *aux = d->aux + 30 * layer;
    for (int t = 0; t < count; ++t) {
        struct prefill_token *token = tokens + t;
        prefill_loop_inputs(d, token, layer, pass);
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
        }
    }
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

    struct ggml_context *ctx = context();
    int64_t in[3], output[8] = {0}, weight[8] = {0}, work[16] = {0};
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

    for (int t = 0; t < count; ++t) {
        float projected[8664];
        size_t src = 0, dst = 0;
        for (int i = 0; i < matrices; ++i) {
            memcpy(projected + dst, scratch->projections + src + t * widths[i], widths[i] * sizeof(float));
            src += (size_t)count * widths[i];
            dst += widths[i];
        }
        prefill_attend(d, aux, slot, begin + t, sliding, projected, packed, scratch->attention[t]);
    }
    prefill_projection(d, core[3], scratch->attention[0], attn_width, scratch->linear[0], model_width, count);
    for (int t = 0; t < count; ++t) {
        struct prefill_token *token = tokens + t;
        float *next = scratch->residual[t];
        float *ffn = scratch->ff[t];
        assert(i2_postconv_token(vec(scratch->linear[t], model_width), coefficients(d, aux[2], 4 * model_width),
            vec(d->ha + slot * 3 * model_width, 3 * model_width), vec(next, model_width)) == model_width);
        if (layer % 6 == 0) {
            assert(token->count < 11);
            memcpy(token->blocks + token->count++ * model_width, token->current, sizeof token->current);
        } else {
            for (int i = 0; i < model_width; ++i) {
                next[i] += token->current[i];
            }
        }
        mix(token->blocks, token->count, next, coefficients(d, aux[9], model_width), 1, ffn);
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

static int prefill(struct decoder *d, const int *ids, int begin, int count, bool need_logits) {
    assert(count > 0 && count <= max_prefill_batch);
    struct prefill_token *tokens = calloc((size_t)count, sizeof *tokens);
    struct prefill_workspace *scratch = malloc(sizeof *scratch);
    assert(tokens && scratch);
    weight embedding = named(d, d->globals[0]);
    const int64_t row = ggml_row_size(GGML_TYPE_Q8_0, model_width);
    for (int t = 0; t < count; ++t) {
        assert(i2_embed_row((sl_u8){(uint8_t *)(uintptr_t)(embedding.data + row * ids[t]), row},
                            coefficients(d, d->globals[1], model_width), 1e-6f,
                            vec(tokens[t].current, model_width)) == model_width);
    }
    for (int i = 0; i < logical; ++i) {
        prefill_layer(d, tokens, scratch, count, begin, d->visits + i * 4);
    }
    for (int t = 0; t < count; ++t) {
        check_state("residual", begin + t, tokens[t].current, model_width);
    }
    int best = -1;
    if (need_logits) {
        struct prefill_token *last = tokens + count - 1;
        best = read_token_logits(d, begin + count - 1, last->current, last->blocks, last->count);
    }
    free(scratch);
    free(tokens);
    return best;
}
