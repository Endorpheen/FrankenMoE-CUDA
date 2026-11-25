"""Keep single-token arithmetic for shape-sensitive verifier operations."""
from pathlib import Path
import sys


def replace(text, old, new):
    assert text.count(old) == 1, old
    return text.replace(old, new)


base, output = map(Path, sys.argv[1:])
text = base.read_text()
text = replace(text, "    int verify_q8;", """    int verify_q8, verify_native;
    CUdeviceptr verify_rope_q, verify_rope_k, verify_positions;""")
text = replace(text, "static int gdn(model *m, const CUdeviceptr p[8])", "static int gdn(model *m, const CUdeviceptr p[8], unsigned tokens)")
text = replace(text, "    if (m->count > 1) {\n        const uint64_t count = m->count;",
               "    if (tokens > 1) {\n        const uint64_t count = tokens;")
text = replace(text, "if (m->gdn_prefill_v32 && m->count >= 64)", "if (m->gdn_prefill_v32 && tokens >= 64)")
text = replace(text, "return launch(m, m->count > 1 ? GDN_BATCH : GDN", "return launch(m, tokens > 1 ? GDN_BATCH : GDN")
text = replace(text, "static int flash_f32(", '#include "verify-ops.inc"\n\nstatic int flash_f32(')
text = replace(text, "    switch (op) {\n        case NORM:", """    if (exact_verify(m, count) &&
        ((op == MATRIX && e == 1024) || op == DECAY || op == CONV ||
         op == GDN || op == ROPE || op == TILES)) {
        return launch_verify_rows(m, (enum kernel)op, p);
    }
    switch (op) {
        case NORM:""")
text = replace(text, "return gdn(m, p);", "return gdn(m, p, count);")
text = replace(text, "    m->partial = reserve(&ptr, partial);", """    m->partial = reserve(&ptr, partial);
    if (m->verify_native) {
        m->verify_rope_q = reserve(&ptr, 27 * 8);
        m->verify_rope_k = reserve(&ptr, 27 * 8);
        m->verify_positions = reserve(&ptr, VERIFY_ROWS * sizeof(struct verify_position));
    }""")
text = replace(text, "    m->positions += m->memory; m->rope_q += m->memory;", """    if (m->verify_native) {
        m->verify_rope_q += m->memory;
        m->verify_rope_k += m->memory;
        m->verify_positions += m->memory;
        if (rope_config(m->verify_rope_q, 24, 1) || rope_config(m->verify_rope_k, 4, 1)) return -1;
    }
    m->positions += m->memory; m->rope_q += m->memory;""")
text = replace(text, "    m->q8_mode = 1;", """    const char *verify_native = getenv("BONSAI_MTP_VERIFY_EXACT");
    m->verify_native = verify_native && strcmp(verify_native, "1") == 0;
    m->q8_mode = 1;""")
text = replace(text, "    const int64_t result = bonsai_graph(", """    if (exact_verify(m, count) && init_verify_batch(m)) return -1;
    const int64_t result = bonsai_graph(""")
output.write_text(text)
