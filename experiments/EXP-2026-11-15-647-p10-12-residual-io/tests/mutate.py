#!/usr/bin/env python3
# P10-12 mutation run: 12 mutations per slice, two halves of six.
#
#   python3 tests/mutate.py 1     # fr 6 + pr 6 + sn 6
#   python3 tests/mutate.py 2     # fr 6 + pr 6 + sn 6
#
# Each mutation rewrites one decision in one Goose module, regenerates that
# module at -O0, relinks its fixture and reruns the gate. A mutation that still
# passes is a hole in the gate. Builds are serial on purpose.
from pathlib import Path
import subprocess
import sys

B = Path(__file__).resolve().parent.parent
OUT = B / 'build'
WORK = OUT / 'mut'
GOOSE = '/home/end0/AI/LlamaServer/toolchain/goose-2026.3-string/goose'
REPO = Path('/home/end0/AI/FrankenMoE-CUDA')
GINC = Path('/home/end0/AI/LlamaServer/llama.cpp-flint-cuda/ggml/include')
CC = 'gcc-13'
CXX = 'g++-13'
SCRATCH = '/tmp/opencode/sub-648/scratch'

SRC = {
    'fr': B / 'source' / 'file_reader_windows.goose',
    'pr': B / 'source' / 'proc-readers.goose',
    'sn': B / 'source' / 'shard-naming.goose',
}
FIX = {
    'fr': B / 'tests' / 'fr_fixture.cpp',
    'pr': B / 'tests' / 'pr_fixture.cpp',
    'sn': B / 'tests' / 'sn_fixture.cpp',
}

# Half 1: the alignment defaults, the EOF clip, the window boundaries.
M1 = [
    ('fr', 'default alignment becomes 512',
     'const FR_ALIGN: i64 = 4096;', 'const FR_ALIGN: i64 = 512;'),
    ('fr', 'the lane clamp accepts zero lanes',
     '    if lanes < 1 {\n        return 1;\n    }', '    if lanes < 0 {\n        return 1;\n    }'),
    ('fr', 'the verify window may end past EOF',
     '    if voff + align > fsize {\n        voff = 0;\n    }',
     '    if voff + align >= fsize {\n        voff = 0;\n    }'),
    ('fr', 'the verify probes the first quarter',
     '    var voff: i64 = (fsize / 2) & fr_mask(align);',
     '    var voff: i64 = (fsize / 4) & fr_mask(align);'),
    ('fr', 'the plain clip ends one byte early',
     '    if fsize != 0 && off + nbytes > fsize {\n        end = fsize;\n    }',
     '    if fsize != 0 && off + nbytes > fsize {\n        end = fsize - 1;\n    }'),
    ('fr', 'the aligned window is not rounded up',
     '    let a1: i64 = (off + nbytes + align - 1) & m;',
     '    let a1: i64 = (off + nbytes) & m;'),

    # ── pr half 1 ─────────────────────────────────────────────────────────
    ('pr', 'the key no longer ends at a colon',
     '    if line[key.len] != 58 {', '    if line[key.len] != 32 {'),
    ('pr', 'the fgets line buffer grows to 511',
     '    while i < text.len && (i - pos) < 255 {', '    while i < text.len && (i - pos) < 511 {'),
    ('pr', 'a zero kB line locks the key',
     '        if v == 0 {\n            let r: i64 = pr_line_kb(text[pos..le], key);',
     '        if v != 0 {\n            let r: i64 = pr_line_kb(text[pos..le], key);'),
    ('pr', 'every digit is off by one',
     '        v = v * 10 + (text[at] - 48);', '        v = v * 10 + (text[at] - 47);'),
    ('pr', 'the u64 clamp is dropped',
     '    if over != 0 {\n        v = 0 - 1;\n    }', '    if over != 0 {\n        v = 0;\n    }'),
    ('pr', 'the maps pathname keeps its trailing spaces',
     '    while plen > 0 && (line[at + plen - 1] == 10 || line[at + plen - 1] == 32) {',
     '    while plen > 0 && line[at + plen - 1] == 10 {'),

    # ── sn half 1 ─────────────────────────────────────────────────────────
    ('sn', 'the shard tail is one byte short',
     'const SN_TAIL: i64 = 19;', 'const SN_TAIL: i64 = 18;'),
    ('sn', 'the separator may be an underscore',
     '    if path[tail - 1] != 45 {', '    if path[tail - 1] != 95 {'),
    ('sn', 'the digits accept one byte below ASCII 0',
     '        if p[i] < 48 || p[i] > 57 {', '        if p[i] < 47 || p[i] > 57 {'),
    ('sn', 'the split infix is upper case',
     '    if path[tail + 5..tail + 9] != "-of-" {', '    if path[tail + 5..tail + 9] != "-OF-" {'),
    ('sn', 'one shard is a split set',
     '    if count <= 1 {\n        return 0;\n    }', '    if count < 1 {\n        return 0;\n    }'),
    ('sn', 'the prefix keeps the separating dash',
     '    return tail - 1;\n}', '    return tail;\n}'),
]

