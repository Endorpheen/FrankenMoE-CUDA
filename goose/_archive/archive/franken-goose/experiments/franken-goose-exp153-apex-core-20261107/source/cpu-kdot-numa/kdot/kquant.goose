// Active Apex Q4_K_M K-quant dot-product block.
// Source: inputs/native/ggml/src/ggml-cpu/quants.c
//   ggml_vec_dot_q4_K_q8_K_generic (line 696)
//   ggml_vec_dot_q6_K_q8_K_generic (line 851)
// The runtime's AVX2 entries and the generic entries were probed bit-identical
// on the applicable Q4_K/Q6_K x Q8_K blocks before this port.
struct Q4Scales { scales:u8[8], mins:u8[8] }
extern "fg143_half" fn half(x:u16)->f32;
extern "fg143_from_bits" fn from_bits(x:u32)->f32;
extern "fmaf" fn fma(a:f32,b:f32,c:f32)->f32;
extern "fg143_witness" fn witness(op:i64,values:i64);

fn read16(a:const u8[:],i:i64)->u16 {assert(i>=0&&i<=a.len-2);return (a[i] as u16)|((a[i+1] as u16)<<8);}
fn read32(a:const u8[:],i:i64)->u32 {assert(i>=0&&i<=a.len-4);return (a[i] as u32)|((a[i+1] as u32)<<8)|((a[i+2] as u32)<<16)|((a[i+3] as u32)<<24);}
fn unpack_scales(x:const u8[:],base:i64)->Q4Scales {
    var u:u32[4]=default<u32[4]>();
    for k in 4 {u[k]=read32(x,base+4*k);}
    let m1:u32=0x3f3f3f3f;let m2:u32=0x0f0f0f0f;let m3:u32=0x03030303;
    u[3]=((u[2]>>4)&m2)|(((u[1]>>6)&m3)<<4);
    let aux:u32=u[1]&m1;
    u[1]=(u[2]&m2)|(((u[0]>>6)&m3)<<4);
    u[2]=aux;
    u[0]=u[0]&m1;
    var out:Q4Scales=default<Q4Scales>();
    for k in 4 {
        out.scales[k]=(u[0]>>((8*k) as u32)) as! u8;
        out.scales[4+k]=(u[1]>>((8*k) as u32)) as! u8;
        out.mins[k]=(u[2]>>((8*k) as u32)) as! u8;
        out.mins[4+k]=(u[3]>>((8*k) as u32)) as! u8;
    }
    return out;
}

fn hsum8(a:f32[8])->f32 {
    let s0=a[0]+a[4];let s1=a[1]+a[5];let s2=a[2]+a[6];let s3=a[3]+a[7];
    return (s0+s2)+(s1+s3);
}
fn dot_q4_K_q8_K(n:i64,x:const u8[:],y:const u8[:])->f32 {
    assert(n>0&&n%256==0);
    let nb=n/256;
    var acc:f32[8]=default<f32[8]>();
    var accm:f32[4]=default<f32[4]>();
    for ib in nb {
        let xb=ib*144;let yb=ib*292;
        let yd=from_bits(read32(y,yb));
        let d=half(read16(x,xb))*yd;
        let dminneg=(0 as f32)-(half(read16(x,xb+2))*yd);
        let sm=unpack_scales(x,xb+4);
        var aux8:i32[256]=default<i32[256]>();
        for j in 4 {
            for l in 32 {
                let byte=x[xb+16+j*32+l];
                aux8[j*64+l]=(byte&15) as i32;
                aux8[j*64+32+l]=(byte>>4) as i32;
            }
        }
        var prod:i32[4]=default<i32[4]>();
        for k in 4 {
            let j=2*k;
            let b0=(read16(y,yb+260+2*(4*k)) as! i16) as i32;
            let b1=(read16(y,yb+260+2*(4*k+1)) as! i16) as i32;
            let b2=(read16(y,yb+260+2*(4*k+2)) as! i16) as i32;
            let b3=(read16(y,yb+260+2*(4*k+3)) as! i16) as i32;
            prod[k]=(sm.mins[j] as i32)*(b0+b1)+(sm.mins[j+1] as i32)*(b2+b3);
        }
        for k in 4 {accm[k]=fma(dminneg,prod[k] as f32,accm[k]);}
        var aux32:i32[8]=default<i32[8]>();
        for chunk in 8 {
            let scale=sm.scales[chunk] as i32;
            for lane in 8 {
                var sum4:i32=0;
                for k in 4 {
                    let idx=chunk*32+4*lane+k;
                    sum4+=((y[yb+4+idx] as! i8) as i32)*aux8[idx];
                }
                aux32[lane]+=scale*sum4;
            }
        }
        for l in 8 {acc[l]=fma(d,aux32[l] as f32,acc[l]);}
    }
    witness(0,n);
    return hsum8(acc)+((accm[0]+accm[2])+(accm[1]+accm[3]));
}

fn dot_q6_K_q8_K(n:i64,x:const u8[:],y:const u8[:])->f32 {
    assert(n>0&&n%256==0);
    let nb=n/256;
    var acc:f32[8]=default<f32[8]>();
    for ib in nb {
        let xb=ib*210;let yb=ib*292;
        let yd=from_bits(read32(y,yb));
        let d=half(read16(x,xb+208))*yd;
        var aux8:i32[256]=default<i32[256]>();
        for halfb in 2 {
            let qlb=halfb*64;let qhb=halfb*32;let ab=halfb*128;
            for l in 32 {
                let qh=x[xb+128+qhb+l];
                aux8[ab+l]=((x[xb+qlb+l]&15) as i32)|((((qh>>0)&3) as i32)<<4);
                aux8[ab+32+l]=((x[xb+qlb+32+l]&15) as i32)|((((qh>>2)&3) as i32)<<4);
                aux8[ab+64+l]=((x[xb+qlb+l]>>4) as i32)|((((qh>>4)&3) as i32)<<4);
                aux8[ab+96+l]=((x[xb+qlb+32+l]>>4) as i32)|((((qh>>6)&3) as i32)<<4);
            }
        }
        var aux32:i32[8]=default<i32[8]>();
        for chunk in 8 {
            for lane in 8 {
                let part=lane/4;
                let scale=(x[xb+192+2*chunk+part] as! i8) as i32;
                var sum4:i32=0;
                for k in 4 {
                    let idx=chunk*32+part*16+4*(lane%4)+k;
                    sum4+=((y[yb+4+idx] as! i8) as i32)*aux8[idx];
                }
                aux32[lane]+=scale*sum4;
            }
        }
        for lane in 8 {
            let b0=(read16(y,yb+260+2*(2*lane)) as! i16) as i32;
            let b1=(read16(y,yb+260+2*(2*lane+1)) as! i16) as i32;
            let s0=(x[xb+192+2*lane] as! i8) as i32;
            let s1=(x[xb+192+2*lane+1] as! i8) as i32;
            aux32[lane]-=32*(b0*s0+b1*s1);
        }
        for l in 8 {acc[l]=fma(d,aux32[l] as f32,acc[l]);}
    }
    witness(1,n);
    return hsum8(acc);
}

fn entry(mode:i64,n:i64,x:const u8[:],y:const u8[:])->f32 {
    if mode==0 {return dot_q4_K_q8_K(n,x,y);}
    assert(mode==1);
    return dot_q6_K_q8_K(n,x,y);
}
fn main() {
    let x:u8[1]=default<u8[1]>();let y:u8[1]=default<u8[1]>();
    print(entry(0,256,x,y));
}
