#!/usr/bin/env python3
"""EXP275: wire the EXP-274 batch-compatibility predicate into server-context.cpp and compile."""
from pathlib import Path
import json, shlex, shutil, subprocess, sys

F = Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108')
FL = Path('/home/end0/AI/LlamaServer/llama.cpp-flint-cuda')
E = F / 'experiments/EXP-2026-11-12-275-goose-batch-compat-live'
I = E / 'integration'
I.mkdir(parents=True, exist_ok=True)
src = (FL / 'tools/server/server-context.cpp').read_text()

decls = '''
// Goose batch-compatibility predicate (EXP-274). Default on; GGML_GOOSE_BATCH_COMPAT=0 restores the native rule.
extern "C" int     fg274b_enabled(void);
extern "C" uint8_t fg274b_can_batch(int64_t type_a, int64_t type_b, int64_t embd_a, int64_t embd_b,
                                    int64_t dec_a, int64_t dec_b, uint64_t ptr_a, uint64_t sc_a, int64_t n_a,
                                    uint64_t ptr_b, uint64_t sc_b, int64_t n_b);
'''
anchor = 'struct server_slot {'
assert src.count(anchor) == 1
src = src.replace(anchor, decls + '\n' + anchor, 1)

old = '''    bool can_batch_with(server_slot & other_slot) const {
        GGML_ASSERT(task);

        // a joint decision head reads the whole batch
        if (!task->decision.order.empty() || !other_slot.task->decision.order.empty()) {
            return false;
        }

        return task->type == other_slot.task->type
            && inp_embd.size() == other_slot.inp_embd.size()
            && are_lora_equal(lora, other_slot.lora);
    }'''
new = '''    bool can_batch_with(server_slot & other_slot) const {
        GGML_ASSERT(task);

        if (fg274b_enabled()) {
            auto flatten = [](const std::vector<common_adapter_lora_info> & ls,
                              std::vector<int64_t> & ptrs, std::vector<uint64_t> & scales) {
                ptrs.reserve(ls.size()); scales.reserve(ls.size());
                for (const auto & l : ls) {
                    uint32_t b; float s = l.scale; memcpy(&b, &s, 4);
                    ptrs.push_back((int64_t) (uintptr_t) l.ptr);
                    scales.push_back((uint64_t) b);
                }
            };
            std::vector<int64_t> pa, pb;
            std::vector<uint64_t> sa, sb;
            flatten(lora, pa, sa);
            flatten(other_slot.lora, pb, sb);
            return fg274b_can_batch((int64_t) task->type, (int64_t) other_slot.task->type,
                                    (int64_t) inp_embd.size(), (int64_t) other_slot.inp_embd.size(),
                                    task->decision.order.empty() ? 0 : 1,
                                    other_slot.task->decision.order.empty() ? 0 : 1,
                                    (uint64_t) (uintptr_t) pa.data(), (uint64_t) (uintptr_t) sa.data(), (int64_t) pa.size(),
                                    (uint64_t) (uintptr_t) pb.data(), (uint64_t) (uintptr_t) sb.data(), (int64_t) pb.size()) != 0;
        }

        // a joint decision head reads the whole batch
        if (!task->decision.order.empty() || !other_slot.task->decision.order.empty()) {
            return false;
        }

        return task->type == other_slot.task->type
            && inp_embd.size() == other_slot.inp_embd.size()
            && are_lora_equal(lora, other_slot.lora);
    }'''
assert src.count(old) == 1
src = src.replace(old, new, 1)

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
