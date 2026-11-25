"""Add a layer-bounded Q8 down-projection and a separate quality probe."""
from pathlib import Path
import sys


def replace(text, old, new):
    assert text.count(old) == 1, old
    return text.replace(old, new)


base, output, quality = map(Path, sys.argv[1:])
text = base.read_text()
text = replace(text, "    unsigned ffn_layer;", "    unsigned ffn_layer, ffn_down_q8_last;")
text = replace(text, "int64_t bonsai_ffn_tail_launch(", """static int ffn_down_q8_enabled(const model *m) {
    return m->ffn_down_q8_last != 0 && m->count >= 128 &&
           m->ffn_layer > LAYERS - m->ffn_down_q8_last &&
           (size_t)m->count * (17408 / 128) * sizeof(float) <= m->partial_capacity;
}

int64_t bonsai_ffn_tail_launch(""")
text = replace(text, "if (!m->ffn_tail || m->count<32) return 0;", """if (!m->ffn_tail || m->count < 32 || ffn_down_q8_enabled(m)) return 0;""")
text = replace(text, """            if (m->ffn_q8 && (ffn_gate || ffn_up) && scale_bytes <= m->partial_capacity &&
                (m->ffn_q8 == 1 || m->ffn_layer > (unsigned)(LAYERS - m->ffn_q8))) {""", """
            const int down_q8 = in_width == 17408 && out_width == 5120 &&
                p[2] == (CUdeviceptr)m->b[18] && ffn_down_q8_enabled(m);
            if (down_q8 || (m->ffn_q8 && (ffn_gate || ffn_up) && scale_bytes <= m->partial_capacity &&
                (m->ffn_q8 == 1 || m->ffn_layer > (unsigned)(LAYERS - m->ffn_q8)))) {""")
text = replace(text, '    const char *ffn_q8 = getenv("BONSAI_FFN_Q8_PREFILL");', '''
    const char *down_q8 = getenv("BONSAI_FFN_DOWN_Q8_LAST");
    if (down_q8) {
        char *end;
        const unsigned long layers = strtoul(down_q8, &end, 10);
        if (down_q8 == end || *end || layers > 16) goto error;
        m->ffn_down_q8_last = (unsigned)layers;
    }
    if (m->ffn_down_q8_last) fprintf(stderr, "Q8 FFN down: last %u layers\\n", m->ffn_down_q8_last);
    const char *ffn_q8 = getenv("BONSAI_FFN_Q8_PREFILL");''')
output.write_text(text)

text = replace(text, '    const char *head_output = getenv("BONSAI_MTP_HEAD_CAPTURE");', '''
    FILE *dump = NULL, *replay = NULL;
    const char *head_output = getenv("BONSAI_MTP_HEAD_CAPTURE");''')
text = replace(text, "    const double decoding = seconds();", '''    const double decoding = seconds();
    const char *dump_path = getenv("BONSAI_LOGITS_FILE");
    const char *replay_path = getenv("BONSAI_REPLAY_IDS");
    if (dump_path) dump = fopen(dump_path, "wb");
    if (replay_path) replay = fopen(replay_path, "r");
    if ((dump_path && !dump) || (replay_path && !replay) || (use_draft && (dump || replay))) status = 1;''')
text = replace(text, "        if (status) break;\n        if (ignore_eos)", '''        if (status) break;
        if (dump && (i < 4 || i % 8 == 0 || i + 1 == generate)) {
            const uint32_t step = i;
            if (fwrite(&step, sizeof(step), 1, dump) != 1 ||
                fwrite(logits, sizeof(*logits), VOCAB, dump) != VOCAB) {
                status = 1;
                break;
            }
        }
        if (ignore_eos)''')
text = replace(text, """        if (stop) break;
        if (i+1 < generate && evaluate_batch(m, &best, 1, logits)) status = 1;""", '''
        if (stop && !replay) break;
        if (i + 1 < generate) {
            unsigned next = best;
            if (replay && (fscanf(replay, "%u", &next) != 1 || next >= VOCAB)) status = 1;
            if (!status && evaluate_batch(m, &next, 1, logits)) status = 1;
        }''')
text = replace(text, "cleanup:\n    free(ids);", """cleanup:
    if (dump && fclose(dump) != 0) status = 1;
    if (replay) fclose(replay);
    free(ids);""")
quality.write_text(text)
