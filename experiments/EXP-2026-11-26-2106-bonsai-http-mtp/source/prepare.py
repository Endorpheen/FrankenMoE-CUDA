"""Wire the saved Bonsai MTP implementation into the cumulative HTTP host."""

from pathlib import Path
import sys


def replace(source, old, new):
    if source.count(old) != 1:
        raise ValueError(f"Expected one source anchor: {old!r}")
    return source.replace(old, new)


def section(source, start, end):
    return source[source.index(start):source.index(end, source.index(start))]


base, main, saved, out = map(Path, sys.argv[1:])
host = (base / "backend.c").read_text()
model_host = (base / "model-host.c").read_text()
unit = main.read_text()
old = (saved / "bonsai-mtp-warp-rows-2047/backend.c").read_text()
five = saved / "bonsai-mtp-five-2046"
head = saved / "bonsai-mtp-head-2038"
pairs = saved / "bonsai-mtp-pairs-2043"

transaction = (five / "transaction.inc").read_text()
verify = (five / "verify-ops.inc").read_text()
verify = replace(verify, "    if (op == TILES && m->verify_attention", "    if (op == TILES && !m->decode_mma && m->verify_attention")
anchor = "            if (tile_status) return -1;"
verify = replace(verify, "            const int tile_status = m->grouped_decode", """            const int direct = m->grouped_decode && m->decode_mma &&
                               (keys >= 32768 || m->decode_mma == 2);
            const int tile_status = direct
                ? launch(m, m->decode_half2 ? DECODE_DIRECT_HALF2 : DECODE_DIRECT_MMA,
                         4, tiles, 1, 128, (void *[]){ &row[0], &row[1], &row[2], &m->partial, &keys, &tiles })
                : m->grouped_decode""")
verify = replace(verify, anchor, anchor + """
            if (m->grouped_decode && m->decode_ordered_merge && keys >= 32768) {
                status = launch(m, DECODE_ORDERED_MERGE, 24, 1, 1, 128,
                                (void *[]){ &m->partial, &row[3], &keys, &tiles });
                break;
            }""")
(out / "transaction.inc").write_text(transaction)
(out / "verify-ops.inc").write_text(verify)

# Bind only the functions that the port uses from the preserved second module.
kernel_ids = """MTP_EH MTP_MAT VERIFY_Q8_1 VERIFY_Q8_2 VERIFY_CONV DRAFT_CONCAT
DRAFT_MAT_BATCH VERIFY_ATTN VERIFY_MERGE VERIFY_ATTN_TAIL VERIFY_Q8_FOUR_1
VERIFY_Q8_FOUR_2 VERIFY_ATTN_FOUR VERIFY_CONV_FOUR VERIFY_Q8_FIVE_1 VERIFY_Q8_FIVE_2
VERIFY_ATTN_FIVE VERIFY_CONV_FIVE VERIFY_MVQ_FOUR_PAIR VERIFY_MVQ_FOUR_QUAD
VERIFY_MVQ_FIVE_QUAD""".split()
kernel_names = """bonsai_mtp_eh_q4 bonsai_mtp_mat_q4 bonsai_pq2_mvq_q8_three_1
bonsai_pq2_mvq_q8_three_2 bonsai_conv_verify_prefix bonsai_mtp_concat
bonsai_mtp_q4_four bonsai_verify_q4_gqa2 bonsai_verify_merge_parallel
bonsai_verify_q4_tail bonsai_pq2_mvq_q8_four_1 bonsai_pq2_mvq_q8_four_2
bonsai_verify_pair_tail3 bonsai_conv_verify_prefix_four bonsai_pq2_mvq_q8_five_1
bonsai_pq2_mvq_q8_five_2 bonsai_verify_five_32 bonsai_conv_verify_prefix_five
bonsai_mvq_4_permutation_2 bonsai_mvq_4_permutation_4 bonsai_mvq_5_spread_4""".split()
assert len(kernel_ids) == len(kernel_names)
assert all(f'"{name}"' in old for name in kernel_names)
extra = " ".join(f'[{key}] = "{name}",\n        ' for key, name in zip(kernel_ids, kernel_names))
host = replace(host, "    X(cuMemcpyDtoH)", "    X(cuMemcpyDtoDAsync) \\\n    X(cuMemcpyDtoH)")
host = replace(host, "GDN_STATE = 48", "VERIFY_ROWS = 5, GDN_STATE = 48")
host = replace(host, "CHOICE_FINAL, KERNEL_COUNT", "CHOICE_FINAL, " + ", ".join(kernel_ids) + ", KERNEL_COUNT")
host = replace(host, "typedef struct {\n    CUcontext cuda;", """enum target_phase { TXN_IDLE, TXN_RUNNING, TXN_READY, TXN_FAILED };

typedef struct {
    CUcontext cuda;
    CUmodule mtp_module;
    int mtp_enabled, mtp_request;
    unsigned mtp_pending[VERIFY_ROWS], mtp_next, mtp_end;
    unsigned verify_rows;
    int verify_native, verify_q8, verify_attention, verify_merge, verify_attn_tail;
    int device_commit, device_choice, draft_q8_head, draft_batch;
    int64_t mtp[4], mtp_layer[15], draft_kv[2];
    CUdeviceptr verify_rope_q, verify_rope_k, verify_positions;
    CUdeviceptr draft_carry, draft_chain, draft_rope_k;
    struct {
        enum target_phase phase;
        unsigned start, layers, next_layer;
        unsigned layer_ids[LAYERS];
        CUdeviceptr prefix[LAYERS][VERIFY_ROWS - 1][2];
    } txn;""")
