#!/usr/bin/env python3
"""Hash-bound replay: copy /inputs/llama-kv-cache.cpp, verify sha256, insert
Goose dispatch guards at public method entry points, and record hashes.

The original body is preserved verbatim; when the cache is not Goose-attached
the hooks return 0 and the native body runs (default-on, native-disable
rollback). No layout or signature changes.
"""
import hashlib, json, os, sys

SRC = sys.argv[1] if len(sys.argv) > 1 else '/inputs/llama-kv-cache.cpp'
HDR = sys.argv[2] if len(sys.argv) > 2 else '/inputs/native/src/llama-kv-cache.h'
CELLS = sys.argv[3] if len(sys.argv) > 3 else '/inputs/native/src/llama-kv-cells.h'
OUT = 'patched/llama-kv-cache.cpp'
MAN = 'patched/hashes.json'

src = open(SRC, 'r').read()
h_src = hashlib.sha256(src.encode()).hexdigest()
h_hdr = hashlib.sha256(open(HDR, 'rb').read()).hexdigest()
h_cells = hashlib.sha256(open(CELLS, 'rb').read()).hexdigest()

EXPECTED = {
    'source': 'e31e5b246bf6d303995a57058458764c2a7f1296530db1c3333fb3ce301e8b29',
    'header': '94650dcfff9f37ec0ae60b0f2e744e35fc360377bb74f341fca6657c625ae1e1',
    'cells': '8611143cace09f163ce7b3c2493b80cfccd606c5be2745949309cbc823567958',
}
if h_src != EXPECTED['source']:
    raise SystemExit('llama-kv-cache.cpp hash mismatch: ' + h_src)
if h_hdr != EXPECTED['header']:
    raise SystemExit('llama-kv-cache.h hash mismatch: ' + h_hdr)
if h_cells != EXPECTED['cells']:
    raise SystemExit('llama-kv-cells.h hash mismatch: ' + h_cells)

