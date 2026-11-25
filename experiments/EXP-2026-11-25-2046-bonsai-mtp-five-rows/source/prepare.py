"""Add optional five-row Bonsai verification without changing the four-row path."""
from pathlib import Path
import sys


def replace(text, old, new):
    assert text.count(old) == 1, old
    return text.replace(old, new)


backend, pairs, output = map(Path, sys.argv[1:])
text = backend.read_text()
text = replace(text, "VERIFY_ROWS = 4,", "VERIFY_ROWS = 5,")
text = replace(text, "FFN_PREFIX_Q8, KERNEL_COUNT",
    "FFN_PREFIX_Q8, VERIFY_Q8_FIVE_1, VERIFY_Q8_FIVE_2, VERIFY_ATTN_FIVE, VERIFY_CONV_FIVE, KERNEL_COUNT")
text = replace(text, """                const enum kernel projection = count == 4 ? (vocabulary ? VERIFY_Q8_FOUR_1 : VERIFY_Q8_FOUR_2)
                                                          : (vocabulary ? VERIFY_Q8_1 : VERIFY_Q8_2);""", """                enum kernel projection = vocabulary ? VERIFY_Q8_1 : VERIFY_Q8_2;
                if (count == 4) projection = vocabulary ? VERIFY_Q8_FOUR_1 : VERIFY_Q8_FOUR_2;
                if (count == 5) projection = vocabulary ? VERIFY_Q8_FIVE_1 : VERIFY_Q8_FIVE_2;""")
text = replace(text, 'strcmp(verify_rows, "4") == 0 && m->device_commit',
    '(strcmp(verify_rows, "4") == 0 || strcmp(verify_rows, "5") == 0) && m->device_commit')
text = replace(text, "            m->verify_rows = 4;", """            m->verify_rows = (unsigned)(verify_rows[0] - '0');""")
text = replace(text, '"bonsai_ffn_prefix_q8",', '''"bonsai_ffn_prefix_q8",
        "bonsai_pq2_mvq_q8_five_1", "bonsai_pq2_mvq_q8_five_2",
        "bonsai_verify_five_32", "bonsai_conv_verify_prefix_five",''')
(output / "backend.c").write_text(text)

ops = (pairs / "verify-ops.inc").read_text()
ops = replace(ops, """    if (launch(m, m->verify_rows == 4 ? VERIFY_ATTN_FOUR : (m->verify_attn_tail ? VERIFY_ATTN_TAIL : VERIFY_ATTN), m->verify_rows == 4 ? 8 : 12, max_tiles, m->verify_rows == 4 ? 2 : 1, 32,""", """
    enum kernel attention = m->verify_attn_tail ? VERIFY_ATTN_TAIL : VERIFY_ATTN;
    unsigned groups = 12, panels = 1;
    if (m->verify_rows == 4) {
        attention = VERIFY_ATTN_FOUR;
        groups = 8;
        panels = 2;
    } else if (m->verify_rows == 5) {
        attention = VERIFY_ATTN_FIVE;
        panels = 2;
    }
    if (launch(m, attention, groups, max_tiles, panels, 32,""")
(output / "verify-ops.inc").write_text(ops)

txn = (pairs / "transaction.inc").read_text()
txn = replace(txn, "m->txn.prefix[i][1][0], m->txn.prefix[i][2][0]};",
    """m->txn.prefix[i][1][0], m->txn.prefix[i][2][0], m->txn.prefix[i][3][0]};
    if (m->verify_rows == 5) {
        return launch(m, VERIFY_CONV_FIVE, 40, 1, 1, 256,
                      (void *[]){&args[0], &args[1], &args[2], &args[3], &args[4], &args[5], &args[6], &args[7]});
    }""")
txn = replace(txn, "m->txn.prefix[i][2][1], p[5],", "m->txn.prefix[i][2][1], m->txn.prefix[i][3][1], p[5],")
(output / "transaction.inc").write_text(txn)
