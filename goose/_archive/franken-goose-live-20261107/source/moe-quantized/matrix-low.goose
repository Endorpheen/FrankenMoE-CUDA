fn q2_codes(x:const Q2&,part:i64)->u8[32] {
    var out:u8[32]=default<u8[32]>();
    for i in 32 {out[i]=(x.q[part*8+i/4]>>(((i%4)*2) as u8))&3;}
    return out;
}
fn q2_acc(x:const Q2&,y:const Y0[:],first:i64,acc:f32[8])->f32[8] {
    var ones:u8[32]=default<u8[32]>();for i in 32 {ones[i]=1;}
    let q0=sub16(packed_simd::maddubs(q2_codes(x,0),y[first].q),packed_simd::maddubs(ones,y[first].q));
    let q1=sub16(packed_simd::maddubs(q2_codes(x,1),y[first+1].q),packed_simd::maddubs(ones,y[first+1].q));
    let p0=packed_simd::madd16(packed_simd::splat16(1),q0);let p1=packed_simd::madd16(packed_simd::splat16(1),q1);
    let d=half(x.d);
    let out=packed_simd::fma32(d*half(y[first].d),p0,acc);
    return packed_simd::fma32(d*half(y[first+1].d),p1,out);
}
fn dot_q2(x:const Q2[:],y:const Y0[:])->f32 {
    assert(y.len>=2*x.len);
    var a0:f32[8]=default<f32[8]>();var a1:f32[8]=default<f32[8]>();var i:i64=0;
    while i+1<x.len {a0=q2_acc(x[i],y,2*i,a0);a1=q2_acc(x[i+1],y,2*i+2,a1);i+=2;}
    if i<x.len {a0=q2_acc(x[i],y,2*i,a0);}
    return hsum(addf(a0,a1));
}
fn q5_values(x:const Q5&)->i8[32] {
    var out:i8[32]=default<i8[32]>();
    for i in 32 {
        let low=(x.q[i%16]>>(((i/16)*4) as u8))&15;
        let high=(x.high[i/8]>>((i%8) as u8))&1;
        out[i]=((low as i16)+(high as i16)*16-16) as! i8;
    }
    return out;
}
fn dot_q5(x:const Q5[:],y:const Y0[:])->f32 {
    assert(y.len>=x.len);var acc:f32[8]=default<f32[8]>();
    for i in x.len {let d=half(x[i].d)*half(y[i].d);acc=packed_simd::fma32(d,signed_product(q5_values(x[i]),y[i].q),acc);}
    return hsum(acc);
}
fn nl_values(x:const NL4&,palette:const i8[:])->i8[32] {
    var lookup:u8[32]=default<u8[32]>();var indices:u8[32]=default<u8[32]>();
    for i in 32 {lookup[i]=palette[i%16] as! u8;}
    for i in 16 {indices[i]=x.q[i]&15;indices[i+16]=x.q[i]>>4;}
    return signed_bytes(packed_simd::shuffle8(lookup,indices));
}
fn dot_nl(x:const NL4[:],y:const Y0[:],palette:const i8[:])->f32 {
    assert(y.len>=x.len);var a0:f32[8]=default<f32[8]>();var a1:f32[8]=default<f32[8]>();var i:i64=0;
    while i+1<x.len {
        a0=packed_simd::fma32(half(y[i].d)*half(x[i].d),signed_product(nl_values(x[i],palette),y[i].q),a0);
        a1=packed_simd::fma32(half(y[i+1].d)*half(x[i+1].d),signed_product(nl_values(x[i+1],palette),y[i+1].q),a1);
        i+=2;
    }
    var result=hsum(addf(a0,a1));
    if i<x.len {
        var s0:i32=0;var s1:i32=0;
        for j in 16 {s0+=(y[i].q[j] as i32)*(palette[(x[i].q[j]&15) as i64] as i32);s1+=(y[i].q[j+16] as i32)*(palette[(x[i].q[j]>>4) as i64] as i32);}
        let scale=half(y[i].d)*half(x[i].d);
        if iq4_tail_fused() {result=fma(scale,(s0+s1) as f32,result);}
        else {let product=scale*((s0+s1) as f32);result=result+product;}
    }
    return result;
}

