#!/usr/bin/env python3
"""EXP265: add the EXP-262 admission decision to the integrated cache copy and compile it."""
from pathlib import Path
import json, shlex, subprocess, sys

F = Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108')
FL = Path('/home/end0/AI/LlamaServer/llama.cpp-flint-cuda')
E = F / 'experiments/EXP-2026-11-12-265-moe-cache-admission-integration'
I = E / 'integration'
I.mkdir(parents=True, exist_ok=True)
prev = F / 'experiments/EXP-2026-11-12-264-moe-cache-copy-integration/integration/llama-moe-cache.cpp'
src = prev.read_text()

decls = '''
// Goose admission/grouping (EXP-262). Default on; GGML_GOOSE_MOE_CACHE_ADMIT=0 restores the native group loop.
extern "C" int      fg262a_enabled(void);
extern "C" uint8_t  fg262a_run(int64_t n_layers, uint64_t n_experts, uint64_t device, uint64_t layout, uint64_t nbytes,
                               uint64_t group, uint64_t group_dev, uint64_t group_layout, uint64_t group_bytes,
                               uint64_t group_count, uint64_t n_groups, int64_t max_groups);
'''
anchor = 'namespace {\n'
assert src.count(anchor) == 1
src = src.replace(anchor, decls + '\n' + anchor, 1)

old = '''            const int32_t id = (int32_t) (it_dev - devices.begin());
            auto it = std::find_if(groups.begin(), groups.end(), [&](const group & g) { return g.id == id && llama_moe_cache_same_layout(g.ref, experts); });
            if (it == groups.end()) {
                groups.emplace_back();
                it = groups.end() - 1;
                it->id  = id;
                it->ref = experts;
            }
            it->layers.push_back(il);
            for (const ggml_tensor * t : experts) {
                it->host_bytes         += ggml_nbytes(t);
                devices[id].host_bytes += ggml_nbytes(t);
            }
        }'''
new = '''            const int32_t id = (int32_t) (it_dev - devices.begin());
            if (fg262a_enabled()) {
                admitted_experts.push_back(experts);
                admitted_device.push_back(id);
                admitted_layer.push_back(il);
            } else {
                auto it = std::find_if(groups.begin(), groups.end(), [&](const group & g) { return g.id == id && llama_moe_cache_same_layout(g.ref, experts); });
                if (it == groups.end()) {
                    groups.emplace_back();
                    it = groups.end() - 1;
                    it->id  = id;
                    it->ref = experts;
                }
                it->layers.push_back(il);
                for (const ggml_tensor * t : experts) {
                    it->host_bytes         += ggml_nbytes(t);
                    devices[id].host_bytes += ggml_nbytes(t);
                }
            }
        }
        if (fg262a_enabled() && !admitted_layer.empty()) {
            // exact layout identities first: the Goose decision receives indices, not hashes, so it can never merge two
            // layouts the native comparison would keep apart.
            std::vector<std::vector<ggml_tensor *>> layouts;
            const size_t n = admitted_layer.size();
            std::vector<int64_t> ne(n), dev(n), lay(n), nby(n), grp(n, -1);
            for (size_t k = 0; k < n; ++k) {
                ne[k]  = (int64_t) admitted_experts[k].size();
                dev[k] = admitted_device[k];
                int64_t id = 0;
                for (size_t j = 0; j < layouts.size(); ++j) if (llama_moe_cache_same_layout(layouts[j], admitted_experts[k])) { id = (int64_t) j + 1; break; }
                if (id == 0) { layouts.push_back(admitted_experts[k]); id = (int64_t) layouts.size(); }
                lay[k] = id;
                for (const ggml_tensor * t : admitted_experts[k]) nby[k] += (int64_t) ggml_nbytes(t);
            }
            std::vector<int64_t> gdev(n, 0), glay(n, 0), gby(n, 0), gcnt(n, 0), ng(1, 0);
            const uint8_t ok = fg262a_run((int64_t) n,
                (uint64_t) (uintptr_t) ne.data(), (uint64_t) (uintptr_t) dev.data(), (uint64_t) (uintptr_t) lay.data(),
                (uint64_t) (uintptr_t) nby.data(), (uint64_t) (uintptr_t) grp.data(),
                (uint64_t) (uintptr_t) gdev.data(), (uint64_t) (uintptr_t) glay.data(),
                (uint64_t) (uintptr_t) gby.data(), (uint64_t) (uintptr_t) gcnt.data(),
                (uint64_t) (uintptr_t) ng.data(), (int64_t) n);
            if (!ok) {
                throw std::runtime_error("MoE cache admission plan refused");
            }
            groups.resize((size_t) ng[0]);
            for (int64_t g = 0; g < ng[0]; ++g) {
                groups[(size_t) g].id = (int32_t) gdev[(size_t) g];
            }
            for (size_t k = 0; k < n; ++k) {
                if (grp[k] < 0) continue;
                group & g = groups[(size_t) grp[k]];
                if (g.ref.empty()) g.ref = admitted_experts[k];
                g.layers.push_back(admitted_layer[k]);
                g.host_bytes += (size_t) nby[k];
                devices[g.id].host_bytes += (size_t) nby[k];
            }
        }'''
assert src.count(old) == 1
src = src.replace(old, new, 1)

# declare the per-layer staging vectors next to the other locals of the constructor
anchor2 = '        // only cache layers that keep all of their experts in host memory, on the device the layer is assigned to\n'
assert src.count(anchor2) == 1
src = src.replace(anchor2, anchor2 + '''        std::vector<std::vector<ggml_tensor *>> admitted_experts;
        std::vector<int32_t> admitted_device;
        std::vector<int32_t> admitted_layer;
''', 1)

(I / 'llama-moe-cache.cpp').write_text(src)

db = json.loads((FL / 'build-cuda/compile_commands.json').read_text())
row = [r for r in db if r['file'].endswith('/src/llama-moe-cache.cpp')][0]
cmd = shlex.split(row['command'])
cmd[cmd.index('-c') + 1] = str(I / 'llama-moe-cache.cpp')
cmd[cmd.index('-o') + 1] = str(E / 'moe-cache-goose.o')
q = subprocess.run(cmd, cwd=row['directory'], capture_output=True, text=True)
(E / 'compile.stderr').write_text(q.stderr)
print('compile rc =', q.returncode)
if q.returncode:
    print(q.stderr[-3000:]); sys.exit(1)


def undef(o):
    s = subprocess.run(['nm', '-u', str(o)], capture_output=True, text=True).stdout
    return {l.split()[-1] for l in s.splitlines() if l.strip()}


native = undef(FL / 'build-cuda/src/CMakeFiles/llama.dir/llama-moe-cache.cpp.o')
goose = undef(E / 'moe-cache-goose.o')
added = sorted(x for x in goose - native if x.startswith('fg'))
print('fg symbols added:', added)
(E / 'abi.json').write_text(json.dumps(dict(added=added), indent=2) + '\n')
