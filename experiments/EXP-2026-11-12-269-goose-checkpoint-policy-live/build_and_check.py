#!/usr/bin/env python3
"""EXP269: integrate the EXP-268 checkpoint policy into create_checkpoint() and compile the object."""
from pathlib import Path
import json, shlex, subprocess, sys

F = Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108')
FL = Path('/home/end0/AI/LlamaServer/llama.cpp-flint-cuda')
E = F / 'experiments/EXP-2026-11-12-269-goose-checkpoint-policy-live'
I = E / 'integration'
I.mkdir(parents=True, exist_ok=True)
src = (FL / 'tools/server/server-context.cpp').read_text()

decls = '''
// Goose context-checkpoint policy (EXP-268). Default on; GGML_GOOSE_CTX_CHECKPOINTS=0 restores the native loops.
extern "C" int     fg268p_enabled(void);
extern "C" int64_t fg268p_near_evict(uint64_t tokens, uint64_t task, int64_t count, int64_t id_task,
                                     int64_t min_step, int64_t limit, uint64_t out_mask);
extern "C" int64_t fg268p_make_room(int64_t count, int64_t limit);
extern "C" int64_t fg268p_supersede(uint64_t tokens, int64_t count, int64_t n_tokens_new, uint64_t out_mask);
'''
anchor = 'struct server_context_impl {'
if src.count(anchor) != 1:
    anchor = 'struct server_context {'
assert src.count(anchor) == 1, 'anchor'
src = src.replace(anchor, decls + '\n' + anchor, 1)

# --- 1. near-eviction
old1 = '''        int64_t last = -1;
        for (auto it = slot.prompt.checkpoints.begin();
                slot.prompt.checkpoints.size() + 1 >= (size_t) params_base.n_ctx_checkpoints &&
                it != slot.prompt.checkpoints.end(); ) {
            if (it->id_task != id_task && last >= 0 && it->n_tokens <= last + params_base.checkpoint_min_step) {'''
new1 = '''        int64_t last = -1;
        if (fg268p_enabled()) {
            std::vector<int64_t> ck_tok, ck_task;
            ck_tok.reserve(slot.prompt.checkpoints.size());
            ck_task.reserve(slot.prompt.checkpoints.size());
            for (const auto & c : slot.prompt.checkpoints) { ck_tok.push_back(c.n_tokens); ck_task.push_back(c.id_task); }
            std::vector<uint64_t> ck_mask(ck_tok.size()/64 + 1, 0);
            fg268p_near_evict((uint64_t) (uintptr_t) ck_tok.data(), (uint64_t) (uintptr_t) ck_task.data(),
                              (int64_t) ck_tok.size(), id_task, params_base.checkpoint_min_step,
                              params_base.n_ctx_checkpoints, (uint64_t) (uintptr_t) ck_mask.data());
            size_t ck_idx = 0;
            for (auto it = slot.prompt.checkpoints.begin(); it != slot.prompt.checkpoints.end(); ) {
                if ((ck_mask[ck_idx/64] >> (ck_idx%64)) & 1) { it = slot.prompt.checkpoints.erase(it); }
                else { ++it; }
                ++ck_idx;
            }
        }
        for (auto it = slot.prompt.checkpoints.begin();
                !fg268p_enabled() &&
                slot.prompt.checkpoints.size() + 1 >= (size_t) params_base.n_ctx_checkpoints &&
                it != slot.prompt.checkpoints.end(); ) {
            if (it->id_task != id_task && last >= 0 && it->n_tokens <= last + params_base.checkpoint_min_step) {'''
assert src.count(old1) == 1, 'near block'
src = src.replace(old1, new1, 1)

# --- 2. make-room
old2 = '''        while (slot.prompt.checkpoints.size() >= (size_t) params_base.n_ctx_checkpoints) {
            // make room for the new checkpoint, if needed
            const auto & cur = slot.prompt.checkpoints.front();'''
