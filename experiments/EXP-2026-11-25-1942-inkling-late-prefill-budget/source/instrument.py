from pathlib import Path
p = Path("prefill.h")
s = p.read_text()
def replace(old, new):
    global s
    assert s.count(old) == 1, old
    s = s.replace(old, new)

replace("static void prefill_layer(struct decoder *d,", """extern int nvtxRangePushA(const char *message);
extern int nvtxRangePop(void);

static _Thread_local double input_ms, attention_ms, postconv_ms, glue_ms, moe_ms;

static void prefill_layer(struct decoder *d,""")
replace("""    setup.shared_attention_input = resident_attention && share_attention_inputs(sliding, aux);
    prepare_prefill(team, &setup, count);""",
"""    setup.shared_attention_input = resident_attention && share_attention_inputs(sliding, aux);
    const bool tracing = begin >= 28672;
    if (tracing) nvtxRangePushA("Inkling input prep");
    const double start = now();
    prepare_prefill(team, &setup, count);
    input_ms += 1000.0 * (now() - start);
    if (tracing) {
        nvtxRangePop();
        nvtxRangePushA("Inkling attention");
    }
    const double attn_start = now();""")
replace("""    const char *postconv_batch = getenv("I2_PREFILL_POSTCONV_BATCH");""",
"""    attention_ms += 1000.0 * (now() - attn_start);
    if (tracing) {
        nvtxRangePop();
        nvtxRangePushA("Inkling postconv");
    }
    const double post_start = now();
    const char *postconv_batch = getenv("I2_PREFILL_POSTCONV_BATCH");""")
replace("""    setup.after_attention = true;
    setup.message = NULL;
    prepare_prefill(team, &setup, count);""",
"""    postconv_ms += 1000.0 * (now() - post_start);
    if (tracing) {
        nvtxRangePop();
        nvtxRangePushA("Inkling FFN prep");
    }
    const double glue_start = now();
    setup.after_attention = true;
    setup.message = NULL;
    prepare_prefill(team, &setup, count);
    glue_ms += 1000.0 * (now() - glue_start);
    if (tracing) {
        nvtxRangePop();
        nvtxRangePushA("Inkling MoE");
    }
    const double moe_start = now();""")
replace("""            memcpy(tokens[t].current, scratch->linear[t], sizeof tokens[t].current);
        }
        return;""",
"""            memcpy(tokens[t].current, scratch->linear[t], sizeof tokens[t].current);
        }
        moe_ms += 1000.0 * (now() - moe_start);
        if (tracing) nvtxRangePop();
        return;""")
replace("""static void prefill(struct decoder *d, const int32_t *ids, int begin, int count, bool need_logits) {
    assert(count > 0 && count <= max_prefill_batch);""",
"""static void prefill(struct decoder *d, const int32_t *ids, int begin, int count, bool need_logits) {
    assert(count > 0 && count <= max_prefill_batch && use_resident_moe());
    const double start = now();
    input_ms = attention_ms = postconv_ms = glue_ms = moe_ms = 0;
    if (begin >= 28672) nvtxRangePushA("Inkling late prefill");""")
replace("""    free(scratch);
    free(tokens);""",
"""    free(scratch);
    free(tokens);
    if (begin >= 28672) nvtxRangePop();
    fprintf(stderr, "I2 prefill phases first=%d count=%d wall=%.4f input=%.4f attention=%.4f postconv=%.4f glue=%.4f moe=%.4f\\n",
            begin, count, (now() - start) * 1000, input_ms, attention_ms, postconv_ms, glue_ms, moe_ms);""")
p.write_text(s)
