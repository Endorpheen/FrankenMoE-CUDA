"""Build two sequential KV tiles into a single ordered FP32 partial."""
from pathlib import Path
import sys

device = Path(sys.argv[1]).read_text()
start = device.index('__kernel void bonsai_flash_staged_tile(')
end = device.index('__kernel void bonsai_flash_stage_kv(', start)
tile = device[start:end].strip()


def replace(text, old, new):
    if text.count(old) != 1:
        raise ValueError(old)
    return text.replace(old, new)


tile = replace(tile, 'bonsai_flash_staged_tile(', 'bonsai_flash_bundle_tile(')
tile = replace(tile, '    __local uint probability[FLASH_ROWS][FLASH_KEYS/2];',
    '''    __local uint probability[FLASH_ROWS][FLASH_KEYS/2];
    __local float maximum[FLASH_ROWS], normalizer[FLASH_ROWS];''')
tile = replace(tile, '    const uint tile = flash_grid_y(), key_start = tile * FLASH_KEYS;',
    '''    const uint tile = flash_grid_y(), bundle_start = tile * (2 * FLASH_KEYS);
    const uint visible_last = first_visible + first + (first+1 < queries);''')
tile = replace(tile, '    if (key_start >= first_visible + first + (first+1 < queries)) return;',
    '    if (bundle_start >= visible_last) return;')
tile = replace(tile, '    for (uint kb = 0; kb < FLASH_KEYS; kb += 32) {',
    '''    float4 carry[8];
    #pragma unroll
    for (uint n = 0; n < 8; ++n) carry[n] = (float4)(0.0f);
    float2 max_used = (float2)(neg_inf), sum_used = (float2)(0.0f);
    for (uint key_start = bundle_start; key_start < bundle_start + 2*FLASH_KEYS &&
         key_start < visible_last; key_start += FLASH_KEYS) {
    for (uint kb = 0; kb < FLASH_KEYS; kb += 32) {''')
tile = replace(tile, '''            out[r*FLASH_PARTIAL] = mx;
            out[r*FLASH_PARTIAL+1] = sum;''',
    '''            maximum[r] = mx;
            normalizer[r] = sum;''')
a = tile.index('    #pragma unroll\n    for (uint result = 0; result < 8; ++result) {\n        const uint column')
tile = tile[:a] + '''    const float2 maxima = (float2)(maximum[lane_row], maximum[lane_row+8]);
    const float2 sums = (float2)(normalizer[lane_row], normalizer[lane_row+8]);
    const float2 next = (float2)(flash_max(max_used.x,maxima.x), flash_max(max_used.y,maxima.y));
    const float2 keep = (float2)(sums.x > 0 ? flash_exp(max_used.x-next.x) : 1.0f,
                                sums.y > 0 ? flash_exp(max_used.y-next.y) : 1.0f);
    const float2 add = (float2)(sums.x > 0 ? flash_exp(maxima.x-next.x) : 0.0f,
                               sums.y > 0 ? flash_exp(maxima.y-next.y) : 0.0f);
    sum_used = sum_used*keep + sums*add;
    max_used = next;
    #pragma unroll
    for (uint n = 0; n < 8; ++n) {
        carry[n] = carry[n]*keep.xxyy + acc[n]*add.xxyy;
    }
    sync_threads();
    }

    if (warp == 0 && lane_pair == 0) {
        out[lane_row*FLASH_PARTIAL] = max_used.x;
        out[lane_row*FLASH_PARTIAL+1] = sum_used.x;
        out[(lane_row+8)*FLASH_PARTIAL] = max_used.y;
        out[(lane_row+8)*FLASH_PARTIAL+1] = sum_used.y;
    }
    #pragma unroll
    for (uint result = 0; result < 8; ++result) {
        const uint column = 32*result + 8*warp + 2*lane_pair + 2;
        out[lane_row*FLASH_PARTIAL+column] = carry[result].x;
        out[lane_row*FLASH_PARTIAL+column+1] = carry[result].y;
        out[(lane_row+8)*FLASH_PARTIAL+column] = carry[result].z;
        out[(lane_row+8)*FLASH_PARTIAL+column+1] = carry[result].w;
    }
}
'''
merge = device.split('__kernel void bonsai_flash_f32_merge(',1)[1].split('\n}\n',1)[0]
merge = replace(merge, '(first_visible+token+FLASH_KEYS-1)/FLASH_KEYS',
                '(first_visible+token+2*FLASH_KEYS-1)/(2*FLASH_KEYS)')
print(tile)
print('__kernel void bonsai_flash_bundle_merge(' + merge + '\n}')
