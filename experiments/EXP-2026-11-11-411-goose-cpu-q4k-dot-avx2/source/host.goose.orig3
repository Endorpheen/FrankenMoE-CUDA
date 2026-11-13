// Apex Q4_K x Q8_K dot product in Goose (ggml_vec_dot_q4_K_q8_K, generic order).
//
// Layout of one super-block pair (256 values):
//   q4:     d (f16), dmin (f16), scales[12] (packed 6-bit scale/min pairs), qs[128] (nibbles)
//   q8:     d (f32), qs[256] (int8), bsums[16] (int16)
//
// The scales array holds eight 6-bit scales and eight 6-bit mins. The first four pairs are packed as
// scale[i] = s[i] | (s[i+4] << 6) and the remaining four live in the tail, exactly as the reference unpacks
// them with three masks.
//
// Packet:
//   k=0  0 nb, 1 q4 pointer, 2 q8 pointer, 3 out (float bits)
//   k=1  0 scratch (enough for the unpacked arrays)
//   k=2  0 done
//
// Records: 0 block (a = block index, b = sumf bits)
extern "fg410k_get" fn get(p:i64,k:i64,i:i64)->u64;
extern "fg410k_put" fn put(p:i64,k:i64,i:i64,v:u64);
extern "fg410k_stw" fn stw(base:u64,byte:i64,v:i64);
extern "fg410k_ld_i64" fn ld(base:u64,byte:i64)->i64;
extern "fg410k_ldu8" fn ldu8(base:u64,byte:i64)->i64;
extern "fg410k_ldi8" fn ldi8(base:u64,byte:i64)->i64;
extern "fg410k_ldf" fn ldf(base:u64,byte:i64)->f32;
extern "fg410k_f16f32" fn f16f32(h:i64)->f32;
extern "fg410k_ldi16" fn ldi16(base:u64,byte:i64)->i64;
extern "fg410k_ldu32" fn ldu32(base:u64,byte:i64)->i64;
extern "fg410k_stb" fn stb(base:u64,byte:i64,v:i64);
extern "fg410k_fail" fn fail(code:i64);

fn ensure(v:bool,c:i64){if !v{fail(c);}}

extern "fg410k_f32bits" fn f32b(x:f32)->i64;
extern "fg410k_fma" fn fma(a:f32,b:f32,c:f32)->f32;