INSERTS = [
    ('llama_memory_context_ptr llama_kv_cache::init_batch(\n            llama_batch_allocr & balloc,\n            uint32_t n_ubatch,\n            bool embd_all) {\n',
     '    if (void * g = fg_goose_init_batch(this, &balloc, n_ubatch, embd_all ? 1 : 0)) {\n        return llama_memory_context_ptr(static_cast<llama_memory_context_i *>(g));\n    }\n'),
    ('llama_memory_context_ptr llama_kv_cache::init_full() {\n',
     '    if (void * g = fg_goose_init_full(this)) {\n        return llama_memory_context_ptr(static_cast<llama_memory_context_i *>(g));\n    }\n'),
    ('llama_memory_context_ptr llama_kv_cache::init_update(llama_context * lctx, bool optimize) {\n',
     '    if (void * g = fg_goose_init_update(this, lctx)) {\n        return llama_memory_context_ptr(static_cast<llama_memory_context_i *>(g));\n    }\n'),
    ('bool llama_kv_cache::update(llama_context * lctx, bool do_shift, const stream_copy_info & sc_info) {\n',
     '    { int gu = 0; if (fg_goose_update(this, lctx, do_shift ? 1 : 0, &sc_info, &gu)) { return gu != 0; } }\n'),
    ('llama_kv_cache::slot_info_vec_t llama_kv_cache::prepare(const std::vector<llama_ubatch> & ubatches) {\n',
     '    if (void * g = fg_goose_prepare(this, ubatches.data(), ubatches.size())) {\n        auto * v = static_cast<slot_info_vec_t *>(g);\n        slot_info_vec_t r = std::move(*v);\n        delete v;\n        return r;\n    }\n'),
    ('llama_kv_cache::slot_info llama_kv_cache::find_slot(const llama_ubatch & ubatch, bool cont) const {\n',
     '    if (void * g = fg_goose_find_slot(const_cast<llama_kv_cache *>(this), &ubatch, cont ? 1 : 0)) {\n        auto * s = static_cast<slot_info *>(g);\n        slot_info r = std::move(*s);\n        delete s;\n        return r;\n    }\n'),
    ('void llama_kv_cache::apply_ubatch(const slot_info & sinfo, const llama_ubatch & ubatch) {\n',
     '    if (fg_goose_apply_ubatch(this, &sinfo, &ubatch)) { return; }\n'),
    ('bool llama_kv_cache::seq_rm(llama_seq_id seq_id, llama_pos p0, llama_pos p1) {\n',
     '    if (fg_goose_seq_rm(this, seq_id, p0, p1)) { return true; }\n'),
    ('void llama_kv_cache::seq_cp(llama_seq_id seq_id_src, llama_seq_id seq_id_dst, llama_pos p0, llama_pos p1) {\n',
     '    if (fg_goose_seq_cp(this, seq_id_src, seq_id_dst, p0, p1)) { return; }\n'),
    ('void llama_kv_cache::seq_keep(llama_seq_id seq_id) {\n',
     '    if (fg_goose_seq_keep(this, seq_id)) { return; }\n'),
    ('void llama_kv_cache::seq_add(llama_seq_id seq_id, llama_pos p0, llama_pos p1, llama_pos shift) {\n',
     '    if (fg_goose_seq_add(this, seq_id, p0, p1, shift)) { return; }\n'),
    ('void llama_kv_cache::seq_div(llama_seq_id seq_id, llama_pos p0, llama_pos p1, int d) {\n',
     '    if (fg_goose_seq_div(this, seq_id, p0, p1, d)) { return; }\n'),
    ('llama_pos llama_kv_cache::seq_pos_min(llama_seq_id seq_id) const {\n',
     '    { int gv = -1; if (fg_goose_seq_pos_min(const_cast<llama_kv_cache *>(this), seq_id, &gv)) { return gv; } }\n'),
    ('llama_pos llama_kv_cache::seq_pos_max(llama_seq_id seq_id) const {\n',
     '    { int gv = -1; if (fg_goose_seq_pos_max(const_cast<llama_kv_cache *>(this), seq_id, &gv)) { return gv; } }\n'),
    ('void llama_kv_cache::clear(bool data) {\n',
     '    if (fg_goose_clear(this, data ? 1 : 0)) { return; }\n'),
    ('void llama_kv_cache::state_write(llama_io_write_i & io, llama_seq_id seq_id, llama_state_seq_flags flags) const {\n',
     '    if (fg_goose_state_write(const_cast<llama_kv_cache *>(this), &io, seq_id, (int) flags)) { return; }\n'),
    ('void llama_kv_cache::state_read(llama_io_read_i & io, llama_seq_id seq_id, llama_state_seq_flags flags) {\n',
     '    if (fg_goose_state_read(this, &io, seq_id, (int) flags)) { return; }\n'),
    ('void llama_kv_cache::state_read_sinfo(\n        llama_io_read_i & io,\n           llama_seq_id   seq_id,\n  llama_state_seq_flags   flags,\n      slot_info_vec_t *   sinfos_out,\nconst slot_info_vec_t *   sinfos_in) {\n',
     '    if (fg_goose_state_read_sinfo(this, &io, seq_id, (int) flags, sinfos_out, sinfos_in)) { return; }\n'),
    ('void llama_kv_cache::state_clear(llama_seq_id seq_id) {\n',
     '    if (fg_goose_state_clear(this, seq_id)) { return; }\n'),
]

out = '#include "kv-goose-patch.h"\n' + src
used = set()
for sig, ins in INSERTS:
    if sig not in out:
        tmp = src
        if sig not in tmp:
            raise SystemExit('patch anchor not found: ' + sig.split('(')[0])
    out = out.replace(sig, sig + ins, 1)
    used.add(sig.split('(')[0])

os.makedirs('patched', exist_ok=True)
open(OUT, 'w').write(out)
man = {
    'source': SRC,
    'source_sha256': h_src,
    'header': HDR,
    'header_sha256': h_hdr,
    'cells_header': CELLS,
    'cells_sha256': h_cells,
    'patched': OUT,
    'patched_sha256': hashlib.sha256(out.encode()).hexdigest(),
    'patched_methods': sorted(used),
    'mode': 'default-on when attached; original body preserved as native-disable rollback',
}
json.dump(man, open(MAN, 'w'), indent=2)
print('patched', OUT, 'methods', len(used), 'sha', man['patched_sha256'][:16])