host = replace(host, "static int gdn(model *m, const CUdeviceptr p[8])", "static int gdn(model *m, const CUdeviceptr p[8], unsigned tokens)")
host = replace(host, "if (m->count > 1) {\n        const uint64_t count = m->count;", "if (tokens > 1) {\n        const uint64_t count = tokens;")
host = replace(host, "m->gdn_prefill_v32 && m->count >= 64", "m->gdn_prefill_v32 && tokens >= 64")
host = replace(host, "launch(m, m->count > 1 ? GDN_BATCH", "launch(m, tokens > 1 ? GDN_BATCH")
host = replace(host, "return gdn(m, p);", "return gdn(m, p, count);")
host = replace(host, "static int flash_f32(", '#include "transaction.inc"\n#include "verify-ops.inc"\n\nstatic int flash_f32(')
verify_matrix = section(old, "            if (count == m->verify_rows &&", "            unsigned pairs = count")
verify_matrix = verify_matrix.replace("count == m->verify_rows && m->verify_q8 && m->in_decode && m->q8_mode == 1", "exact_verify(m, count)").replace("m->verify_mvq_shape", "m->mtp_enabled")
host = replace(host, "            unsigned pairs = count * in_width", verify_matrix + "            unsigned pairs = count * in_width")
host = replace(host, "    switch (op) {\n        case NORM:", """    if (m->txn.phase == TXN_RUNNING) {
        if (op == CONV) return launch_transaction_conv(m, p);
        if (op == GDN) return launch_transaction_gdn(m, p);
    }
    if (exact_verify(m, count) &&
        ((op == MATRIX && e == 1024) || op == DECAY || op == ROPE || op == TILES))
        return launch_verify_rows(m, (enum kernel)op, p);
    switch (op) {
        case NORM:""")
host = replace(host, "(i < 19 ? m->ubatch : 1)", "(i < 19 ? m->ubatch : i == 19 && m->mtp_enabled ? m->verify_rows : 1)")
for start, end in (
    ("    if (m->device_commit) {", "    m->positions = reserve"),
    ("    if (m->verify_native) {", "    if (m->stage_kv) {"),
    ("    if (m->verify_native) {\n        m->verify_rope_q +=", "    m->positions += m->memory;"),
):
    addition = section(old, start, end)
    if end == "    if (m->stage_kv) {":
        anchor = "    m->partial = reserve(&ptr, partial);\n"
        host = replace(host, anchor, anchor + addition)
    else:
        host = replace(host, end, addition + end)