fn run(packet:i64)->bool {
    let nb:i64 = get(packet,0,0) as i64;
    let q4:u64 = get(packet,0,1);
    let q8:u64 = get(packet,0,2);
    let sc:u64 = get(packet,1,0);
    ensure(nb > 0, 10);

    var sumf:f32 = 0.0 as f32;
    var sums:f32[8] = default<f32[8]>();
    var accm:f32[4] = default<f32[4]>();
    var am:i64 = 0 as i64;
    while am < 4 { accm[am] = 0.0 as f32; am = am + 1 as i64; }
    var li:i64 = 0 as i64;
    var q:i64 = 0 as i64;
    while q < 8 { sums[q] = 0.0 as f32; q = q + 1 as i64; }
    var i:i64 = 0 as i64;
    while i < nb {
        // super-block headers
        let d:f32 = f16f32(ld(q4 + (i*144) as u64, 0 as i64)) * ldf(q8 + (i*292) as u64, 0 as i64);
        let dmin:f32 = f16f32(ld(q4 + (i*144) as u64, 2 as i64)) * ldf(q8 + (i*292) as u64, 0 as i64);

        // unpack the packed 6-bit scales and mins into sc[0..15] (scales) and sc[16..31] (mins)
        var ut0:i64 = ldu32(q4 + ((i*144) as u64), 4 as i64);
        var ut1:i64 = ldu32(q4 + ((i*144) as u64), 8 as i64);
        var ut2:i64 = ldu32(q4 + ((i*144) as u64), 12 as i64);
        let ut3:i64 = ((ut2 >> 4) & (0x0f0f0f0f as i64)) | (((ut1 >> 6) & (0x03030303 as i64)) << 4);
        let uaux:i64 = ut1 & (0x3f3f3f3f as i64);
        ut1 = (ut2 & (0x0f0f0f0f as i64)) | (((ut0 >> 6) & (0x03030303 as i64)) << 4);
        ut2 = uaux;
        ut0 = ut0 & (0x3f3f3f3f as i64);
        byte_out(sc, 0 as i64, ut0);
        byte_out(sc, 4 as i64, ut1);
        byte_out(sc, 8 as i64, ut2);
        byte_out(sc, 12 as i64, ut3);

        // sumi = sum over the 16 sub-blocks of bsum * min
        var sumi:i64 = 0 as i64;
        var j:i64 = 0 as i64;
        while j < 16 {
            let bsum:i64 = ldi16(q8 + (i*292) as u64, 260 as i64 + j*2 as i64);
            let mn:i64 = ldu8(sc, 8 as i64 + (j/2 as i64)) as i64;
            sumi = sumi + bsum * mn;
            if j < 4 as i64 { stw(sc, 1400 as i64 + j*16 as i64, bsum); stw(sc, 1408 as i64 + j*16 as i64, mn); }
            j = j + 1 as i64;
        }

        // unpack the nibbles, then accumulate scale * q8 * q4 into eight lanes
        // the reference unpacks in groups of 32: all low nibbles of the group, then all high ones
        var gj:i64 = 0 as i64;
        while gj < 4 {
            var gl:i64 = 0 as i64;
            while gl < 32 {
                let bb:i64 = ldu8(q4 + ((i*144) as u64), 16 as i64 + gj*32 as i64 + gl as i64);
                stb(sc, 64 as i64 + gj*64 as i64 + gl as i64, bb & 0x0f);
                stb(sc, 64 as i64 + gj*64 as i64 + 32 as i64 + gl as i64, (bb >> 4) & 0x0f);
                gl = gl + 1 as i64;
            }
            gj = gj + 1 as i64;
        }
        var acc:i64[8] = default<i64[8]>();
        while li < 8 { acc[li] = 0 as i64; li = li + 1 as i64; }
        j = 0 as i64;
        while j < 8 {
            li = 0 as i64;
            while li < 32 {
                let idx:i64 = j*32 as i64 + li;
                let grp:i64 = idx / 64 as i64;
                let half:i64 = (idx % 64 as i64) / 32 as i64;
                let scale:i64 = ldu8(sc, grp*2 as i64 + half) as i64;
                let qv:i64 = ldu8(sc, 64 as i64 + idx);
                let xv:i64 = ldi8(q8 + (i*292) as u64, 4 as i64 + idx) as i64;
                let lane:i64 = li / 4 as i64;
                acc[lane] = acc[lane] + scale * qv * xv;
                li = li + 1 as i64;
            }
            j = j + 1 as i64;
        }
        li = 0 as i64;
        while li < 8 {
            sums[li] = fma(d, acc[li] as f32, sums[li]);
            if li == 0 as i64 { stw(sc, 1500 as i64, acc[0]); }
            li = li + 1 as i64;
        }
        let dminn:f32 = 0.0 as f32 - dmin;
        // madd_epi16 pairs: prod[i] = mins[2i]*(bs[4i]+bs[4i+1]) + mins[2i+1]*(bs[4i+2]+bs[4i+3])
        am = 0 as i64;
        while am < 4 {
            let bs0:i64 = ldi16(q8 + (i*292) as u64, 260 as i64 + am*8 as i64);
            let bs1:i64 = ldi16(q8 + (i*292) as u64, 262 as i64 + am*8 as i64);
            let bs2:i64 = ldi16(q8 + (i*292) as u64, 264 as i64 + am*8 as i64);
            let bs3:i64 = ldi16(q8 + (i*292) as u64, 266 as i64 + am*8 as i64);
            let mna:i64 = ldu8(sc, 8 as i64 + am*2 as i64) as i64;
            let mnb:i64 = ldu8(sc, 9 as i64 + am*2 as i64) as i64;
            let pr:i64 = mna*(bs0+bs1) + mnb*(bs2+bs3);
            accm[am] = fma(dminn, pr as f32, accm[am]);
            am = am + 1 as i64;
        }
        stw(sc, 2000 as i64 + i*32 as i64, f32b(d));
        stw(sc, 2008 as i64 + i*32 as i64, f32b(dmin));
        stw(sc, 2016 as i64 + i*32 as i64, sumi);
        stw(sc, 2024 as i64 + i*32 as i64, f32b(sums[0]));
        stw(sc, 1300 as i64, f32b(d));
        stw(sc, 1308 as i64, sumi);
        stw(sc, 1316 as i64, f32b(sums[0]));
        i = i + 1 as i64;
    }
    // AVX2 horizontal reduction: hi128 + lo128, then add movehl, then movehdup
    let r0:f32 = sums[4] + sums[0];
    let r1:f32 = sums[5] + sums[1];
    let r2:f32 = sums[6] + sums[2];
    let r3:f32 = sums[7] + sums[3];
    let hsum:f32 = (r0 + r2) + (r1 + r3);
    // acc_m: movehl then movehdup
    let msum:f32 = (accm[0] + accm[2]) + (accm[1] + accm[3]);
    stw(sc, 1700 as i64, f32b(hsum));
    stw(sc, 1708 as i64, f32b(msum));
    sumf = hsum + msum;
    put(packet,2,0, f32b(sumf) as u64);
    return true;
}

fn byte_out(base:u64, off:i64, v:i64) {
    for bi in 4 {
        stb(base, off + bi as i64, (v >> ((bi*8) as i64)) & 0xff);
    }
}

fn main(){run(0);}
