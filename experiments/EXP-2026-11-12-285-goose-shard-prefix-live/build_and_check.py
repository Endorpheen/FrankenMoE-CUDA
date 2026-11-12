#!/usr/bin/env python3
"""EXP285: wire the EXP-284 shard-prefix rule into a copy of src/llama.cpp and check the ABI delta."""
from pathlib import Path
import json, shlex, subprocess, sys

F = Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108')
FL = Path('/home/end0/AI/LlamaServer/llama.cpp-flint-cuda')
E = F / 'experiments/EXP-2026-11-12-285-goose-shard-prefix-live'
I = E / 'integration'
I.mkdir(parents=True, exist_ok=True)
src = (FL / 'src/llama.cpp').read_text()

decls = '''
// Goose multi-shard file-name rule (EXP-284). Default on; GGML_GOOSE_SHARD_PREFIX=0 restores the native check.
extern "C" int     fg284p_enabled(void);
extern "C" int64_t fg284p_split_prefix(uint64_t path, int64_t len, int64_t split_no, int64_t split_count);
'''
anchor = 'int32_t llama_split_prefix('
assert src.count(anchor) == 1
src = src.replace(anchor, decls + '\n' + anchor, 1)

old = '''    const std::string str_split_path(split_path);

    char postfix[32];'''
new = '''    const std::string str_split_path(split_path);

    if (fg284p_enabled()) {
        const int64_t prefix_len = fg284p_split_prefix((uint64_t) (uintptr_t) split_path,
                                                       (int64_t) str_split_path.size(), split_no, split_count);
        if (prefix_len < 0) {
            return 0;
        }
        const size_t copy_len = std::min((size_t) prefix_len + 1, maxlen);
        snprintf(split_prefix, copy_len, "%s", split_path);
        return (int32_t) prefix_len;
    }

    char postfix[32];'''
assert src.count(old) == 1
src = src.replace(old, new, 1)
(I / 'llama.cpp').write_text(src)

db = json.loads((FL / 'build-cuda/compile_commands.json').read_text())
row = [r for r in db if r['file'].endswith('/src/llama.cpp')][0]
cmd = shlex.split(row['command'])
cmd[cmd.index('-c') + 1] = str(I / 'llama.cpp')
cmd[cmd.index('-o') + 1] = str(E / 'llama-goose.o')
q = subprocess.run(cmd, cwd=row['directory'], capture_output=True, text=True)
(E / 'compile.stderr').write_text(q.stderr)
print('compile rc =', q.returncode)
if q.returncode:
    print(q.stderr[-2500:]); sys.exit(1)

def undef(o):
    s = subprocess.run(['nm', '-u', str(o)], capture_output=True, text=True).stdout
    return {l.split()[-1] for l in s.splitlines() if l.strip()}
native = undef(FL / 'build-cuda/src/CMakeFiles/llama.dir/llama.cpp.o')
goose = undef(E / 'llama-goose.o')
added = sorted(x for x in goose - native if x.startswith('fg'))
print('fg symbols added:', added)
(E / 'abi.json').write_text(json.dumps(dict(added=added), indent=2) + '\n')