# Half 2: the aligned boundaries, the residency window, the naming detail.
M2 = [
    ('fr', 'the verify runs in plain mode too',
     '    if aligned == 0 {\n        return 0 - 1;\n    }',
     '    if aligned == 1 {\n        return 0 - 1;\n    }'),
    ('fr', 'the verify boundary includes a one-block file',
     '    if fsize < align {\n        return 0 - 1;\n    }',
     '    if fsize <= align {\n        return 0 - 1;\n    }'),
    ('fr', 'the read end clip is one block too late',
     '    if fsize != 0 && a1 > fsize {\n        read_end = fsize;\n    }',
     '    if fsize != 0 && a1 > fsize + 4096 {\n        read_end = fsize;\n    }'),
    ('fr', 'the bulk end is not rounded down',
     '    let bulk_end: i64 = read_end & m;', '    let bulk_end: i64 = read_end;'),
    ('fr', 'read() reports the requested bytes, not the window',
     '    return read_end - a0;\n}\n\n// The exported form',
     '    return read_end - bulk_end;\n}\n\n// The exported form'),
    ('fr', 'the bounce grows on an exact fit',
     '    if cap < len {\n        return 1;\n    }', '    if cap <= len {\n        return 1;\n    }'),

    # ── pr half 2 ─────────────────────────────────────────────────────────
    ('pr', 'the maps name is matched by prefix',
     '        if line[at + plen - basename.len..at + plen] != basename {',
     '        if line[at..at + basename.len] != basename {'),
    ('pr', 'a pathname as long as the name is refused',
     '    if plen < basename.len {', '    if plen <= basename.len {'),
    ('pr', 'the residency window starts at the page base',
     '    let d0: i64 = (0 - off_in_page) & (page - 1);',
     '    let d0: i64 = page - off_in_page;'),
    ('pr', 'the residency window ignores the offset in the page',
     '    let d1: i64 = (off_in_page + sz) & mask;', '    let d1: i64 = sz & mask;'),
    ('pr', 'the chunking loses the partial 512-page call',
     '    return (pages + 511) / 512;', '    return pages / 512;'),
    ('pr', 'the dma-buf ceiling is reported as one page',
     '    if android != 0 {\n        return 2147483647;\n    }',
     '    if android != 0 {\n        return 4096;\n    }'),

    # ── sn half 2 ─────────────────────────────────────────────────────────
    ('sn', 'the shard number is parsed in base 2',
     '        v = v * 10 + (p[i] - 48);', '        v = v * 2 + (p[i] - 48);'),
    ('sn', 'every shard number is accepted',
     '    if no != 1 {\n        return 0 - 1;\n    }', '    if no != 0 {\n        return 0 - 1;\n    }'),
    ('sn', 'the shard field pads one zero short',
     '    var zeros: i64 = 5 - digits;', '    var zeros: i64 = 4 - digits;'),
    ('sn', 'the shard path drops the dash',
     '    at = sn_put05(out, at, no);\n    at = sn_putc(out, at, 45); // \'-\'\n    at = sn_putc(out, at, 111); // \'o\'',
     '    at = sn_put05(out, at, no);\n    at = sn_putc(out, at, 95); // \'_\'\n    at = sn_putc(out, at, 111); // \'o\''),
    ('sn', 'the shard path truncates its last byte',
     '    at = sn_putc(out, at, 102); // \'f\'\n    return at;',
     '    at = sn_putc(out, at, 102); // \'f\'\n    return at - 1;'),
    ('sn', 'the extension check reads four bytes',
     '    if path[tail + 14..tail + 19] != ".gguf" {',
     '    if path[tail + 14..tail + 18] != ".ggu" {'),
]


