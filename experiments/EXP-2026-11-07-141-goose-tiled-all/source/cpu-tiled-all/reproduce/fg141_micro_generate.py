from pathlib import Path
cfg=[(32,1,0,0),(16,0,32,1),(16,0,4,1),(16,1,0,0),(32,0,128,0),(32,0,128,1),(16,0,128,1)];parts=[]
for variant,(sub,minimum,bias,act) in enumerate(cfg):
 nb=256//sub
 lines=[f'fn micro_{variant}(w:Workspace&,row0:i64,col0:i64,out0:i64,nk:i64,slab:i64) {{','    let a:WeightTile& = &w.a;let b:ActivationTile& = &w.b;let bs=if nk==1 {256} else {16};','    for i in 16 {let ar=row0+i;let di=slab*16+ar;let panel=ar*nk+slab;','        for group in 2 {let col=col0+group*8;']
 lines.append('            '+''.join(f'var ac{t}:i32[8]=default<i32[8]>();' for t in range(8))+'var mins:i32[8]=default<i32[8]>();')
 lines.append('            for j in 8 {let codes=a.q[panel][j];')
 if sub==32:lines.append('                let scales=packed_simd::splat16(a.scales[panel][j] as! i16);')
 else:lines.append('                let s0=a.scales[panel*2+j/4][2*(j%4)];let s1=a.scales[panel*2+j/4][2*(j%4)+1];let scales=pair_scales(s0 as! i16,s1 as! i16);')
 if bias:lines.append(f'                let signed=debias(codes,{bias});')
 for t in range(8):
  y=f'b.q[(col+{t})*nk+slab][j]'
  pair=f'packed_simd::maddubs(as_unsigned({y}),signed)' if act else f'signed_pairs(signed,{y})' if bias else f'packed_simd::maddubs(codes,{y})'
  lines.append(f'                ac{t}=packed_simd::add32(ac{t},packed_simd::madd16(scales,{pair}));')
 if minimum:
  lines.append('                let bi0=((slab*16+2*j)*bs+col)/8;let bi1=((slab*16+2*j+1)*bs+col)/8;')
  if sub==32:lines.append('                mins=packed_simd::add32(mins,packed_simd::mul32(packed_simd::add32(b.sums[bi0],b.sums[bi1]),splat32(a.mins[panel][j])));')
  else:lines.append('                mins=packed_simd::add32(mins,packed_simd::add32(packed_simd::mul32(b.sums[bi0],splat32(a.mins[panel*2+j/4][2*(j%4)])),packed_simd::mul32(b.sums[bi1],splat32(a.mins[panel*2+j/4][2*(j%4)+1]))));')
 lines.append('            }')
 lines.append('            var sums:i32[8]=default<i32[8]>();'+''.join(f'sums[{t}]=hsum(ac{t});' for t in range(8)))
 if act:lines.append(f'            sums=sub32(sums,splat32(a.mins[panel*{nb//8}][0]));')
 if minimum:lines.append('            let result=packed_simd::fma32(-a.dmin[di],mins,scaled(a.d[di],sums));')
 else:lines.append('            let result=scaled(a.d[di],sums);')
 lines.append('            let target=(out0+i)*32+col/8;for lane in 8 {w.acc[target][lane]=fma(result[lane],b.d[slab*16+col+lane],w.acc[target][lane]);}')
 lines.extend(['        }','    }','}']);parts.append('\n'.join(lines))
parts.append('''fn micro(p:const Plan&,w:Workspace&) {
    assert(p.nk>=1&&p.nk<=16&&p.slab>=0&&p.slab<p.nk&&p.row0>=0&&p.col0>=0&&p.out0>=0&&p.row0+16<=256/p.nk&&p.col0+16<=256/p.nk&&p.out0+16<=256&&p.col0%16==0);
    if p.nk>1 {assert(p.row0==0&&p.col0==0);}
    if p.kind==0||p.kind==1 {micro_0(w,p.row0,p.col0,p.out0,p.nk,p.slab);}
    else if p.kind==2 {micro_1(w,p.row0,p.col0,p.out0,p.nk,p.slab);}
    else if p.kind==3 {micro_2(w,p.row0,p.col0,p.out0,p.nk,p.slab);}
    else if p.kind==4 {micro_3(w,p.row0,p.col0,p.out0,p.nk,p.slab);}
    else if p.kind==5 {micro_4(w,p.row0,p.col0,p.out0,p.nk,p.slab);}
    else if p.kind==6||p.kind==9||p.kind==10||p.kind==11 {micro_5(w,p.row0,p.col0,p.out0,p.nk,p.slab);}
    else {assert(p.kind==7||p.kind==8||p.kind==12);micro_6(w,p.row0,p.col0,p.out0,p.nk,p.slab);}
    witness(1,p.kind,1);
}''')
parts.append('''fn gather_y(p:const Plan&,y:const Q8[:],routes:const Route[:],w:Workspace&) {
    let f=tile_traits(p.kind);let b:ActivationTile& = &w.b;let bs=if p.nk==1 {256} else {16};
    assert(p.count>=0&&p.count<=256&&p.nk>=1&&p.nk<=16&&round16(p.count)*p.nk<=256&&p.route_first>=0&&p.route_first+p.count<=routes.len&&p.slots>0&&p.y_stride>0&&p.start>=0);
    if p.nk>1 {assert(p.count<=16);}
    for slab in p.nk {for row in round16(p.count) {
        let panel=row*p.nk+slab;let di=slab*16+row;
        if row<p.count {
            let route=routes[p.route_first+row];assert(route.slot>=0&&route.token>=0);
            let source=((route.slot as i64)%p.slots+(route.token as i64)*p.slots)*p.y_stride+p.start+slab;let x=&y[source];b.d[di]=x.d;
            for s in 16 {let index=(slab*16+s)*bs+row;b.sums[index/8][index%8]=x.sums[s] as i32;}
            for j in 8 {b.q[panel][j]=if f.act!=0 {biased(x.q[j])} else {x.q[j]};}
        } else {
            b.d[di]=(0 as f32);for s in 16 {let index=(slab*16+s)*bs+row;b.sums[index/8][index%8]=0;}
            for j in 8 {b.q[panel][j]=if f.act!=0 {biased(default<i8[32]>())} else {default<i8[32]>()};}
        }
    }}
    witness(2,p.kind,p.count*p.nk);
}''')
Path('/tmp/fg141_micro.goose').write_text('\n\n'.join(parts)+'\n')
p=Path('/tmp/fg141_common.goose');s=p.read_text().replace('slots:i64,blocks:i64 }','slots:i64,blocks:i64,y_stride:i64 }').replace('fn signed_product(w:i8[32],y:i8[32])->i32[8]','fn signed_pairs(w:i8[32],y:i8[32])->i16[16]').replace('return packed_simd::madd16(packed_simd::splat16(1),packed_simd::maddubs(mag,sy));','return packed_simd::maddubs(mag,sy);');p.write_text(s)
p=Path('/tmp/fg141_entry.goose');s=p.read_text();s=s.replace('assert(p.mode==0);\n    unpack_panel(p,x,w,gxx,gxs,gs,g3x,g3s,g1,signs,palette);','if p.mode==0 {unpack_panel(p,x,w,gxx,gxs,gs,g3x,g3s,g1,signs,palette);}\n    else if p.mode==1 {micro(p,w);}\n    else {assert(p.mode==2);gather_y(p,y,routes,w);}');p.write_text(s)