binding = section(old, "    for (unsigned i = 0; i < 4; ++i) {\n        m->mtp[i]", "    fprintf(stderr, \"Device GGUF payload")
host = replace(host, '    fprintf(stderr, "Device GGUF payload', "    if (m->mtp_enabled) {\n" + binding + '    }\n    fprintf(stderr, "Device GGUF payload')
host = replace(host, "const size_t partial_bytes = CHOICE_JOBS", "const unsigned choice_rows = m->mtp_enabled ? m->verify_rows : 1;\n    const size_t partial_bytes = choice_rows * CHOICE_JOBS")
host = replace(host, "partial_bytes + sizeof(struct token_choice)", "partial_bytes + choice_rows * sizeof(struct token_choice)")

choice = section((pairs / "choice.inc").read_text(), "static int choose_device(", "static struct token_choice choose_host")
choice = choice.replace("choose_device(", "choose_mtp_rows(")
draft = section((pairs / "greedy.inc").read_text(), "enum { DRAFT_HIDDEN_BYTES", "static void emit_token")
draft = draft.replace("    if (m->device_choice && choice_open(m)) return -1;\n", "")
draft = draft.replace('    const char *device_choice = getenv("BONSAI_MTP_DEVICE_CHOICE");\n    m->device_choice = device_choice && strcmp(device_choice, "1") == 0;\n', "")
draft = replace(draft, "    const int target_decode = m->in_decode;",
                "    const int target_decode = m->in_decode, target_graph = m->ffn_graph_enabled;\n"
                "    m->ffn_graph_enabled = 0;")
draft = replace(draft, "    m->in_decode = target_decode;",
                "    m->in_decode = target_decode;\n    m->ffn_graph_enabled = target_graph;")
draft = draft.replace('    const char *draft_batch = getenv("BONSAI_MTP_BATCH_PREFILL");\n    m->draft_batch = draft_batch && strcmp(draft_batch, "1") == 0;', "    m->draft_batch = 1;")
prefix = section((head / "head-probe.inc").read_text(), "int64_t bonsai_mtp_mat(", "static int capture_mtp_head")
eh = section(old, "int64_t bonsai_mtp_eh(", "static int dump_device_row")
prefill_file = Path(__file__).resolve().parents[2] / "EXP-2026-11-25-2032-bonsai-mtp-batch-prefill/source/prefill-batch.inc"
prefill = prefill_file.read_text().split("static int dump_draft_cache(", 1)[0]
(out / "draft.inc").write_text(choice + eh + prefix + draft + prefill)
host = replace(host, "static model *bonsai_open(", '#include "draft.inc"\n\nstatic model *bonsai_open(')
host = replace(host, "    m->context = context; m->ubatch = ubatch;", """    m->context = context; m->ubatch = ubatch;
    const char *mtp_image = getenv("BONSAI_HTTP_MTP_IMAGE");
    if (mtp_image && *mtp_image) {
        m->mtp_enabled = m->verify_native = m->verify_q8 = m->device_commit = 1;
        m->verify_attention = m->verify_merge = m->verify_attn_tail = m->draft_q8_head = 1;
        m->verify_rows = 4;
    }""")
host = replace(host, '    for (unsigned i = 0; i < KERNEL_COUNT; ++i) {', '''    if (m->mtp_enabled &&
        check(bonsai_cuda.fn_cuModuleLoad(&m->mtp_module, mtp_image), "Bonsai MTP CUDA image")) goto error;
    const char *mtp_names[KERNEL_COUNT] = {
        ''' + extra.rstrip() + '''
    };
    for (unsigned i = 0; i < KERNEL_COUNT; ++i) {
        if (i > CHOICE_FINAL) {
            if (m->mtp_enabled && check(bonsai_cuda.fn_cuModuleGetFunction(&m->kernel[i],
                m->mtp_module, mtp_names[i]), mtp_names[i])) goto error;
            continue;
        }''')
