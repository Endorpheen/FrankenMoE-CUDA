"""Compose exact MMQ loading experiments from the qualified K64 tile."""
from pathlib import Path
import sys

device=Path(sys.argv[1]).read_text()


def read(rows,tokens):
    name=f'bonsai_pq2_mmq_{rows}x{tokens}_k64'
    return device.split('__kernel void '+name+'(',1)[1].split('\n}\n',1)[0]


def emit(rows,tokens,warps,k):
    text=read(64,128)
    row_warps=rows//16
    token_warps=warps//row_warps
    frag=tokens//(8*token_warps)
    text=text.replace('uint w[64][36]',f'uint w[{rows}][{k//2+4}]')
    text=text.replace('uint x[128][36]',f'uint x[{tokens}][{k//2+4}]')
    text=text.replace('row_base=64*work_group(), token_base=128*flash_grid_y()',
        f'row_base={rows}*work_group(), token_base={tokens}*flash_grid_y()')
    text=text.replace('warp_row=16*(warp/2),warp_token=64*(warp%2)',
        f'warp_row=16*(warp/{token_warps}),warp_token={frag*8}*(warp%{token_warps})')
    text=text.replace('acc[8]',f'acc[{frag}]').replace('i<8;++i',f'i<{frag};++i')
    text=text.replace('step<2*blocks',f'step<{128//k}*blocks')
    text=text.replace('packed<128*32;packed+=1024',f'packed<{tokens}*{k//2};packed+={warps*128}')
    text=text.replace('packed/32',f'packed/{k//2}').replace('packed%32',f'packed%{k//2}')
    text=text.replace('step*32',f'step*{k//2}')
    text=text.replace('index<64*16;index+=256',f'index<{rows}*{k//4};index+={warps*32}')
    text=text.replace('index/16',f'index/{k//4}').replace('index%16',f'index%{k//4}')
    text=text.replace('step/2)*34',f'step/{128//k})*34')
    text=text.replace('16*(step&1)',f'{k//4}*(step&{128//k-1})')
    text=text.replace('k<32',f'k<{k//2}').replace(',36,k,lane',f',{k//2+4},k,lane')
    print(f'__kernel void bonsai_mmq_{rows}x{tokens}_w{warps}_k{k}('+text+'\n}')


for spec in ((64,64,8,32),(64,128,8,32),(32,128,8,64),(32,128,4,64),
             (32,128,8,32),(64,128,4,32)):
    emit(*spec)

for rows,tokens,warps,k in ((64,128,4,32),(64,128,8,64)):
    if k==64:
        body=read(rows,tokens)
    else:
        import contextlib
        import io

        stream=io.StringIO()
        with contextlib.redirect_stdout(stream):
            emit(rows,tokens,warps,k)
        body=stream.getvalue().split('(',1)[1].rsplit('\n}',1)[0]
    old=f'pair=bonsai_ffn_unpack(q,quad+{k//4}*(step&{128//k-1}));'
    new=f'pair=bonsai_pq2_quad(*(__global const ushort *)q,q[2+quad+{k//4}*(step&{128//k-1})]);'
    assert body.count(old)==1
    print(f'__kernel void bonsai_mmq_{rows}x{tokens}_w{warps}_k{k}_lut('+body.replace(old,new)+'\n}')
