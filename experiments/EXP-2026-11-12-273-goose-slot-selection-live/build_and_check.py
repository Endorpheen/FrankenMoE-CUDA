#!/usr/bin/env python3
"""EXP273: wire the EXP-272 slot-selection decisions into server-context.cpp and rebuild the archive member."""
from pathlib import Path
import json, shlex, shutil, subprocess, sys

F = Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108')
FL = Path('/home/end0/AI/LlamaServer/llama.cpp-flint-cuda')
E = F / 'experiments/EXP-2026-11-12-273-goose-slot-selection-live'
I = E / 'integration'
I.mkdir(parents=True, exist_ok=True)
src = (FL / 'tools/server/server-context.cpp').read_text()

decls = '''
// Goose slot-selection decisions (EXP-272). Default on; GGML_GOOSE_SLOT_SELECT=0 restores the native logic.
extern "C" int     fg272s_enabled(void);
extern "C" int64_t fg272s_pick_similar(uint64_t eligible, uint64_t lcp, int64_t count, int64_t n_tokens, int64_t thr_bits);
extern "C" int64_t fg272s_pick_lru(uint64_t idle, uint64_t empty, uint64_t t_last, int64_t count);
static inline int64_t fg_slot_bits(float f) { int32_t b; memcpy(&b, &f, 4); return (int64_t) b; }
'''
anchor = 'struct server_context_impl {'
if src.count(anchor) != 1:
    anchor = 'struct server_context {'
assert src.count(anchor) == 1
src = src.replace(anchor, decls + '\n' + anchor, 1)

# --- 1. similarity loop inside get_available_slot
old1 = '''            for (server_slot & slot : slots) {
                if (task.id_slot != -1 && slot.id != task.id_slot) {
                    continue;
                }'''
new1 = '''            if (fg272s_enabled()) {
                const size_t nsl = slots.size();
                std::vector<int64_t> s_ok(nsl, 0), s_lcp(nsl, 0);
                for (size_t k = 0; k < nsl; ++k) {
                    server_slot & slot = slots[k];
                    if (task.id_slot != -1 && slot.id != task.id_slot) continue;
                    if (slot.is_processing()) continue;
                    const auto & tokens = slot.prompt.tokens;
                    if (tokens.empty()) continue;
                    s_ok[k]  = 1;
                    s_lcp[k] = (int64_t) tokens.get_common_prefix(task.tokens);
                }
                const int64_t idx = fg272s_pick_similar((uint64_t) (uintptr_t) s_ok.data(),
                                                        (uint64_t) (uintptr_t) s_lcp.data(),
                                                        (int64_t) nsl, (int64_t) task.tokens.size(),
                                                        fg_slot_bits(slot_prompt_similarity));
                if (idx >= 0) {
                    ret = &slots[(size_t) idx];
                    f_sim_best = float(s_lcp[(size_t) idx]) / float(task.tokens.size());
                }
            }
            for (server_slot & slot : slots) {
                if (fg272s_enabled()) {
                    break;
                }
                if (task.id_slot != -1 && slot.id != task.id_slot) {
                    continue;
                }'''
assert src.count(old1) == 1
src = src.replace(old1, new1, 1)

# --- 2. single free slot
old2 = '''    std::vector<server_slot *> get_free_slots(size_t n_slots_needed, int exclude_id_slot) {
        std::vector<server_slot *> free_slots;'''
new2 = '''    std::vector<server_slot *> get_free_slots(size_t n_slots_needed, int exclude_id_slot) {
        std::vector<server_slot *> free_slots;
        if (fg272s_enabled() && n_slots_needed == 1 && exclude_id_slot == -1) {
            const size_t nsl = slots.size();
            std::vector<int64_t> g_idle(nsl, 0), g_empty(nsl, 0), g_tl(nsl, 0);
            for (size_t k = 0; k < nsl; ++k) {
                server_slot & slot = slots[k];
                g_idle[k]  = (!slot.is_processing() && slot.id != exclude_id_slot) ? 1 : 0;
                g_empty[k] = slot.prompt.n_tokens() == 0 ? 1 : 0;
                g_tl[k]    = slot.t_last_used;
            }
            const int64_t idx = fg272s_pick_lru((uint64_t) (uintptr_t) g_idle.data(),
                                                (uint64_t) (uintptr_t) g_empty.data(),
                                                (uint64_t) (uintptr_t) g_tl.data(), (int64_t) nsl);
            if (idx >= 0) {
                free_slots.push_back(&slots[(size_t) idx]);
            }
            return free_slots;
        }'''
assert src.count(old2) == 1
src = src.replace(old2, new2, 1)

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

src_ar = FL / 'build-cuda/tools/server/libserver-context.a'
dst_ar = E / 'libserver-context.a'
shutil.copy2(src_ar, dst_ar)
for a in (['ar', 'd', str(dst_ar), 'server-context.cpp.o'], ['ar', 'r', str(dst_ar), str(E / 'server-context-goose.o')]):
    r = subprocess.run(a, capture_output=True, text=True)
    print(a[1], r.returncode, r.stderr.strip()[:150])

def undef(o):
    s = subprocess.run(['nm', '-u', str(o)], capture_output=True, text=True).stdout
    return {l.split()[-1] for l in s.splitlines() if l.strip()}
added = sorted(x for x in (undef(E / 'server-context-goose.o') - undef(FL / 'build-cuda/tools/server/CMakeFiles/server-context.dir/server-context.cpp.o')) if x.startswith('fg'))
print('fg symbols added:', added)
(E / 'abi.json').write_text(json.dumps(dict(added=added), indent=2) + '\n')
