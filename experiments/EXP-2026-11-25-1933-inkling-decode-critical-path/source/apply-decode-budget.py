from pathlib import Path

p=Path("inkling-backend.c")
s=p.read_text()


def replace(old,new):
    global s
    if s.count(old)!=1:
        raise ValueError(old)
    s=s.replace(old,new)


replace("static _Thread_local unsigned call_count[9];",
        """static _Thread_local unsigned call_count[9];
static _Thread_local bool time_decode;
static _Thread_local double decode_attention, decode_bridge, decode_moe, decode_head;""")
replace("    float proj[model_width], next[model_width], ffn[model_width];",
        """    float proj[model_width], next[model_width], ffn[model_width];
    double phase_start = time_decode ? now() : 0.0;""")
replace("""    assert(i2_postconv_token(vec(proj,model_width),coefficients(d,aux[2],4*model_width),""",
        """    if (time_decode) {
        const double at = now();
        decode_attention += at - phase_start;
        phase_start = at;
    }
    assert(i2_postconv_token(vec(proj,model_width),coefficients(d,aux[2],4*model_width),""")
replace("""        moe_resident(d, core, aux, slot, absolute, 1, ffn, messages ? messages[3] : NULL, next, cur);
        return;""",
        """        if (time_decode) {
            const double at = now();
            decode_bridge += at - phase_start;
            phase_start = at;
        }
        moe_resident(d, core, aux, slot, absolute, 1, ffn, messages ? messages[3] : NULL, next, cur);
        if (time_decode) decode_moe += now() - phase_start;
        return;""")
replace("    if (need_logits) read_token_logits(d, absolute, cur, blocks, count);",
        """    if (need_logits) {
        const double head_at = time_decode ? now() : 0.0;
        read_token_logits(d, absolute, cur, blocks, count);
        if (time_decode) decode_head += now() - head_at;
    }""")
replace("    fg_cuda_inkling_expert_phase(prompt, d->position, count);",
        """    time_decode = !prompt && getenv("I2_DECODE_BUDGET") != NULL;
    const double start = time_decode ? now() : 0.0;
    if (time_decode) {
        assert(use_attention_graph() && use_resident_moe());
        decode_attention = decode_bridge = decode_moe = decode_head = 0.0;
    }
    fg_cuda_inkling_expert_phase(prompt, d->position, count);""")
replace("    d->position += count;",
        """    if (time_decode) {
        fprintf(stderr, "I2 decode step=%d total=%.4f attn=%.4f bridge=%.4f moe=%.4f head=%.4f\\n",
            d->position, (now() - start) * 1000, decode_attention * 1000, decode_bridge * 1000,
            decode_moe * 1000, decode_head * 1000);
        time_decode = false;
    }
    d->position += count;""")
p.write_text(s)