def compile_and_run(name, src, gen_c, gen_h, obj):
    r = subprocess.run([GOOSE, '--standalone', '-o', str(gen_c), '--header', str(gen_h), str(src)],
                       capture_output=True, text=True)
    if r.returncode != 0:
        return 'gen-fail', r.stderr
    r = subprocess.run([CC, '-O0', '-std=c11', '-w', '-c', str(gen_c), '-o', str(obj)],
                       capture_output=True, text=True)
    if r.returncode != 0:
        return 'cc-fail', r.stderr

    gate = WORK / ('gate-' + name)
    if name == 'fr':
        cmd = [CXX, '-O2', '-std=c++17', '-I', str(OUT), '-I', str(REPO / 'runtime/core/src/io'),
               str(FIX[name]), str(obj), str(OUT / 'native-file_reader.o'),
               str(OUT / 'native-platform_io.o'), '-o', str(gate)]
    elif name == 'pr':
        cmd = [CXX, '-O2', '-std=c++17', '-I', str(OUT), '-I', str(REPO / 'runtime/core/src/io'),
               str(FIX[name]), str(obj), '-o', str(gate)]
    else:
        cmd = [CXX, '-O2', '-std=c++17', '-ffunction-sections', '-fdata-sections', '-Wl,--gc-sections',
               '-I', str(OUT), '-I', str(REPO / 'runtime/core/src/moe'), '-I', str(GINC),
               str(FIX[name]), str(obj), '-o', str(gate)]
    r = subprocess.run(cmd, capture_output=True, text=True)
    if r.returncode != 0:
        return 'link-fail', r.stderr
    r = subprocess.run([str(gate)], capture_output=True, text=True,
                       env={'FG_SCRATCH': SCRATCH, 'PATH': '/usr/bin:/bin'})
    if r.returncode == 0:
        return 'survived', r.stdout
    return 'caught', r.stdout + r.stderr


def main():
    half = sys.argv[1] if len(sys.argv) > 1 else '1'
    muts = M1 if half == '1' else M2
    WORK.mkdir(parents=True, exist_ok=True)
    total = caught = 0
    for idx, (name, desc, old, new) in enumerate(muts):
        total += 1
        text = SRC[name].read_text()
        n = text.count(old)
        if n != 1:
            print('BROKEN  %-3s %-52s anchor occurs %d times' % (name, desc, n))
            continue
        mut = WORK / ('%s-%s.goose' % (name, idx))
        mut.write_text(text.replace(old, new))
        gen_c = WORK / ('%s-%s.c' % (name, idx))
        gen_h = WORK / ('%s-%s.h' % (name, idx))
        obj = WORK / ('%s-%s.o' % (name, idx))
        verdict, detail = compile_and_run(name, mut, gen_c, gen_h, obj)
        if verdict == 'caught':
            caught += 1
        print('%-7s %-3s %-52s %s' % (verdict, name, desc, '' if verdict == 'caught' else detail[-400:]))
    print('FG648_MUT half=%s caught=%d/%d' % (half, caught, total))
    return 0 if caught == total else 1


if __name__ == '__main__':
    sys.exit(main())
