#!/usr/bin/env python3
"""EXP271: integrate the EXP-270 queue decisions into server-queue.cpp and rebuild the archive member."""
from pathlib import Path
import json, shlex, shutil, subprocess, sys

F = Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108')
FL = Path('/home/end0/AI/LlamaServer/llama.cpp-flint-cuda')
E = F / 'experiments/EXP-2026-11-12-271-goose-queue-live'
I = E / 'integration'
I.mkdir(parents=True, exist_ok=True)
src = (FL / 'tools/server/server-queue.cpp').read_text()

decls = '''
// Goose queue decisions (EXP-270). Default on; GGML_GOOSE_SERVER_QUEUE=0 restores the native logic.
extern "C" int     fg270q_enabled(void);
extern "C" int64_t fg270q_pick_deferred(uint64_t slot_of, int64_t count, int64_t id_slot);
extern "C" int64_t fg270q_cleanup_mask(uint64_t task_ids, int64_t count, int64_t id_target, uint64_t out_mask);
'''
anchor = 'static bool task_resets_idle_timer(server_task_type type) {'
assert src.count(anchor) == 1
src = src.replace(anchor, decls + '\n' + anchor, 1)

old_pop = '''    if (!queue_tasks_deferred.empty()) {
        // try to find a task that uses the specified slot
        bool found = false;'''
new_pop = '''    if (fg270q_enabled() && !queue_tasks_deferred.empty()) {
        std::vector<int64_t> slots;
        slots.reserve(queue_tasks_deferred.size());
        for (const auto & t : queue_tasks_deferred) slots.push_back(t.id_slot);
        const int64_t idx = fg270q_pick_deferred((uint64_t) (uintptr_t) slots.data(),
                                                 (int64_t) slots.size(), id_slot);
        if (idx >= 0) {
            auto it = queue_tasks_deferred.begin();
            std::advance(it, (size_t) idx);
            QUE_DBG("pop deferred task (goose), id_task = %d, slot = %d\\n", it->id, id_slot);
            queue_tasks.emplace_front(std::move(*it));
            queue_tasks_deferred.erase(it);
        }
    }
    if (!fg270q_enabled() && !queue_tasks_deferred.empty()) {
        // try to find a task that uses the specified slot
        bool found = false;'''
assert src.count(old_pop) == 1
src = src.replace(old_pop, new_pop, 1)

old_clean = '''void server_queue::cleanup_pending_task(int id_target) {
    // no need lock because this is called exclusively by post()
    auto rm_func = [id_target](const server_task & task) {'''
new_clean = '''void server_queue::cleanup_pending_task(int id_target) {
    // no need lock because this is called exclusively by post()
    if (fg270q_enabled()) {
        auto clean = [id_target](std::deque<server_task> & q) {
            if (q.empty()) return;
            std::vector<int64_t> ids;
            ids.reserve(q.size());
            for (const auto & t : q) ids.push_back(t.id);
            std::vector<uint64_t> mask(ids.size()/64 + 1, 0);
            fg270q_cleanup_mask((uint64_t) (uintptr_t) ids.data(), (int64_t) ids.size(), id_target,
                                (uint64_t) (uintptr_t) mask.data());
            size_t i = 0;
            for (auto it = q.begin(); it != q.end(); ) {
                if ((mask[i/64] >> (i%64)) & 1) { it = q.erase(it); } else { ++it; }
                ++i;
            }
        };
        clean(queue_tasks);
        clean(queue_tasks_deferred);
        clean(queue_tasks_unhandled);
        return;
    }
    auto rm_func = [id_target](const server_task & task) {'''
assert src.count(old_clean) == 1
src = src.replace(old_clean, new_clean, 1)

(I / 'server-queue.cpp').write_text(src)

db = json.loads((FL / 'build-cuda/compile_commands.json').read_text())
row = [r for r in db if r['file'].endswith('/tools/server/server-queue.cpp')][0]
cmd = shlex.split(row['command'])
cmd[cmd.index('-c') + 1] = str(I / 'server-queue.cpp')
cmd[cmd.index('-o') + 1] = str(E / 'server-queue-goose.o')
q = subprocess.run(cmd, cwd=row['directory'], capture_output=True, text=True)
(E / 'compile.stderr').write_text(q.stderr)
print('compile rc =', q.returncode)
if q.returncode:
    print(q.stderr[-3000:]); sys.exit(1)

def undef(o):
    s = subprocess.run(['nm', '-u', str(o)], capture_output=True, text=True).stdout
    return {l.split()[-1] for l in s.splitlines() if l.strip()}

native = undef(FL / 'build-cuda/tools/server/CMakeFiles/server-context.dir/server-queue.cpp.o')
goose = undef(E / 'server-queue-goose.o')
added = sorted(x for x in goose - native if x.startswith('fg'))
print('fg symbols added:', added)

# archive with our member substituted
src_ar = FL / 'build-cuda/tools/server/libserver-context.a'
dst_ar = E / 'libserver-context.a'
shutil.copy2(src_ar, dst_ar)
for a in (['ar', 'd', str(dst_ar), 'server-queue.cpp.o'], ['ar', 'r', str(dst_ar), str(E / 'server-queue-goose.o')]):
    r = subprocess.run(a, capture_output=True, text=True)
    print(a[1], r.returncode, r.stderr.strip()[:150])

# bridge
(E / 'build' if False else E).mkdir(exist_ok=True)
print('prepared')
