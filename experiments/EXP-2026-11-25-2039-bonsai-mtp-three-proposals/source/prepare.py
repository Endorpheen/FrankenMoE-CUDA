"""Route three- or four-row exact verification in the same experimental binary."""
from pathlib import Path
import sys


def replace(text, old, new):
    assert text.count(old) == 1, old
    return text.replace(old, new)


base, operations, choice, transaction, experiments, output = map(Path, sys.argv[1:])
text = base.read_text()
text = replace(text, "VERIFY_ROWS = 3,", "VERIFY_ROWS = 4,")
text = replace(text, "VERIFY_MERGE, KERNEL_COUNT",
    "VERIFY_MERGE, VERIFY_Q8_FOUR_1, VERIFY_Q8_FOUR_2, VERIFY_ATTN_FOUR, VERIFY_CONV_FOUR, KERNEL_COUNT")
text = replace(text, "    int device_commit;", "    int device_commit;\n    unsigned verify_rows;")
text = replace(text, "if (count == VERIFY_ROWS && m->verify_q8", "if (count == m->verify_rows && m->verify_q8")
text = replace(text, "return launch(m, vocabulary ? VERIFY_Q8_1 : VERIFY_Q8_2,",
    """const enum kernel projection = count == 4 ? (vocabulary ? VERIFY_Q8_FOUR_1 : VERIFY_Q8_FOUR_2)
                                                          : (vocabulary ? VERIFY_Q8_1 : VERIFY_Q8_2);
                return launch(m, projection,""")
text = replace(text, "i == 19 ? VERIFY_ROWS : 1", "i == 19 ? m->verify_rows : 1")
text = text.replace("prefix < VERIFY_ROWS - 1", "prefix < m->verify_rows - 1")
text = replace(text, "reserve(&ptr, VERIFY_ROWS * sizeof(struct verify_position))",
    "reserve(&ptr, m->verify_rows * sizeof(struct verify_position))")
text = replace(text, "    m->q8_mode = 1;", """    m->verify_rows = 3;
    const char *verify_rows = getenv("BONSAI_MTP_VERIFY_ROWS");
    if (verify_rows) {
        if (strcmp(verify_rows, "4") == 0 && m->device_commit && m->verify_q8 &&
            m->verify_native && m->verify_attention && m->verify_merge) {
            m->verify_rows = 4;
        } else if (strcmp(verify_rows, "3") != 0) {
            fprintf(stderr, "Unsupported MTP verifier width or four-row configuration\\n");
            goto error;
        }
    }
    m->q8_mode = 1;""")
text = replace(text, '"bonsai_verify_merge_parallel",', '''"bonsai_verify_merge_parallel",
        "bonsai_pq2_mvq_q8_four_1", "bonsai_pq2_mvq_q8_four_2",
        "bonsai_verify_q4_four_gqa2", "bonsai_conv_verify_prefix_four",''')
text = replace(text, "want_logits && count > VERIFY_ROWS", "want_logits && count > m->verify_rows")
(output / "backend.c").write_text(text)

ops = operations.read_text()
ops = ops.replace("count == VERIFY_ROWS", "count == m->verify_rows")
ops = ops.replace("t < VERIFY_ROWS", "t < m->verify_rows")
ops = ops.replace("sizeof(rows)", "m->verify_rows * sizeof(*rows)")
ops = ops.replace("VERIFY_ROWS * 24", "m->verify_rows * 24")
ops = ops.replace("first_keys + VERIFY_ROWS - 1", "first_keys + m->verify_rows - 1")
ops = replace(ops, "launch(m, VERIFY_ATTN, 12, max_tiles, 1, 32,",
    "launch(m, m->verify_rows == 4 ? VERIFY_ATTN_FOUR : VERIFY_ATTN, 12, max_tiles, 1, 32,")
(output / "verify-ops.inc").write_text(ops)

