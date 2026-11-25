"""Make a ping-pong copy variant while preserving the tile's arithmetic."""
from pathlib import Path
import sys

into = Path(__file__).resolve().parent
device = Path(sys.argv[1]).read_text()
tile = device.split('__kernel void bonsai_flash_async_tile(',1)[1].split('\n}\n',1)[0]


def change(text, old, new):
    if text.count(old)!=1:
        raise ValueError(old)
    return text.replace(old,new)


tile=change(tile,'''    __local __attribute__((aligned(16))) uint query_half[FLASH_ROWS][132];
    __local __attribute__((aligned(16))) uint staging[256 * 20];
    __local float score[FLASH_ROWS][FLASH_KEYS];
    __local __attribute__((aligned(16))) uint probability[FLASH_ROWS][FLASH_KEYS/2];''',
'''    __local __attribute__((aligned(16))) uint shared_words[12288];
    __local uint *query_half = shared_words;
    __local float (*score)[FLASH_KEYS] = (__local float (*)[FLASH_KEYS])(shared_words+2048);
    __local uint (*probability)[FLASH_KEYS/2] = (__local uint (*)[FLASH_KEYS/2])shared_words;
    __local uint *keys_stage = shared_words+4096;
    __local uint *values_stage = shared_words+1024;''')
tile=change(tile,'query_half[r][index%128] = packed;',
    'query_half[bonsai_swizzled_row(r,index%128)] = packed;')
for offset, source, stage, stride, loop in (
    ('key_start + kb','keys','keys_stage',4096,'kb'),
    ('key_start+vb','values','values_stage',5120,'vb'),
):
    start=tile.index(f'    for (uint {loop} = 0; {loop} < FLASH_KEYS; {loop} += 32)')
    stop=tile.index('        sync_threads();\n',start)+len('        sync_threads();\n')
    tile=tile[:start]+f'''    bonsai_stage_next({stage},{source},key_start/32,chunks,kv_head,thread,{int(source=='values')});
    for (uint {loop} = 0; {loop} < FLASH_KEYS; {loop} += 32) {{
        const uint next = {loop}+32, index = ({loop}/32)&1;
        __local uint *staging = {stage} + {stride}*index;
        if (next < FLASH_KEYS) {{
            bonsai_stage_next({stage}+{stride}*(index^1),{source},
                (key_start+next)/32,chunks,kv_head,thread,{int(source=='values')});
            __asm__ volatile("cp.async.wait_group 1;" ::: "memory");
        }} else {{
            __asm__ volatile("cp.async.wait_group 0;" ::: "memory");
        }}
        sync_threads();
'''+tile[stop:]
tile=change(tile,'bonsai_load_a(&query_half[0][0],132,k,lane)',
    'bonsai_swizzled_a(query_half,k,lane)')
tile=change(tile,'bonsai_load_b(staging+warp*8*132,132,k,lane)',
    'bonsai_swizzled_b(staging,warp*8,k,lane)')
print((into/'double-buffer.cl').read_text())
print('__kernel void bonsai_flash_double_tile('+tile+'\n}')
print('__kernel void bonsai_flash_double_l2_tile('+tile.replace('bonsai_stage_next(', 'bonsai_stage_next_l2(')+'\n}')
