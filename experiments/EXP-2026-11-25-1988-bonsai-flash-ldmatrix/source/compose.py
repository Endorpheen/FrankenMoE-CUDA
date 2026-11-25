"""Replace only shared-memory fragment loads in the compact Flash kernel."""
from pathlib import Path
import sys

here=Path(__file__).resolve().parent
device=Path(sys.argv[1]).read_text()
tile=device.split('__kernel void bonsai_flash_compact_tile(',1)[1].split('\n}\n',1)[0]


def replace(text, old, new):
    if text.count(old)!=1:
        raise ValueError(old)
    return text.replace(old,new)


tile=replace(tile,'    __local uint query_half','    __local __attribute__((aligned(16))) uint query_half')
tile=replace(tile,'    __local uint staging','    __local __attribute__((aligned(16))) uint staging')
tile=replace(tile,'    __local uint probability','    __local __attribute__((aligned(16))) uint probability')
tile=replace(tile,'        const uint key_row = warp*8 + lane_row;\n','')
tile=replace(tile,'''            const uint p = k + lane_pair;
            acc = bonsai_mma16x8(acc, query_half[lane_row][p], query_half[lane_row+8][p],
                query_half[lane_row][p+4], query_half[lane_row+8][p+4],
                staging[key_row*132+p], staging[key_row*132+p+4]);''',
'''            const uint4 a = bonsai_load_a(&query_half[0][0],132,k,lane);
            const uint2 b = bonsai_load_b(staging+warp*8*132,132,k,lane);
            acc = bonsai_mma16x8(acc,a.x,a.y,a.z,a.w,b.x,b.y);''')
tile=replace(tile,'''            const uint p = ksub + lane_pair, k = vb/2 + p;
            const uint a0 = probability[lane_row][k], a1 = probability[lane_row+8][k];
            const uint a2 = probability[lane_row][k+4], a3 = probability[lane_row+8][k+4];''',
'''            const uint4 a = bonsai_load_a(&probability[0][0],FLASH_KEYS/2,vb/2+ksub,lane);''')
tile=replace(tile,'''                const uint feature = 32*result + 8*warp + lane_row;
                acc[result] = bonsai_mma16x8(acc[result], a0,a1,a2,a3,
                    staging[feature*20+p],staging[feature*20+p+4]);''',
'''                const uint feature_block = (32*result + 8*warp)*20;
                const uint2 b = bonsai_load_b(staging+feature_block,20,ksub,lane);
                acc[result] = bonsai_mma16x8(acc[result],a.x,a.y,a.z,a.w,b.x,b.y);''')
print((here/'fragment-load.cl').read_text())
print('__kernel void bonsai_flash_ldmatrix_tile('+tile+'\n}')

async_tile=replace(tile,'''        for (uint index = thread; index < 32*128; index += 128) {
            const uint chunk = (key_start + kb)/32;
            const ulong offset = ((ulong)kv_head*chunks+chunk)*4096 + index;
            staging[(index/128)*132+index%128] = chunk < chunks ? keys[offset] : 0;
        }
        sync_threads();''',
'''        const uint chunk = (key_start+kb)/32, available = chunk < chunks ? 16 : 0;
        const ulong base = available ? ((ulong)kv_head*chunks+chunk)*4096 : 0;
        for (uint index = thread*4; index < 32*128; index += 512) {
            bonsai_copy16(staging+(index/128)*132+index%128,keys+base+index,available);
        }
        bonsai_copy_complete();
        sync_threads();''')
async_tile=replace(async_tile,'''        for (uint index = thread; index < 32*128; index += 128) {
            const uint chunk = (key_start+vb)/32;
            const ulong offset = ((ulong)kv_head*chunks+chunk)*4096 + index;
            staging[(index/16)*20+index%16] = chunk < chunks ? values[offset] : 0;
        }
        sync_threads();''',
'''        const uint chunk = (key_start+vb)/32, available = chunk < chunks ? 16 : 0;
        const ulong base = available ? ((ulong)kv_head*chunks+chunk)*4096 : 0;
        for (uint index = thread*4; index < 32*128; index += 512) {
            bonsai_copy16(staging+(index/16)*20+index%16,values+base+index,available);
        }
        bonsai_copy_complete();
        sync_threads();''')
print('__kernel void bonsai_flash_async_tile('+async_tile+'\n}')
