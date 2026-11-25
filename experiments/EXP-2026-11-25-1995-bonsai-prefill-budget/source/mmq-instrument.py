"""Instrument a sampled subset of MMQ CTAs at well-defined stage boundaries."""
from pathlib import Path
import sys

text=Path(sys.argv[1]).read_text()
tile=text.split('__kernel void bonsai_pq2_mmq_64x64(',1)[1].split('\n}\n',1)[0]


def replace(a,b):
    global tile
    if tile.count(a)!=1:
        raise ValueError(a)
    tile=tile.replace(a,b)


replace('uint width, uint rows, uint tokens)', 'uint width, uint rows, uint tokens, __global ulong *counts)')
replace('    for (uint block = 0; block < blocks; ++block) {',
'''    const bool measure=thread==0 && (work_group()%8)==0 && (tile_token%2)==0;
    ulong t0=0,t1=0,t2=0,t3=0;
    ulong issues=0,weights_and_stores=0,waits=0,mma_and_barrier=0;
    for (uint block = 0; block < blocks; ++block) {
        if (measure) t0=budget_clock();''')
replace('        for (uint index = thread;',
'''        if (measure) t1=budget_clock();
        for (uint index = thread;''')
replace('        bonsai_copy_complete();\n        sync_threads();',
'''        if (measure) t2=budget_clock();
        bonsai_copy_complete();
        sync_threads();
        if (measure) t3=budget_clock();''')
replace('        sync_threads();\n    }',
'''        sync_threads();
        if (measure) {
            const ulong stop=budget_clock();
            issues+=t1-t0;
            weights_and_stores+=t2-t1;
            waits+=t3-t2;
            mma_and_barrier+=stop-t3;
        }
    }
    if (measure) {
        const ulong m_tiles=(rows+63)/64;
        __global ulong *out=counts+((ulong)tile_token*m_tiles+work_group())*4;
        out[0]=issues;
        out[1]=weights_and_stores;
        out[2]=waits;
        out[3]=mma_and_barrier;
    }''')
print(text)
print('static ulong budget_clock(void) { ulong t; '
      '__asm__ volatile("mov.u64 %0, %%clock64;" : "=l"(t) :: "memory"); return t; }')
print('__kernel void bonsai_pq2_mmq_budget('+tile+'\n}')
