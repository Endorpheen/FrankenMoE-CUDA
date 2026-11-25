"""Compose the Bonsai device image with token-major variants of its kernels."""
from pathlib import Path
import sys

experiments = Path(__file__).resolve().parents[2]


def read(experiment, file):
    return (experiments / f'EXP-2026-11-25-{experiment}' / 'source' / file).read_text()


def replace_once(text, old, new):
    if text.count(old) != 1:
        raise ValueError(f'Expected one occurrence of {old!r}')
    return text.replace(old, new)


base = read('1973-bonsai-pq2-bootstrap', 'bonsai-pq2.cl')
base = replace_once(base, 'uint head_width, uint key_heads, uint repeat) {',
    '''uint head_width, uint key_heads, uint repeat, uint width) {
    const ulong offset = (ulong)bonsai_grid_y() * width;
    input += offset;
    output += offset;''')
recurrent = read('1974-bonsai-recurrent', 'bonsai-recurrent.cl')
recurrent = replace_once(recurrent, 'uint width, float eps, uint mode) {',
    '''uint width, float eps, uint mode, uint heads, uint stride) {''')
recurrent = replace_once(recurrent, '    float sum = 0.0f;',
    '''    const ulong source = (ulong)(row / heads) * stride + (row % heads) * width;
    float sum = 0.0f;''')
recurrent = replace_once(recurrent, 'x[row * width + i]', 'x[source + i]')
recurrent = replace_once(recurrent, 'x[index] / d', 'x[source + i] / d')
recurrent = replace_once(recurrent, 'const uint lane = local_id(), head = work_group();',
    '''const uint lane = local_id(), head = work_group() % 48, token = work_group() / 48;
    x += (ulong)token * 5120;
    gate += token * 48;
    mix += token * 48;''')
part = (Path(__file__).resolve().parent / 'prefill.cl').read_text()
print('static uint bonsai_grid_y(void);')
print(base, recurrent, read('1975-bonsai-attention', 'bonsai-attention.cl'),
      read('1978-bonsai-pq2-mmq', 'pq2-mmq.cl'), part, sep='\n')
