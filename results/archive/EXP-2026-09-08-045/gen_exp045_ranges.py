#!/usr/bin/env python3
"""EXP-2026-09-08-045: rebuild the gather workload from the saved EXP-032 H2D trace.

Input trace (LOCAL_ONLY, sha256 3267cbeaeec4f25fd328e88db8b882f73b2b307ad0913d452b6aa56577b55945):
    results/archive/EXP-2026-09-06-034/raw/exp032-h2d-ts/h2d.log

Each trace line carries a CUDA buffer tensor name (CUDA0#<tensor>#<idx>), the offset inside the
tensor and the byte count. The absolute file address is reconstructed from the GGUF metadata of
the model shards (gguf-py reports absolute data offsets per shard file), so the prototype can
mmap the very same weight pages the server reads, without running the model.

Output: one range per line  "<shard_index> <absolute_file_offset> <bytes>".
"""
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))
sys.path.insert(0, os.path.join(ROOT, 'work/llama.cpp-exp041/gguf-py'))
import gguf  # noqa: E402

SHARDS = [
    os.path.join(ROOT, 'models/qwen38/UD-IQ3_XXS/Qwen3.8-Flash-Next-UD-IQ3_XXS-0000%d-of-00003.gguf' % i)
    for i in (1, 2, 3)
]
LOG = os.path.join(ROOT, 'results/archive/EXP-2026-09-06-034/raw/exp032-h2d-ts/h2d.log')
OUT = os.path.join(ROOT, 'results/archive/EXP-2026-09-08-045/exp045_ranges.txt')

PATTERN = re.compile(r'tensor=(\S+)\|offset=(\d+)\|bytes=(\d+)')


def main():
    tmap = {}
    for shard, path in enumerate(SHARDS):
        reader = gguf.GGUFReader(path)
        for t in reader.tensors:
            tmap[t.name] = (shard, t.data_offset, t.n_bytes)

    n_lines = n_mapped = n_unmapped = n_oob = 0
    rows = []
    with open(LOG) as f:
        for line in f:
            m = PATTERN.search(line)
            if not m:
                continue
            n_lines += 1
            name = m.group(1)
            off = int(m.group(2))
            nbytes = int(m.group(3))
            tensor = name
            if tensor.startswith('CUDA'):
                tensor = tensor.split('#', 1)[1]
                if '#' in tensor:
                    tensor = tensor.rsplit('#', 1)[0]
            if tensor not in tmap:
                n_unmapped += 1
                continue
            shard, base, tlen = tmap[tensor]
            if off + nbytes > tlen:
                n_oob += 1
                continue
            rows.append((shard, base + off, nbytes))
            n_mapped += 1

    total = sum(r[2] for r in rows)
    uniq = {(r[0], r[1]): r[2] for r in rows}
    with open(OUT, 'w') as f:
        f.write('# EXP-2026-09-08-045 workload: shard_index absolute_file_offset bytes\n')
        f.write('# source trace h2d.log sha256 3267cbeaeec4f25fd328e88db8b882f73b2b307ad0913d452b6aa56577b55945\n')
        for shard, addr, nbytes in rows:
            f.write('%d %d %d\n' % (shard, addr, nbytes))

    print('trace lines %d, mapped %d, unmapped %d, out-of-tensor %d' % (n_lines, n_mapped, n_unmapped, n_oob))
    print('total %.3f GiB, unique ranges %d (%.3f GiB)' % (total / 2**30, len(uniq), sum(uniq.values()) / 2**30))
    if n_unmapped or n_oob or n_mapped != n_lines:
        sys.exit('mapping incomplete')


if __name__ == '__main__':
    main()