host = replace(host, "    ffn_graph_open(m);", """    if (m->mtp_enabled) {
        bonsai_mtp_goose_init(0, NULL);
        if (!m->choice_mask || draft_open(m)) goto error;
    }
    ffn_graph_open(m);""")
host = replace(host, "    m->ffn_graph_layer = 0;\n    const int64_t result = bonsai_graph(", """    m->ffn_graph_layer = 0;
    const int verify = exact_verify(m, count);
    if (m->txn.phase != TXN_IDLE || (verify && (logits || !device_only || init_verify_batch(m)))) return -1;
    if (verify) {
        m->txn.phase = TXN_RUNNING;
        m->txn.start = m->position;
        m->txn.next_layer = 0;
    }
    const int64_t result = (verify ? bonsai_mtp_target_graph : bonsai_graph)(""")
host = replace(host, "if (result != 0 || m->ffn_graph_phase ||", "if (result != 0 || (verify && m->txn.next_layer != m->txn.layers) || m->ffn_graph_phase ||")
host = replace(host, "    m->position += count;\n    return logits", "    if (verify) m->txn.phase = TXN_READY;\n    else m->position += count;\n    return logits")
host = replace(host, "    if (!status) {\n        /* Attention writes", """    if (!status && m->mtp_enabled) {
        status = check(bonsai_cuda.fn_cuMemsetD8(m->draft_carry, 0, DRAFT_HIDDEN_BYTES), "reset draft seed");
    }
    if (!status) {
        m->mtp_next = m->mtp_end = 0;
        m->mtp_request = 0;
        m->txn.phase = TXN_IDLE;
        /* Attention writes""")
host = replace(host, "        at += batch;", "        if (!status && m->mtp_request && !m->in_decode) status = draft_prefill(m, tokens + at, batch);\n        at += batch;")
host = replace(host, '#include "draft.inc"', '#include "mtp-abi.h"\n#include "draft.inc"')
host += '\n#include "http-mtp.inc"\n'

model_host += """
extern void bonsai_session_mtp_prepare(struct bonsai_session *, int);
extern int64_t bonsai_session_mtp_next(struct bonsai_session *, uint32_t, int, int64_t);

void fg_bonsai_mtp_prepare(uint8_t allowed) {
    if (H.bonsai) bonsai_session_mtp_prepare(H.bonsai, allowed);
}
"""
model_host = replace(model_host, "int64_t fg_bonsai_choose_next(int32_t token, sl_f32 output, uint8_t ignore_eos) {",
                     "extern int64_t bonsai_session_mtp_next(struct bonsai_session *, uint32_t, int, int64_t);\n\n"
                     "int64_t fg_bonsai_choose_next(int32_t token, sl_f32 output, uint8_t ignore_eos, int64_t remaining) {")
model_host = replace(model_host, "bonsai_session_choose_next(H.bonsai, (uint32_t)token, ignore_eos != 0);",
                     "bonsai_session_mtp_next(H.bonsai, (uint32_t)token, ignore_eos != 0, remaining);")
unit = replace(unit, 'extern "fg_bonsai_choose_next" fn q35_choose_next(token: i32, logits: f32[:], ignore_eos: bool) -> i64;',
               '''extern "fg_bonsai_choose_next" fn q35_choose_next(token: i32, logits: f32[:], ignore_eos: bool, remaining: i64) -> i64;
extern "fg_bonsai_mtp_prepare" fn q35_mtp_prepare(allowed: bool);''')
unit = replace(unit, "    q35_generation_reset();\n    var read: i64 = 0;", "    q35_generation_reset();\n    q35_mtp_prepare(device_choice && limit > 4);\n    var read: i64 = 0;")
unit = replace(unit, "q35_choose_next(batch[0], logits[..], s.ignore_eos)", "q35_choose_next(batch[0], logits[..], s.ignore_eos, limit - produced)")
(out / "backend.c").write_text(host)
(out / "model-host.c").write_text(model_host)
(out / "main-mtp.goose").write_text(unit)
