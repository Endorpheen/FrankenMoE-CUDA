"""Select the verified two-head attention tile only in the exact target verifier."""
from pathlib import Path
import sys


def replace(text, old, new):
    assert text.count(old) == 1, old
    return text.replace(old, new)


backend, operations, output = map(Path, sys.argv[1:])
text = backend.read_text()
text = replace(text, "DRAFT_MAT_BATCH, KERNEL_COUNT", "DRAFT_MAT_BATCH, VERIFY_ATTN, KERNEL_COUNT")
text = replace(text, "    int verify_q8, verify_native;", "    int verify_q8, verify_native, verify_attention;")
text = replace(text, '    m->q8_mode = 1;', '''    const char *attn = getenv("BONSAI_MTP_VERIFY_SHARED_ATTN");
    m->verify_attention = attn && strcmp(attn, "1") == 0;
    m->q8_mode = 1;''')
text = replace(text, '"bonsai_mtp_concat", "bonsai_mtp_q4_four",',
               '"bonsai_mtp_concat", "bonsai_mtp_q4_four", "bonsai_verify_q4_gqa2",')
(output / "backend.c").write_text(text)

ops = operations.read_text()
ops = replace(ops, "static int launch_verify_rows(", """static int launch_shared_verify_attention(model *m, const CUdeviceptr p[8],
                                          unsigned first_keys, unsigned max_tiles) {
    CUdeviceptr q = p[0], k = p[1], v = p[2];
    float scale = 1.0f / 16.0f;
    if (launch(m, VERIFY_ATTN, 12, max_tiles, 1, 32,
               (void *[]){ &q, &k, &v, &m->partial, &first_keys, &max_tiles, &scale })) return -1;
    const size_t partial_bytes = (size_t)24 * max_tiles * 258 * sizeof(float);
    for (unsigned t = 0; t < VERIFY_ROWS; ++t) {
        CUdeviceptr partial = m->partial + t * partial_bytes;
        CUdeviceptr output = p[3] + (size_t)t * 6144 * sizeof(float);
        CUdeviceptr cfg = m->verify_positions + t * sizeof(struct verify_position) +
                          offsetof(struct verify_position, attention);
        int start = 0;
        if (launch(m, MERGE, 24, 1, 1, 32, (void *[]){ &partial, &output, &cfg, &start })) return -1;
    }
    return 0;
}

static int launch_verify_rows(""")
ops = replace(ops, "    const unsigned narrow = 1024;", """    if (op == TILES && m->verify_attention && m->grouped_decode) {
        const unsigned first_keys = m->position + 1;
        const unsigned max_tiles = (first_keys + VERIFY_ROWS - 1 + 255) / 256;
        const size_t bytes = (size_t)VERIFY_ROWS * 24 * max_tiles * 258 * sizeof(float);
        if (bytes <= m->partial_capacity) {
            return launch_shared_verify_attention(m, p, first_keys, max_tiles);
        }
    }
    const unsigned narrow = 1024;""")
(output / "verify-ops.inc").write_text(ops)
