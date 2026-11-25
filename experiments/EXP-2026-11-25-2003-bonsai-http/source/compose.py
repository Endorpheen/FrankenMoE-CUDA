"""Connect the Bonsai adapter to the frozen common HTTP model host."""
import sys
from pathlib import Path

host = Path(sys.argv[1]).read_text()


def replace(old, new):
    global host
    if host.count(old) != 1:
        raise SystemExit(f"expected one host anchor: {old!r}")
    host = host.replace(old, new)


replace('#include "inkling-backend.h"', '''#include "inkling-backend.h"
#include "session.h"

extern __typeof__(gr_parse) bonsai_gr_parse;
extern void bonsai_goose_init(int argc, char **argv);''')
replace('    struct decoder *inkling;', '''    struct decoder *inkling;
    struct bonsai_session *bonsai;
    bool bonsai_layout;''')
replace('int64_t fg_q35_generation_prepare(void) {', '''int64_t fg_q35_generation_prepare(void) {
    if (H.bonsai) return 0;''')
start = host.index('    if (gr_parse(')
end = host.index('    H.inkling_tokenizer', start)
host = host[:start] + '''    const sl_u8 file = {H.base, hdr};
    const sl_i64 state_slice = {H.state, 8};
    const sl_i64 metadata = {H.kv, 256 * 8};
    const sl_i64 tensors = {H.tens, 2048 * 12};
    const sl_i64 metadata_slots = {H.kvsl, 512};
    const sl_i64 tensor_slots = {H.tensl, 8192};
    if (gr_parse(file, H.size, state_slice, metadata, tensors, metadata_slots, tensor_slots) != 0) {
        memset(H.state, 0, sizeof H.state);
        memset(H.kv, 0, (size_t)metadata.len * sizeof(int64_t));
        memset(H.tens, 0, (size_t)tensors.len * sizeof(int64_t));
        memset(H.kvsl, 0, (size_t)metadata_slots.len * sizeof(int64_t));
        memset(H.tensl, 0, (size_t)tensor_slots.len * sizeof(int64_t));
        bonsai_goose_init(0, NULL);
        if (bonsai_gr_parse(file, H.size, state_slice, metadata, tensors, metadata_slots, tensor_slots) != 0) {
            die("reader");
        }
        H.bonsai_layout = true;
        if (!metadata_is("general.architecture", "qwen35")) die("Bonsai architecture");
    }
''' + host[end:]
replace('    load_vocab();\n    if (metadata_is', '''    load_vocab();
    if (H.bonsai_layout) {
        if (capacity <= 0 || capacity > 262144 || prefill_ubatch <= 0 ||
                prefill_ubatch > 1024 || !kv_compatible(HOST_KV_Q4_0) ||
                H.n_tok != BONSAI_SESSION_VOCAB) die("Bonsai context, ubatch, KV or vocabulary");
        const char *image = getenv("BONSAI_CUDA_IMAGE");
        const char *frozen = getenv("BONSAI_FROZEN_PTX");
        if (!image || !frozen) die("Bonsai requires BONSAI_CUDA_IMAGE and BONSAI_FROZEN_PTX");
        H.bonsai = bonsai_session_open(path, image, frozen, (unsigned)capacity, (unsigned)prefill_ubatch);
        if (!H.bonsai) die("Bonsai model or CUDA placement");
        H.kv_format = HOST_KV_Q4_0;
        H.owner[10] = capacity;
        H.layer_count = 64;
        return;
    }
    if (metadata_is''')
replace('void hose_reset_state(void) {', '''void hose_reset_state(void) {
    if (H.bonsai) {
        if (bonsai_session_reset(H.bonsai) != 0) die("Bonsai model state reset");
        return;
    }''')
replace('    *logits = NULL;\n    if (H.inkling', '''    *logits = NULL;
    if (H.bonsai) {
        if (count != 1) return -1;
        float *row = malloc((size_t)H.n_tok * sizeof(*row));
        if (!row) return -1;
        if (bonsai_session_step(H.bonsai, (const uint32_t *)tokens, 1, row) != 0) {
            free(row);
            return -1;
        }
        *logits = row;
        return H.n_tok;
    }
    if (H.inkling''')
replace('profile.data[2] = !H.inkling &&', 'profile.data[2] = !H.bonsai && !H.inkling &&')
for name in ('enter', 'leave'):
    replace(f'void fg_q35_generation_scope_{name}(void) {{\n    if (H.inkling) return;',
            f'void fg_q35_generation_scope_{name}(void) {{\n    if (H.bonsai || H.inkling) return;')
replace('    int64_t result;\n    if (H.inkling) {', '''    int64_t result;
    if (H.bonsai) {
        float *row = output.len == H.n_tok ? (float *)(exchange + token_bytes) : NULL;
        const int status = (row || !output.len) && tokens.len <= UINT32_MAX ?
            bonsai_session_step(H.bonsai, (const uint32_t *)exchange, (unsigned)tokens.len, row) : -1;
        result = status == 0 ? output.len : -1;
    } else if (H.inkling) {''')
Path(sys.argv[2]).write_text(host)
