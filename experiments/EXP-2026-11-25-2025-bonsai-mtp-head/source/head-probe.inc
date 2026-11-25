int64_t bonsai_mtp_mat(uint64_t context, int64_t weights, int64_t input, int64_t output,
                       int64_t width, int64_t rows) {
    model *m = (model *)(uintptr_t)context;
    if (m->count != 1 || width <= 0 || width % 32 || rows <= 0 || rows % 4) return -1;
    CUdeviceptr w = weights, x = input, y = output;
    unsigned blocks = (unsigned)width / 32;
    return launch(m, MTP_MAT, (unsigned)rows / 4, 1, 1, 128, (void *[]){ &w, &x, &y, &blocks });
}

static int configure_mtp_row(model *m, unsigned pos) {
    const unsigned keys = pos + 1;
    const int64_t tiles = (keys + 255) / 256;
    const int32_t positions[4] = {pos, pos, pos, 0};
    const int64_t cfg[22] = {
        256, 256, 1, 24, 4, 4, keys, 24576, 1024, KV_BYTES, 144, KV_BYTES, 144,
        0, 1024, 24576, 256, tiles, 24, 24, 24 * tiles * 258 * 4, keys,
    };
    m->count = 1;
    m->position = pos;
    m->wave = 24;
    return check(bonsai_cuda.fn_cuMemcpyHtoD(m->positions, positions, sizeof(positions)), "draft positions") ||
           check(bonsai_cuda.fn_cuMemcpyHtoD(m->attention, cfg, sizeof(cfg)), "draft attention");
}

static int capture_mtp_head(model *m, const unsigned *ids, unsigned count, const char *path) {
    enum { HIDDEN = 5120, MAX_ROWS = 16 };
    if (count < 2 || count > MAX_ROWS) return -1;
    const size_t hidden_bytes = HIDDEN * sizeof(float), logit_bytes = VOCAB * sizeof(float);
    const size_t kv_bytes = (size_t)count * KV_BYTES;
    float *hidden = calloc((size_t)count * HIDDEN, sizeof(float));
    float *reference = malloc(count * logit_bytes);
    float *logits = malloc(logit_bytes);
    CUdeviceptr kv;
    int64_t state[2];
    FILE *file = NULL;
    int status = -1;
    if (!hidden || !reference || !logits ||
        check(bonsai_cuda.fn_cuMemAlloc(&kv, 2 * kv_bytes), "isolated draft KV")) goto done;
    // The CUDA context owns the diagnostic allocation until the CLI exits.
    state[0] = (int64_t)kv;
    state[1] = (int64_t)(kv + kv_bytes);
    for (unsigned row = 1; row < count; ++row) {
        if (evaluate_batch(m, &ids[row-1], 1, NULL) ||
            check(bonsai_cuda.fn_cuMemcpyDtoH(hidden + (size_t)row * HIDDEN, m->b[0], hidden_bytes),
                  "trunk hidden")) goto done;
    }

    file = fopen(path, "wb");
    if (!file) goto done;
    for (unsigned pass = 0; pass < 2; ++pass) {
        if (check(bonsai_cuda.fn_cuMemsetD8(kv, 0, 2 * kv_bytes), "draft KV reset")) goto done;
        for (unsigned row = 0; row < count; ++row) {
            if (configure_mtp_row(m, row) ||
                check(bonsai_cuda.fn_cuMemcpyHtoD(m->b[0], hidden + (size_t)row * HIDDEN, hidden_bytes),
                      "draft conditioning")) goto done;
            const int64_t rc = bonsai_mtp_head((uintptr_t)m, slice(m->mtp_layer, 15),
                slice(state, 2), slice(m->mtp, 4), slice(m->globals, 3), slice(m->b, 24), ids[row], row);
            if (rc) {
                fprintf(stderr, "Draft graph failed at row %u: %lld\n", row, (long long)rc);
                goto done;
            }
            if (check(bonsai_cuda.fn_cuMemcpyDtoH(logits, m->b[19], logit_bytes), "draft logits")) goto done;
            float *saved = reference + (size_t)row * VOCAB;
            if (pass) {
                if (memcmp(logits, saved, logit_bytes) != 0) {
                    fprintf(stderr, "Draft replay mismatch at row %u\n", row);
                    goto done;
                }
                continue;
            }
            memcpy(saved, logits, logit_bytes);
            unsigned best = 0;
            for (unsigned id = 0; id < VOCAB; ++id) {
                if (!isfinite(logits[id])) goto done;
                if (logits[id] > logits[best]) best = id;
            }
            if (fwrite(logits, logit_bytes, 1, file) != 1 ||
                dump_device_row(file, m->b[13], HIDDEN) ||
                dump_device_row(file, m->b[1], HIDDEN)) goto done;
            printf("{\"position\":%u,\"input\":%u,\"draft\":%u,\"logit\":%.9g}\n",
                   row, ids[row], best, logits[best]);
        }
    }
    fprintf(stderr, "Draft replay: %u rows, %zu logits byte-identical; isolated KV %zu bytes\n",
            count, (size_t)count * VOCAB, 2 * kv_bytes);
    status = 0;
done:
    if (file && fclose(file) != 0) status = -1;
    free(hidden);
    free(reference);
    free(logits);
    return status;
}
