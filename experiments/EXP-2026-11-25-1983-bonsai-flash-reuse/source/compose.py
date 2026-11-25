"""Emit two layout-only variants from the qualified FP32 Flash kernel."""
from pathlib import Path

here = Path(__file__).resolve().parent
base = here.parents[1] / 'EXP-2026-11-25-1982-bonsai-flash-fp32/source/flash-fp32.cl'
text = base.read_text()
start = text.index('__kernel void bonsai_flash_f32_tile(')
end = text.index('__kernel void bonsai_flash_f32_merge(')
skewed = text[start:end]


def change(text, old, new):
    if text.count(old) != 1:
        raise ValueError(old)
    return text.replace(old, new)


skewed = change(skewed, 'bonsai_flash_f32_tile(', 'bonsai_flash_skewed_tile(')
skewed = change(skewed, 'uint query_half[FLASH_ROWS][128]', 'uint query_half[FLASH_ROWS][132]')
skewed = change(skewed, 'uint staging[32 * 128]', 'uint staging[256 * 20]')
skewed = change(skewed, 'staging[index] = key < key_count', 'staging[(index/128)*132+index%128] = key < key_count')
skewed = change(skewed, 'staging[key_row*128+p], staging[key_row*128+p+4]',
                'staging[key_row*132+p], staging[key_row*132+p+4]')
skewed = change(skewed, 'transpose[(2*pair)*32+row]', 'transpose[(2*pair)*40+row]')
skewed = change(skewed, 'transpose[(2*pair+1)*32+row]', 'transpose[(2*pair+1)*40+row]')
skewed = change(skewed, 'staging[feature*16+p],staging[feature*16+p+4]',
                'staging[feature*20+p],staging[feature*20+p+4]')

staged = change(skewed, 'bonsai_flash_skewed_tile(', 'bonsai_flash_staged_tile(')
staged = change(staged, '__global const uchar *keys', '__global const uint *keys')
staged = change(staged, '__global const uchar *values', '__global const uint *values')
staged = change(staged, 'uint tile_count, uint group_start)', 'uint tile_count, uint group_start, uint chunks)')
staged = change(staged, '''            const uint key = key_start + kb + index / 128;
            staging[(index/128)*132+index%128] = key < key_count
                ? flash_pack_q4_pair(keys + (ulong)key*576 + kv_head*144, index%128)
                : 0;''',
                '''            const uint chunk = (key_start + kb)/32;
            const ulong offset = ((ulong)kv_head*chunks+chunk)*4096 + index;
            staging[(index/128)*132+index%128] = chunk < chunks ? keys[offset] : 0;''')
staged = change(staged, '''        __local ushort *transpose = (__local ushort *)staging;
        for (uint index = thread; index < 32*128; index += 128) {
            const uint row = index / 128, pair = index % 128, key = key_start + vb + row;
            const uint w = key < key_count
                ? flash_pack_q4_pair(values + (ulong)key*576 + kv_head*144,pair)
                : 0;
            transpose[(2*pair)*40+row] = (ushort)w;
            transpose[(2*pair+1)*40+row] = (ushort)(w >> 16);
        }''',
                '''        for (uint index = thread; index < 32*128; index += 128) {
            const uint chunk = (key_start+vb)/32;
            const ulong offset = ((ulong)kv_head*chunks+chunk)*4096 + index;
            staging[(index/16)*20+index%16] = chunk < chunks ? values[offset] : 0;
        }''')
print(skewed, staged, (here/'stage-kv.cl').read_text(), sep='\n')
