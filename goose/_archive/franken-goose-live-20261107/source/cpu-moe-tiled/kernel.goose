import .packed_simd;
struct Plan { width:i64, output_rows:i64, weight_stride:i64, slots:i64, ith:i64, nth:i64, enabled:i64, forced:i64, output_stride1:i64, output_stride2:i64, kind:i64 }
struct Route { slot:i32, token:i32 }
struct Q4 { d:u16, dmin:u16, scales:u8[12], q:u8[32][4] }
struct Q6 { low:u8[32][4], high:u8[32][2], scales:i8[16], d:u16 }
struct Q8 { d:f32, q:i8[32][8], sums:i16[16] }
struct WeightTile { q:u8[32][2048], d:f32[256], dmin:f32[256], scales:i32[4096], mins:i32[4096] }
struct ActivationTile { q:i8[32][2048], sums:i32[8][512], d:f32[256] }
struct Workspace { a:WeightTile, b:ActivationTile, acc:f32[8][8192] }
extern "fg_tiled_half" fn half(x:u16)->f32;
extern "fmaf" fn fma(x:f32,y:f32,z:f32)->f32;
extern "fg_tiled_witness" fn witness(kind:i64);
fn minimum(a:i64,b:i64)->i64 {return if a<b {a} else {b};}
fn round16(a:i64)->i64 {return ((a+15)/16)*16;}
fn narrow_extent(rows:i64,width:i64)->i64 {
    var n=(31*1024)/(16+rows);n=(n/256)*256;
    if n>4096 {n=4096;}
    while n>=256&&width%n!=0 {n-=256;}
    return if n<256 {0} else {n};
}
fn as_unsigned(a:i8[32])->u8[32] {var out:u8[32]=default<u8[32]>();for i in 32 {out[i]=a[i] as! u8;}return out;}
fn debias(a:u8[32])->i8[32] {var out:i8[32]=default<i8[32]>();for i in 32 {out[i]=((a[i] as i16)-32) as! i8;}return out;}
fn biased(a:i8[32])->i8[32] {var out:i8[32]=default<i8[32]>();for i in 32 {out[i]=((a[i] as! u8)^128) as! i8;}return out;}
fn pair_scales(a:i16,b:i16)->i16[16] {var out:i16[16]=default<i16[16]>();for i in 8 {out[i]=a;out[i+8]=b;}return out;}
fn splat32(a:i32)->i32[8] {var out:i32[8]=default<i32[8]>();for i in 8 {out[i]=a;}return out;}
fn subtract(a:i32[8],b:i32[8])->i32[8] {var out:i32[8]=default<i32[8]>();for i in 8 {out[i]=a[i]-b[i];}return out;}
fn hsum(a:i32[8])->i32 {let p=packed_simd::pair_sum32(a);return (p[0]+p[1])+(p[2]+p[3]);}
fn int_scaled(d:f32,a:i32[8])->f32[8] {var out:f32[8]=default<f32[8]>();for i in 8 {out[i]=d*(a[i] as f32);}return out;}
fn product_add(a:f32[8],b:f32[8],c:f32[8])->f32[8] {var out:f32[8]=default<f32[8]>();for i in 8 {out[i]=fma(a[i],b[i],c[i]);}return out;}
fn unpack_q4(x:const Q4[:],first:i64,stride:i64,start:i64,count:i64,nk:i64,w:Workspace&) {
    let a:WeightTile& = &w.a;
    for slab in nk {for row in round16(count) {
        let di=slab*16+row;let si=row*8*nk+slab*8;let qi=(row*nk+slab)*8;
        if row<count {
            let b=&x[(first+row)*stride+start+slab];a.d[di]=half(b.d);a.dmin[di]=half(b.dmin);
            for j in 4 {
                a.scales[si+j]=(b.scales[j]&63) as i32;a.mins[si+j]=(b.scales[j+4]&63) as i32;
                a.scales[si+j+4]=((b.scales[j+8]&15)|((b.scales[j]>>6)<<4)) as i32;
                a.mins[si+j+4]=((b.scales[j+8]>>4)|((b.scales[j+4]>>6)<<4)) as i32;
                a.q[qi+2*j]=packed_simd::and8(b.q[j],15);a.q[qi+2*j+1]=packed_simd::shr8(b.q[j],4);
            }
        } else {
            a.d[di]=0.0;a.dmin[di]=0.0;
            for j in 8 {a.scales[si+j]=0;a.mins[si+j]=0;a.q[qi+j]=default<u8[32]>();}
        }
    }}
}
fn unpack_q6(x:const Q6[:],first:i64,stride:i64,start:i64,count:i64,nk:i64,w:Workspace&) {
    let a:WeightTile& = &w.a;
    var ones:i8[32]=default<i8[32]>();for i in 32 {ones[i]=1;}
    let ones16=packed_simd::splat16(1);
    for slab in nk {for row in round16(count) {
        let di=slab*16+row;let si=row*16*nk+slab*16;let qi=(row*nk+slab)*8;
        a.dmin[di]=0.0;
        if row<count {
            let b=&x[(first+row)*stride+start+slab];a.d[di]=half(b.d);
            for j in 16 {a.scales[si+j]=b.scales[j] as i32;}
            for h in 2 {for lane in 32 {
                let hi=b.high[h][lane];let lo0=b.low[2*h][lane];let lo1=b.low[2*h+1][lane];
                a.q[qi+4*h][lane]=(lo0&15)|((hi&3)<<4);
                a.q[qi+4*h+1][lane]=(lo1&15)|((hi&12)<<2);
                a.q[qi+4*h+2][lane]=(lo0>>4)|(hi&48);
                a.q[qi+4*h+3][lane]=(lo1>>4)|((hi&192)>>2);
            }}
            var correction:i32=0;
            for j in 8 {
                let sums=packed_simd::madd16(ones16,packed_simd::maddubs(a.q[qi+j],ones));
                let lo=(sums[0]+sums[1])+(sums[2]+sums[3]);let hi=(sums[4]+sums[5])+(sums[6]+sums[7]);
                correction+=128*a.scales[si+2*j]*(lo-512)+128*a.scales[si+2*j+1]*(hi-512);
            }
            a.mins[si]=correction;
        } else {
            a.d[di]=0.0;
            for j in 16 {a.scales[si+j]=0;a.mins[si+j]=0;}
            for j in 8 {a.q[qi+j]=default<u8[32]>();}
        }
    }}
}
fn unpack_y(y:const Q8[:],routes:const Route[:],first:i64,count:i64,slots:i64,blocks:i64,start:i64,nk:i64,w:Workspace&,bias:bool) {
    let b:ActivationTile& = &w.b;
    let stride=if nk==1 {256} else {16};
    for slab in nk {for row in round16(count) {
        let qi=(row*nk+slab)*8;let di=slab*16+row;
        if row<count {
            let route=routes[first+row];let source=((route.slot as i64)%slots+(route.token as i64)*slots)*blocks+start+slab;
            let x=&y[source];b.d[di]=x.d;
            for s in 16 {let index=(slab*16+s)*stride+row;b.sums[index/8][index%8]=x.sums[s] as i32;}
            for j in 8 {b.q[qi+j]=if bias {biased(x.q[j])} else {x.q[j]};}
        } else {
            b.d[di]=0.0;
            for s in 16 {let index=(slab*16+s)*stride+row;b.sums[index/8][index%8]=0;}
            for j in 8 {b.q[qi+j]=if bias {biased(default<i8[32]>())} else {default<i8[32]>()};}
        }
    }}
}
fn micro_q4(w:Workspace&,row0:i64,col0:i64,out0:i64,nk:i64,slab:i64) {
    let a:WeightTile& = &w.a;let b:ActivationTile& = &w.b;
    let qstride=nk*8;let qoff=slab*8;let bs=if nk==1 {256} else {16};
    for i in 16 {
        let ar=row0+i;let di=slab*16+ar;let si=ar*8*nk+slab*8;
        for group in 2 {
            let col=col0+group*8;var acc:i32[8][8]=default<i32[8][8]>();var mins:i32[8]=default<i32[8]>();
            for s in 8 {
                let codes=a.q[ar*qstride+qoff+s];let scales=packed_simd::splat16(a.scales[si+s] as! i16);
                for t in 8 {let yy=b.q[(col+t)*qstride+qoff+s];acc[t]=packed_simd::add32(acc[t],packed_simd::madd16(scales,packed_simd::maddubs(codes,yy)));}
                let bi0=((slab*16+2*s)*bs+col)/8;let bi1=((slab*16+2*s+1)*bs+col)/8;
                mins=packed_simd::add32(mins,packed_simd::mul32(packed_simd::add32(b.sums[bi0],b.sums[bi1]),splat32(a.mins[si+s])));
            }
            var sums:i32[8]=default<i32[8]>();for t in 8 {sums[t]=hsum(acc[t]);}
            let result=packed_simd::fma32(-a.dmin[di],mins,int_scaled(a.d[di],sums));
            var scales:f32[8]=default<f32[8]>();for t in 8 {scales[t]=b.d[slab*16+col+t];}
            let output=(out0+i)*32+col/8;w.acc[output]=product_add(result,scales,w.acc[output]);
        }
    }
    witness(12);
}
fn micro_q6(w:Workspace&,row0:i64,col0:i64,out0:i64,nk:i64,slab:i64) {
    let a:WeightTile& = &w.a;let b:ActivationTile& = &w.b;
    let qstride=nk*8;let qoff=slab*8;
    for i in 16 {
        let ar=row0+i;let di=slab*16+ar;let si=ar*16*nk+slab*16;
        for group in 2 {
            let col=col0+group*8;var acc:i32[8][8]=default<i32[8][8]>();
            for j in 8 {
                let codes=debias(a.q[ar*qstride+qoff+j]);let scales=pair_scales(a.scales[si+2*j] as! i16,a.scales[si+2*j+1] as! i16);
                for t in 8 {let yy=as_unsigned(b.q[(col+t)*qstride+qoff+j]);acc[t]=packed_simd::add32(acc[t],packed_simd::madd16(scales,packed_simd::maddubs(yy,codes)));}
            }
            var sums:i32[8]=default<i32[8]>();for t in 8 {sums[t]=hsum(acc[t]);}
            let result=int_scaled(a.d[di],subtract(sums,splat32(a.mins[si])));
            var scales:f32[8]=default<f32[8]>();for t in 8 {scales[t]=b.d[slab*16+col+t];}
            let output=(out0+i)*32+col/8;w.acc[output]=product_add(result,scales,w.acc[output]);
        }
    }
    witness(14);
}
fn goose_tiled(p:const Plan&,x4:const Q4[:],x6:const Q6[:],y:const Q8[:],routes:const Route[:],w:Workspace&,dst:f32[:])->bool {
    if p.enabled==0||(routes.len<8&&p.forced==0) {return false;}
    assert(p.width>0&&p.width%256==0&&p.output_rows>0&&p.nth>0&&p.ith>=0&&p.ith<p.nth&&p.slots>0);
    let six=p.kind==14;assert(six||p.kind==12);
    let blocks=p.width/256;let groups=(p.output_rows+63)/64;let first=(groups*p.ith)/p.nth;let last=(groups*(p.ith+1))/p.nth;
    var k:i64=0;
    while k<routes.len {
        let rows=minimum(256,routes.len-k);
        var g=first;
        while g<last {
            let start=g*64;let end=minimum(start+64,p.output_rows);let nr=end-start;
            for i in nr {for j in round16(rows) {let index=i*256+j;w.acc[index/8][index%8]=0.0;}}
            if rows<=16 {
                let extent=narrow_extent(rows,p.width);assert(extent>0);let nk=extent/256;
                var kb:i64=0;
                while kb<blocks {
                    unpack_y(y,routes,k,rows,p.slots,blocks,kb,nk,w,six);
                    var row=start;
                    while row<end {
                        let count=minimum(16,end-row);
                        if six {unpack_q6(x6,row,p.weight_stride,kb,count,nk,w);} else {unpack_q4(x4,row,p.weight_stride,kb,count,nk,w);}
                        for slab in nk {if six {micro_q6(w,0,0,row-start,nk,slab);} else {micro_q4(w,0,0,row-start,nk,slab);}}
                        row+=16;
                    }
                    kb+=nk;
                }
            } else {
                for kb in blocks {
                    if six {unpack_q6(x6,start,p.weight_stride,kb,nr,1,w);} else {unpack_q4(x4,start,p.weight_stride,kb,nr,1,w);}
                    unpack_y(y,routes,k,rows,p.slots,blocks,kb,1,w,six);
                    var col:i64=0;
                    while col<rows {
                        var row:i64=0;
                        while row<nr {if six {micro_q6(w,row,col,row,1,0);} else {micro_q4(w,row,col,row,1,0);}row+=16;}
                        col+=16;
                    }
                }
            }
            for j in rows {
                let route=routes[k+j];let base=(route.slot as i64)*p.output_stride1+(route.token as i64)*p.output_stride2+start;
                for i in nr {let index=i*256+j;dst[base+i]=w.acc[index/8][index%8];}
            }
            g+=1;
        }
        k+=256;
    }
    return true;
}
fn main() {let p:Plan=default<Plan>();let x4:Q4[1]=default<Q4[1]>();let x6:Q6[1]=default<Q6[1]>();let y:Q8[1]=default<Q8[1]>();let rows:Route[1]=default<Route[1]>();var w:Workspace=default<Workspace>();var dst:f32[1]=default<f32[1]>();print(goose_tiled(p,x4,x6,y,rows,w,dst));}