greedy = (base.parent / "greedy.inc").read_text()
greedy = replace(greedy, "m->ubatch < VERIFY_ROWS", "m->ubatch < m->verify_rows")
(output / "greedy.inc").write_text(greedy)
choice_code = (choice / "choice.inc").read_text()
choice_code = choice_code.replace("(size_t)VERIFY_ROWS *", "(size_t)m->verify_rows *")
choice_code = choice_code.replace("= VERIFY_ROWS * sizeof", "= m->verify_rows * sizeof")
(output / "choice.inc").write_text(choice_code)
run = (choice / "greedy-device.inc").read_text()
for old, new in (
    ("remaining <= VERIFY_ROWS", "remaining <= m->verify_rows"),
    ("pos > m->context - VERIFY_ROWS", "pos + m->verify_rows > m->context"),
    ("i < VERIFY_ROWS - 1", "i < m->verify_rows - 1"),
    ("i == 0 && save_conditioning", "i + 2 < m->verify_rows && save_conditioning"),
    ("tokens, VERIFY_ROWS, host_logits", "tokens, m->verify_rows, host_logits"),
    ("choose_device(m, VERIFY_ROWS,", "choose_device(m, m->verify_rows,"),
    ("kept < VERIFY_ROWS", "kept < m->verify_rows"),
    ("proposed += VERIFY_ROWS - 1", "proposed += m->verify_rows - 1"),
    ("kept == VERIFY_ROWS", "kept == m->verify_rows"),
    ("commit_3=%u ", "commit_3=%u commit_4=%u "),
    ("histogram[2], 1000", "histogram[2], histogram[3], 1000"),
    ("malloc((size_t)VERIFY_ROWS", "malloc((size_t)m->verify_rows"),
):
    run = replace(run, old, new)
(output / "greedy-device.inc").write_text(run)

txn = (transaction / "transaction.inc").read_text()
txn = replace(txn, "    CUdeviceptr args[] = {p[0], p[1], p[2], p[3], m->txn.prefix[i][0][0], m->txn.prefix[i][1][0]};",
    """    CUdeviceptr args[] = {p[0], p[1], p[2], p[3], m->txn.prefix[i][0][0],
                          m->txn.prefix[i][1][0], m->txn.prefix[i][2][0]};
    if (m->verify_rows == 4) {
        return launch(m, VERIFY_CONV_FOUR, 40, 1, 1, 256,
                      (void *[]){&args[0], &args[1], &args[2], &args[3], &args[4], &args[5], &args[6]});
    }""")
txn = replace(txn, "    const CUdeviceptr middle[] = {p[5], m->txn.prefix[i][0][1], m->txn.prefix[i][1][1], p[5]};",
    """    CUdeviceptr middle[VERIFY_ROWS + 1] = {
        p[5], m->txn.prefix[i][0][1], m->txn.prefix[i][1][1], m->txn.prefix[i][2][1], p[5],
    };
    middle[m->verify_rows] = p[5];""")
txn = txn.replace("t < VERIFY_ROWS", "t < m->verify_rows").replace("kept > VERIFY_ROWS", "kept > m->verify_rows")
txn = txn.replace("kept < VERIFY_ROWS", "kept < m->verify_rows")
(output / "transaction.inc").write_text(txn)

old_probe = experiments / "EXP-2026-11-25-2027-bonsai-mtp-target-verify/source/target-probe.inc"
helpers, marker, _ = old_probe.read_text().partition("static int probe_target(")
assert marker
test = (transaction / "commit-test.inc").read_text()
test = test.replace("const unsigned start = m->position;", "const unsigned start = m->position, rows = m->verify_rows;")
test = test.replace("start + CONTROL_ROWS", "start + rows + 1").replace("exact_verify(m, VERIFY_ROWS)", "exact_verify(m, rows)")
test = test.replace("m->ubatch < VERIFY_ROWS", "m->ubatch < rows")
test = test.replace("row < CONTROL_ROWS", "row <= rows").replace("kept <= VERIFY_ROWS", "kept <= rows")
test = test.replace("kept < VERIFY_ROWS", "kept < rows").replace("proposals, VERIFY_ROWS, batch", "proposals, rows, batch")
(output / "commit-probe.inc").write_text(helpers + test)
