"""Generate ordered MMQ with two alternating activation staging buffers."""
from pathlib import Path
import sys

source = Path(sys.argv[1]).read_text()
for tokens, ksize in ((64,64),(128,64),(128,32),(64,32)):
    marker=f"__kernel void bonsai_pq2_mmq_64x{tokens}_k64("
    begin=source.index(marker)
    end=source.index("\n}\n",begin)+3
    code=source[begin:end]

    def change(a,b):
        global code
        assert code.count(a)==1,a
        code=code.replace(a,b)

    change(f"bonsai_pq2_mmq_64x{tokens}_k64",f"bonsai_pq2_pipe_64x{tokens}_k{ksize}")
    depth=ksize//2
    stride=depth+4
    steps=128//ksize
    change("uint w[64][36]", f"uint w[64][{stride}]")
    change(f"uint x[{tokens}][36]", f"uint x[2][{tokens}][{stride}]")
    start=code.index("    for (uint step=0;step<2*blocks;++step)")
    finish=code.index("        for (uint index=thread;",start)
    pack=f"""        for (uint packed=thread*4;packed<{tokens}*{depth};packed+=1024) {{
            const uint token=token_base+packed/{depth},col=packed%{depth};
            const uint valid=token<tokens ? 16 : 0;
            const ulong source=valid ? (ulong)token*(width/2)+STEP*{depth}+col : 0;
            bonsai_copy16(&x[SLOT][packed/{depth}][col],input+source,valid);
        }}
        __asm__ volatile("cp.async.commit_group;" ::: "memory");
"""
    initial=pack.replace("STEP","0").replace("SLOT","0")
    next_pack=pack.replace("STEP","(step+1)").replace("SLOT","((step+1)&1)")
    code=code[:start]+initial+f"""    for (uint step=0; step<{steps}*blocks; ++step) {{
        const int next = step + 1 < {steps}*blocks;
        if (next) {{
"""+"\n".join("    "+s for s in next_pack.rstrip().splitlines())+"\n        }\n"+code[finish:]
    assert code.count("index/16")==3
    code=(code.replace("index<64*16",f"index<64*{depth//2}")
              .replace("index/16",f"index/{depth//2}").replace("index%16",f"index%{depth//2}")
              .replace("step/2)*34",f"step/{steps})*34")
              .replace("quad+16*(step&1)",f"quad+{depth//2}*(step&{steps-1})"))
    change("        bonsai_copy_complete();",'''        if (next) {
            __asm__ volatile("cp.async.wait_group 1;" ::: "memory");
        } else {
            __asm__ volatile("cp.async.wait_group 0;" ::: "memory");
        }''')
    change("for (uint k=0;k<32;k+=8)", f"for (uint k=0;k<{depth};k+=8)")
    change("&w[warp_row][0],36,k,lane", f"&w[warp_row][0],{stride},k,lane")
    change("&x[warp_token+8*i][0],36,k,lane", f"&x[step&1][warp_token+8*i][0],{stride},k,lane")
    print(code)

# Read both packed half-blocks while the shared tile remains single-buffered.
helper_start=source.index("static uint2 bonsai_ffn_unpack(")
helper_end=source.index("\n}\n",helper_start)+3
helper=source[helper_start:helper_end]
helper=helper.replace("bonsai_ffn_unpack(__global const uchar *row,uint quad)",
    "bonsai_ffn_unpack_cached(ushort bits,uint codes)").replace("    const uint codes=row[2+quad];\n","")
helper=helper.replace("*(__global const ushort *)row","bits")
print(helper)
for tokens in (64,128):
    name=f"bonsai_pq2_mmq_64x{tokens}_k64"
    begin=source.index(f"__kernel void {name}(")
    end=source.index("\n}\n",begin)+3
    code=source[begin:end].replace(name,f"bonsai_pq2_cached_64x{tokens}_k64")
    anchor="    for (uint step=0;step<2*blocks;++step) {"
    assert code.count(anchor)==1
    code=code.replace(anchor,"""    uint raw[4];
    for (uint step=0;step<2*blocks;++step) {
        if ((step&1)==0) {
            #pragma unroll
            for (uint lane_row=0;lane_row<4;++lane_row) {
                const uint index=thread+256*lane_row;
                const uint row=row_base+index/16, quad=index%16;
                uint bits=0,low=0,high=0;
                if (row<rows) {
                    const __global uchar *q=weights+((ulong)row*blocks+step/2)*34;
                    bits=*(__global const ushort *)q;
                    low=q[2+quad];
                    high=q[18+quad];
                }
                raw[lane_row]=bits|(low<<16)|(high<<24);
            }
        }""")
    start=code.index("        for (uint index=thread;index<64*16;index+=256) {")
    stop=code.index("        bonsai_copy_complete();",start)
    code=code[:start]+"""        #pragma unroll
        for (uint lane_row=0;lane_row<4;++lane_row) {
            const uint index=thread+256*lane_row, quad=index%16;
            const uint entry=raw[lane_row];
            const uint codes=(entry >> (16+8*(step&1))) & 255;
            const uint2 pair=bonsai_ffn_unpack_cached((ushort)entry,codes);
            w[index/16][quad*2]=pair.x;
            w[index/16][quad*2+1]=pair.y;
        }
"""+code[stop:]
    print(code)