new2 = '''        if (fg268p_enabled()) {
            const int64_t n_room = fg268p_make_room((int64_t) slot.prompt.checkpoints.size(),
                                                    params_base.n_ctx_checkpoints);
            for (int64_t k = 0; k < n_room && !slot.prompt.checkpoints.empty(); ++k) {
                const auto & cur = slot.prompt.checkpoints.front();
                SLT_WRN(slot, "erasing old context checkpoint (pos_min = %d, pos_max = %d, n_tokens = %" PRId64 ", size = %.3f MiB)\\n",
                        cur.pos_min, cur.pos_max, cur.n_tokens, (float) cur.size() / 1024 / 1024);
                slot.prompt.checkpoints.erase(slot.prompt.checkpoints.begin());
            }
        }
        while (!fg268p_enabled() && slot.prompt.checkpoints.size() >= (size_t) params_base.n_ctx_checkpoints) {
            // make room for the new checkpoint, if needed
            const auto & cur = slot.prompt.checkpoints.front();'''
assert src.count(old2) == 1, 'room block'
src = src.replace(old2, new2, 1)

# --- 3. supersede
old3 = '''            const int64_t n_tokens_new = slot.prompt.n_tokens() - n_tokens_cur;
            for (auto it = slot.prompt.checkpoints.begin(); it != slot.prompt.checkpoints.end(); ) {
                if (it->n_tokens == n_tokens_new) {'''
new3 = '''            const int64_t n_tokens_new = slot.prompt.n_tokens() - n_tokens_cur;
            if (fg268p_enabled()) {
                std::vector<int64_t> ck_tok;
                ck_tok.reserve(slot.prompt.checkpoints.size());
                for (const auto & c : slot.prompt.checkpoints) ck_tok.push_back(c.n_tokens);
                std::vector<uint64_t> ck_mask(ck_tok.size()/64 + 1, 0);
                fg268p_supersede((uint64_t) (uintptr_t) ck_tok.data(), (int64_t) ck_tok.size(), n_tokens_new,
                                 (uint64_t) (uintptr_t) ck_mask.data());
                size_t ck_idx = 0;
                for (auto it = slot.prompt.checkpoints.begin(); it != slot.prompt.checkpoints.end(); ) {
                    if ((ck_mask[ck_idx/64] >> (ck_idx%64)) & 1) {
                        SLT_TRC(slot, "superseding context checkpoint at n_tokens = %" PRId64 "\\n", it->n_tokens);
                        it = slot.prompt.checkpoints.erase(it);
                    } else { ++it; }
                    ++ck_idx;
                }
            }
            for (auto it = slot.prompt.checkpoints.begin(); !fg268p_enabled() && it != slot.prompt.checkpoints.end(); ) {
                if (it->n_tokens == n_tokens_new) {'''
assert src.count(old3) == 1, 'supersede block'
src = src.replace(old3, new3, 1)

(I / 'server-context.cpp').write_text(src)

db = json.loads((FL / 'build-cuda/compile_commands.json').read_text())
row = [r for r in db if r['file'].endswith('/tools/server/server-context.cpp')][0]
cmd = shlex.split(row['command'])
cmd[cmd.index('-c') + 1] = str(I / 'server-context.cpp')
cmd[cmd.index('-o') + 1] = str(E / 'server-context-goose.o')
q = subprocess.run(cmd, cwd=row['directory'], capture_output=True, text=True)
(E / 'compile.stderr').write_text(q.stderr)
print('compile rc =', q.returncode)
if q.returncode:
    print(q.stderr[-3000:]); sys.exit(1)

def undef(o):
    s = subprocess.run(['nm', '-u', str(o)], capture_output=True, text=True).stdout
    return {l.split()[-1] for l in s.splitlines() if l.strip()}

native = undef(FL / 'build-cuda/tools/server/CMakeFiles/server-context.dir/server-context.cpp.o')
goose = undef(E / 'server-context-goose.o')
added = sorted(x for x in goose - native if x.startswith('fg'))
print('fg symbols added:', added)
(E / 'abi.json').write_text(json.dumps(dict(added=added), indent=2) + '\n')
