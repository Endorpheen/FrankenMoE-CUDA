"""Add optional recurrent fusion inside the exact Bonsai target transaction."""
from pathlib import Path
import sys


def replace(text, old, new):
    assert text.count(old) == 1, old
    return text.replace(old, new)


backend, transaction, output = map(Path, sys.argv[1:])
text = backend.read_text()
text = replace(text, "FFN_PREFIX_Q8, KERNEL_COUNT", "FFN_PREFIX_Q8, VERIFY_GDN_PREFIX, KERNEL_COUNT")
text = replace(text, "    int device_commit;", "    int device_commit, verify_gdn_fused;")
text = replace(text, "    m->q8_mode = 1;", """    const char *fused_gdn = getenv("BONSAI_MTP_FUSED_GDN");
    m->verify_gdn_fused = fused_gdn && strcmp(fused_gdn, "1") == 0;
    m->q8_mode = 1;""")
text = replace(text, '"bonsai_ffn_prefix_q8",', '"bonsai_ffn_prefix_q8", "bonsai_verify_gdn_prefix",')
(output / "backend.c").write_text(text)
txn = transaction.read_text()
txn = replace(txn, "    CUdeviceptr middle[VERIFY_ROWS + 1] = {", """    if (m->verify_gdn_fused) {
        CUdeviceptr q = p[0], k = p[1], v = p[2], decay = p[3], beta = p[4], state = p[5], output = p[6];
        CUdeviceptr first = m->txn.prefix[i][0][1], second = m->txn.prefix[i][1][1];
        CUdeviceptr third = m->verify_rows == 4 ? m->txn.prefix[i][2][1] : 0;
        unsigned rows = m->verify_rows;
        if (launch(m, VERIFY_GDN_PREFIX, 48, 1, 8, 128,
                   (void *[]){&q, &k, &v, &decay, &beta, &state, &output, &state,
                               &first, &second, &third, &rows})) return -1;
        ++m->txn.next_layer;
        return 0;
    }
    CUdeviceptr middle[VERIFY_ROWS + 1] = {""")
(output / "transaction.inc").write_text(txn)
