"""Add the exact combined prefix before the existing Bonsai gate/up projections."""
from pathlib import Path
import sys


def replace(text, old, new):
    assert text.count(old) == 1, old
    return text.replace(old, new)


base, graph, output = map(Path, sys.argv[1:])
g = graph.read_text()
g = replace(g, "fn b_ffn_finish(c: u64,", """extern "bonsai_ffn_prefix_launch" fn launch_ffn_prefix(c: u64, hidden: i64, branch: i64,
    weight: i64, signs: i64, residual: i64, owner: i64) -> i64;

fn b_ffn_prefix(c: u64, b: i64[:], weight: i64) -> i64 {
    let fused = launch_ffn_prefix(c, b[0], b[12], weight, b[20], b[13], b[2]);
    if fused > 0 { return 0; }
    if fused < 0 ||
       b_add(c, b[0], b[12], b[13]) != 0 ||
       b_norm(c, b[13], weight, 0, b[1], 5120, 1, 0) != 0 ||
       b_rotate(c, b[1], b[20], b[2], 5120, 0, 0) != 0 { return -1; }
    return 0;
}

fn b_ffn_finish(c: u64,""")
g = replace(g, """           b_add(c, b[0], b[12], b[13]) != 0 ||
           b_norm(c, b[13], post, 0, b[1], 5120, 1, 0) != 0 ||
           b_rotate(c, b[1], b[20], b[2], 5120, 0, 0) != 0 ||""", """           b_ffn_prefix(c, b, post) != 0 ||""")
(output / "prefix.goose").write_text(g)

text = base.read_text()
text = replace(text, "VERIFY_CONV_FOUR, KERNEL_COUNT", "VERIFY_CONV_FOUR, FFN_PREFIX_FP16, FFN_PREFIX_Q8, KERNEL_COUNT")
text = replace(text, "    unsigned ffn_layer;", "    unsigned ffn_layer;\n    int ffn_prefix;")
text = replace(text, "int64_t bonsai_ffn_tail_launch(", """int64_t bonsai_ffn_prefix_launch(uint64_t context, int64_t hidden, int64_t branch,
    int64_t weight, int64_t signs, int64_t residual, int64_t owner) {
    model *m = (model *)(uintptr_t)context;
    if (!m->ffn_prefix || m->in_decode || !m->norm_wide || m->count < 128) return 0;
    const unsigned layer = m->ffn_layer + 1;
    const int q8 = m->ffn_q8 && (m->ffn_q8 == 1 || layer > (unsigned)(LAYERS - m->ffn_q8)) &&
                   (size_t)m->count * (5120 / 128) * sizeof(float) <= m->partial_capacity;
    CUdeviceptr x = hidden, y = branch, w = weight, s = signs, dst = residual;
    void *args[] = {&x, &y, &w, &s, &dst, &m->packed, &m->epsilon, NULL};
    if (q8) {
        args[6] = &m->partial;
        args[7] = &m->epsilon;
    }
    if (launch(m, q8 ? FFN_PREFIX_Q8 : FFN_PREFIX_FP16, m->count, 1, 1, 256, args)) return -1;
    m->packed_source = q8 ? 0 : (CUdeviceptr)owner;
    m->ffn_q8_input = q8 ? (CUdeviceptr)owner : 0;
    m->q8_input = 0;
    return 1;
}

int64_t bonsai_ffn_tail_launch(""")
text = replace(text, '    const char *ffn_q8 = getenv("BONSAI_FFN_Q8_PREFILL");', '''
    const char *ffn_prefix = getenv("BONSAI_FFN_PREFIX");
    m->ffn_prefix = ffn_prefix && strcmp(ffn_prefix, "1") == 0;
    const char *ffn_q8 = getenv("BONSAI_FFN_Q8_PREFILL");''')
text = replace(text, '"bonsai_verify_pair_tail3", "bonsai_conv_verify_prefix_four",',
    '"bonsai_verify_pair_tail3", "bonsai_conv_verify_prefix_four", "bonsai_ffn_prefix_fp16", "bonsai_ffn_prefix_q8",')
(output / "backend.c").write_text(text)
