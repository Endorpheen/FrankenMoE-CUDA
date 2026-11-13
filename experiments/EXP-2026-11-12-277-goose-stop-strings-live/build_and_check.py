#!/usr/bin/env python3
"""EXP277: wire the EXP-276 full-stop decision into find_stopping_strings() and rebuild the archive member."""
from pathlib import Path
import json, shlex, shutil, subprocess, sys

F = Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108')
FL = Path('/home/end0/AI/LlamaServer/llama.cpp-flint-cuda')
E = F / 'experiments/EXP-2026-11-12-277-goose-stop-strings-live'
I = E / 'integration'
I.mkdir(parents=True, exist_ok=True)
src = (FL / 'tools/server/server-context.cpp').read_text()

decls = '''
// Goose stop-string decision (EXP-276), full-stop path. Default on; GGML_GOOSE_STOP_STRINGS=0 restores the native loop.
extern "C" int     fg276t_enabled(void);
extern "C" int64_t fg276t_find_full(uint64_t text, int64_t len, uint64_t words, uint64_t offs, uint64_t lens,
                                    int64_t n_words, int64_t last_token_size, uint64_t out_word);
'''
anchor = 'struct server_slot {'
assert src.count(anchor) == 1
src = src.replace(anchor, decls + '\n' + anchor, 1)

old = '''        size_t stop_pos = std::string::npos;

        for (const std::string & word : task->params.antiprompt) {
            size_t pos;

            if (is_full_stop) {'''
new = '''        size_t stop_pos = std::string::npos;

        if (is_full_stop && fg276t_enabled() && !task->params.antiprompt.empty()) {
            std::string words;
            std::vector<int64_t> offs, lens;
            offs.reserve(task->params.antiprompt.size());
            lens.reserve(task->params.antiprompt.size());
            for (const std::string & w : task->params.antiprompt) {
                offs.push_back((int64_t) words.size());
                lens.push_back((int64_t) w.size());
                words += w;
            }
            int64_t word = -1;
            const int64_t pos = fg276t_find_full((uint64_t) (uintptr_t) text.data(), (int64_t) text.size(),
                                                 (uint64_t) (uintptr_t) words.data(),
                                                 (uint64_t) (uintptr_t) offs.data(), (uint64_t) (uintptr_t) lens.data(),
                                                 (int64_t) offs.size(), (int64_t) last_token_size,
                                                 (uint64_t) (uintptr_t) &word);
            if (pos >= 0 && word >= 0) {
                stop           = STOP_TYPE_WORD;
                stopping_word  = task->params.antiprompt[(size_t) word];
                has_next_token = false;
            }
            return pos >= 0 ? (size_t) pos : std::string::npos;
        }

        for (const std::string & word : task->params.antiprompt) {
            size_t pos;

            if (is_full_stop) {'''
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
