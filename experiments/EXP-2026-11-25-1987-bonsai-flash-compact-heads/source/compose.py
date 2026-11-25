"""Pack consecutive real Q heads into each 16-row Flash tensor fragment."""
from pathlib import Path
import sys

device = Path(sys.argv[1]).read_text()


def fn_source(name):
    return device.split('__kernel void '+name+'(',1)[1].split('\n}\n',1)[0]


def replace(text, old, new):
    if text.count(old) != 1:
        raise ValueError(old)
    return text.replace(old,new)


tile = fn_source('bonsai_flash_staged_tile')
tile = replace(tile, '    const uint kv_head = work % 4, first = (work / 4) * 2;',
    '''    const uint groups_per_head = (queries*6 + FLASH_ROWS-1)/FLASH_ROWS;
    const uint kv_head = work / groups_per_head;
    const uint first_row = (work % groups_per_head)*FLASH_ROWS;
    const uint last_row = flash_min(first_row+FLASH_ROWS, queries*6)-1;''')
tile = replace(tile, '    if (key_start >= first_visible + first + (first+1 < queries)) return;',
    '    if (key_start >= first_visible + last_row/6) return;')
tile = replace(tile, '        const uint token = first + r/8, h = r % 8;',
    '''        const uint original = first_row+r, token = original/6, h = original%6;''')
tile = replace(tile, '        if (token < queries && h < 6) {', '        if (token < queries) {')
tile = replace(tile, '        const uint token = first + r/8;', '        const uint token = (first_row+r)/6;')
print('__kernel void bonsai_flash_compact_tile('+tile+'\n}')

for original, name in (
    ('bonsai_flash_f32_merge','bonsai_flash_compact_merge'),
    ('bonsai_flash_ordered_merge','bonsai_flash_compact_ordered_merge'),
):
    merge=fn_source(original)
    start=merge.index('    const uint group =')
    stop=merge.index('    if (token >= queries)',start)
    index_name='thread' if original.endswith('ordered_merge') else 'lane'
    merge=merge[:start]+f'''    const uint group = work_group()/FLASH_ROWS;
    const uint work = group+group_start, slot = work_group()%FLASH_ROWS;
    const uint groups_per_head = (queries*6 + FLASH_ROWS-1)/FLASH_ROWS;
    const uint original_row = (work % groups_per_head)*FLASH_ROWS+slot;
    const uint token = original_row/6;
    const uint head = (work / groups_per_head)*6+original_row%6;
    const uint {index_name} = local_id();
'''+merge[stop:]
    merge=replace(merge,'    const uint slot = (r/6)*8+r%6;\n','')
    print('__kernel void '+name+'('+merge+'\n}')
