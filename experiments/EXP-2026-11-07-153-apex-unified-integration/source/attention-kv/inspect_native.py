import subprocess, sys, os

def run(argv):
    try:
        p = subprocess.run(argv, capture_output=True, text=True, timeout=120)
        return p.returncode, p.stdout, p.stderr
    except Exception as e:
        return 127, '', repr(e)

print('--- compiler version ---')
rc, out, err = run(['/compiler', '--version'])
print('rc', rc)
print(out[:2000])
print(err[:2000])

for lib in ['/runtime/libllama.so',
            '/runtime/libllama.so.0.7.0',
            '/runtime/libllama-apex-goose.so.0.7.0',
            '/runtime/libllama-apex-hybrid-goose.so.0.7.0']:
    if not os.path.exists(lib):
        print('missing', lib)
        continue
    print('--- symbols', lib, '---')
    rc, out, err = run(['/usr/bin/nm', '-D', '--defined-only', lib])
    if rc != 0:
        print('nm rc', rc, err[:500])
        continue
    keys = ['llama_kv_cache', 'llama_kv_cells', 'llama_memory', 'llama_kv_cache_context']
    shown = 0
    for line in out.splitlines():
        if any(k in line for k in keys):
            print(line)
            shown += 1
            if shown > 120:
                print('... truncated')
                break
print('inspect done')
