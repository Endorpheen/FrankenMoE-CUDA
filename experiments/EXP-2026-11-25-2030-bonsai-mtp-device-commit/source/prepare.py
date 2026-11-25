"""Add device-resident prefix ownership to the fixed-size Bonsai verifier."""
from pathlib import Path
import sys


def replace(text, old, new):
    assert text.count(old) == 1, old
    return text.replace(old, new)


backend, graph, output = map(Path, sys.argv[1:])
g = graph.read_text()
g = replace(g, "fn b_gdn(c: u64,", """extern "bonsai_gdn_defer_state_swap" fn defer_gdn_swap(context: u64) -> i64;

fn b_gdn(c: u64,""")
g = replace(g, """    let old = s[1];
    s[1] = b[23]; b[23] = old;""", """    if defer_gdn_swap(c) == 0 {
        let old = s[1];
        s[1] = b[23]; b[23] = old;
    }""")
(output / "target.goose").write_text(g)

text = backend.read_text()
text = replace(text, "VERIFY_Q8_2, KERNEL_COUNT", "VERIFY_Q8_2, VERIFY_CONV, KERNEL_COUNT")
text = replace(text, "typedef struct {\n    CUcontext cuda;", """enum target_phase { TXN_IDLE, TXN_RUNNING, TXN_READY, TXN_FAILED };

typedef struct {
    CUcontext cuda;
    int device_commit;
    struct {
        enum target_phase phase;
        unsigned start, layers, next_layer;
        unsigned layer_ids[LAYERS];
        CUdeviceptr prefix[LAYERS][VERIFY_ROWS - 1][2];
    } txn;""")
text = replace(text, '#include "verify-ops.inc"', '#include "transaction.inc"\n#include "verify-ops.inc"')
text = replace(text, "    if (exact_verify(m, count) &&\n", """    if (m->txn.phase == TXN_RUNNING) {
        if (op == CONV) return launch_transaction_conv(m, p);
        if (op == GDN) return launch_transaction_gdn(m, p);
    }
    if (exact_verify(m, count) &&
""")
text = replace(text, "    m->positions = reserve(&ptr, 16 * m->ubatch);", """    if (m->device_commit) {
        for (unsigned layer = 0; layer < LAYERS; ++layer) {
            if (m->bound[layer * 15] != 1) continue;
            const unsigned i = m->txn.layers++;
            m->txn.layer_ids[i] = layer;
            for (unsigned prefix = 0; prefix < VERIFY_ROWS - 1; ++prefix) {
                m->txn.prefix[i][prefix][0] = reserve(&ptr, sizeof(float) * GDN_HISTORY);
                m->txn.prefix[i][prefix][1] = reserve(&ptr, sizeof(float) * GDN_STATE);
            }
        }
    }
    m->positions = reserve(&ptr, 16 * m->ubatch);""")
text = replace(text, "    m->positions += m->memory; m->rope_q += m->memory;", """    for (unsigned i = 0; i < m->txn.layers; ++i) {
        for (unsigned prefix = 0; prefix < VERIFY_ROWS - 1; ++prefix) {
            for (unsigned plane = 0; plane < 2; ++plane) m->txn.prefix[i][prefix][plane] += m->memory;
        }
    }
    m->positions += m->memory; m->rope_q += m->memory;""")
text = replace(text, '    m->q8_mode = 1;', '''    const char *device_commit = getenv("BONSAI_MTP_DEVICE_COMMIT");
    m->device_commit = device_commit && strcmp(device_commit, "1") == 0;
    m->q8_mode = 1;''')
text = replace(text, '"bonsai_pq2_mvq_q8_three_2",', '"bonsai_pq2_mvq_q8_three_2", "bonsai_conv_verify_prefix",')
text = replace(text, "    const int pos = (int)m->position, keys = pos + (int)count;",
               "    if (m->txn.phase != TXN_IDLE) return -1;\n    const int pos = (int)m->position, keys = pos + (int)count;")
text = replace(text, "    const int64_t result = bonsai_graph(", """    const int transactional = m->device_commit && exact_verify(m, count);
    if (transactional) {
        m->txn.phase = TXN_RUNNING;
        m->txn.start = m->position;
        m->txn.next_layer = 0;
    }
    const int64_t result = bonsai_graph(""")
text = replace(text, """    if (result != 0) { fprintf(stderr, "Goose graph failed: %lld\\n", (long long)result); return -1; }
    m->position += count;
    return logits ? check(bonsai_cuda.fn_cuMemcpyDtoH(logits, m->b[19], (size_t)count * VOCAB * sizeof(float)), "logits") : 0;""", """    if (result != 0 || (transactional && m->txn.next_layer != m->txn.layers)) {
        if (transactional) m->txn.phase = TXN_FAILED;
        fprintf(stderr, "Goose graph failed: %lld\\n", (long long)result);
        return -1;
    }
    const int rc = logits ? check(bonsai_cuda.fn_cuMemcpyDtoH(logits, m->b[19],
        (size_t)count * VOCAB * sizeof(float)), "logits") : 0;
    if (transactional) {
        m->txn.phase = rc ? TXN_FAILED : TXN_READY;
    } else {
        m->position += count;
    }
    return rc;""")
text = replace(text, '#include "target-probe.inc"', '#include "commit-probe.inc"')
(output / "backend.c").write_text(text)
