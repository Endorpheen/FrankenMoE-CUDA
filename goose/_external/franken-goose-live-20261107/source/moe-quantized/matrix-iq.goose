fn decode_iq(kind:i64,xx:const IQ2X[:],xs:const IQ2S[:],weight:i64,sub:i64,gx:const u8[8][:],gs:const u8[8][:],signs:const u8[:],out:Decoded&) {
    if kind==3 {
        let x=&xx[weight];let info=word32(x.q,sub*8+4);
        out.scales=packed_simd::splat16((2*(info>>28)+1) as i16);
        for group in 4 {
            let grid=gx[x.q[sub*8+group] as i64];
            let mask=signs[((info>>((7*group) as u32))&127) as i64];
            for lane in 8 {out.q[group*8+lane]=grid[lane];out.mask[group*8+lane]=(if ((mask>>(lane as u8))&1)!=0 {255} else {0}) as! u8;}
        }
    } else {
        assert(kind==4);let x=&xs[weight];
        out.scales=pair_scale(((x.scales[sub]&15) as i16)*2+1,((x.scales[sub]>>4) as i16)*2+1);
        for group in 4 {
            let index=(x.q[sub*4+group] as u16)|(((x.high[sub] as u16)<<((8-2*group) as u16))&768);
            let grid=gs[index as i64];let mask=x.q[32+sub*4+group];
            for lane in 8 {out.q[group*8+lane]=grid[lane];out.mask[group*8+lane]=(if ((mask>>(lane as u8))&1)!=0 {255} else {0}) as! u8;}
        }
    }
}
fn iq_product(weight:const Decoded&,activation:i8[32])->i32[8] {
    return packed_simd::madd16(weight.scales,packed_simd::maddubs(weight.q,flip(activation,weight.mask)));
}
fn dot_iq_rows(kind:i64,xx:const IQ2X[:],xs:const IQ2S[:],first:i64,y:const YK[:],blocks:i64,columns:i64,o0:i64,o1:i64,o2:i64,gx:const u8[8][:],gs:const u8[8][:],signs:const u8[:])->f32[3] {
    assert(columns>=1&&columns<=3&&blocks>=0&&y.len>=o0+blocks);
    if columns>=2 {assert(y.len>=o1+blocks);}
    if columns>=3 {assert(y.len>=o2+blocks);}
    var a0:f32[8]=default<f32[8]>();var a1:f32[8]=default<f32[8]>();var a2:f32[8]=default<f32[8]>();
    for ib in blocks {
        var s00:i32[8]=default<i32[8]>();var s01:i32[8]=default<i32[8]>();
        var s10:i32[8]=default<i32[8]>();var s11:i32[8]=default<i32[8]>();
        var s20:i32[8]=default<i32[8]>();var s21:i32[8]=default<i32[8]>();
        var w0:Decoded=default<Decoded>();var w1:Decoded=default<Decoded>();
        for pair in 4 {
            decode_iq(kind,xx,xs,first+ib,pair*2,gx,gs,signs,w0);
            decode_iq(kind,xx,xs,first+ib,pair*2+1,gx,gs,signs,w1);
            s00=packed_simd::add32(s00,iq_product(w0,y[o0+ib].q[pair*2]));
            s01=packed_simd::add32(s01,iq_product(w1,y[o0+ib].q[pair*2+1]));
            if columns>=2 {
                s10=packed_simd::add32(s10,iq_product(w0,y[o1+ib].q[pair*2]));
                s11=packed_simd::add32(s11,iq_product(w1,y[o1+ib].q[pair*2+1]));
            }
            if columns>=3 {
                s20=packed_simd::add32(s20,iq_product(w0,y[o2+ib].q[pair*2]));
                s21=packed_simd::add32(s21,iq_product(w1,y[o2+ib].q[pair*2+1]));
            }
        }
        let d=if kind==3 {half(xx[first+ib].d)} else {half(xs[first+ib].d)};
        a0=packed_simd::fma32(d*y[o0+ib].d,packed_simd::add32(s00,s01),a0);
        if columns>=2 {a1=packed_simd::fma32(d*y[o1+ib].d,packed_simd::add32(s10,s11),a1);}
        if columns>=3 {a2=packed_simd::fma32(d*y[o2+ib].d,packed_simd::add32(s20,s21),a2);}
    }
    var out:f32[3]=default<f32[3]>();out[0]=0.125*hsum(a0);
    if columns>=2 {out[1]=0.125*hsum(a1);}
    if columns>=3 {out[2]=0.125*hsum(a2);}
    return out;
}

