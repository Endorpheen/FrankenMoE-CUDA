"""Add grouped one-token attention to the frozen EXP-2000 CUDA host."""
from pathlib import Path
import sys

path, session, out = map(Path, sys.argv[1:])
src = path.read_text()


def replace(old, new):
    global src
    if src.count(old) != 1:
        raise SystemExit(f"expected one host anchor: {old!r}")
    src = src.replace(old, new)


replace('MMQ_K64_128, KERNEL_COUNT', 'MMQ_K64_128, DECODE_GQA2, DECODE_GQA3, KERNEL_COUNT')
replace('    unsigned context, position, ubatch, count, wave;',
        '    unsigned context, position, ubatch, count, wave;\n    int grouped_decode;')
replace('            const unsigned tasks = 24 * count;',
        '''            const unsigned tasks = 24 * count;
            if (m->grouped_decode && count == 1 && m->wave >= tasks) {
                unsigned keys = m->position + 1;
                unsigned n_tiles = tiles;
                unsigned heads = keys >= 32768 ? 3 : 2;
                if (launch(m, heads == 3 ? DECODE_GQA3 : DECODE_GQA2, 24 / heads, tiles, 1, 32,
                    (void *[]){ &p[0], &p[1], &p[2], &m->partial, &keys, &n_tiles, &scale })) return -1;
                return launch(m, MERGE, tasks, 1, 1, 32,
                    (void *[]){ &m->partial, &p[3], &m->attention, &start });
            }''')
replace('    CUdevice device;',
        '''    const char *grouped = getenv("BONSAI_DECODE_GQA");
    m->grouped_decode = !grouped || strcmp(grouped, "1") == 0;
    if (m->grouped_decode) fputs("Decode attention: grouped Q4 K/V\\n", stderr);
    CUdevice device;''')
replace('"bonsai_pq2_mmq_64x128_k64",',
        '"bonsai_pq2_mmq_64x128_k64", "bonsai_decode_gqa2", "bonsai_decode_gqa3",')
out.write_text(src + "\n" + session.read_text())
