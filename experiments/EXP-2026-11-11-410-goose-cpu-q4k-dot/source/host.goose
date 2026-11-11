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
extern "fg410k_fail" fn fail(code:i64);

fn ensure(v:bool,c:i64){if !v{fail(c);}}

extern "fg410k_f32bits" fn f32b(x:f32)->i64;

fn run(packet:i64)->bool {
    let nb:i64 = get(packet,0,0) as i64;
    let q4:u64 = get(packet,0,1);
    let q8:u64 = get(packet,0,2);
    let sc:u64 = get(packet,1,0);
    ensure(nb > 0, 10);

    var sumf:f32 = 0.0 as f32;
    var sums:f32[8] = default<f32[8]>();
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
        let ut3:i64 = ((ut2 >> 4) & 0x0f0f0f0f) | (((ut1 >> 6) & 0x03030303) << 4);
        let uaux:i64 = ut1 & 0x3f3f3f3f;
        ut1 = (ut2 & 0x0f0f0f0f) | (((ut0 >> 6) & 0x03030303) << 4);
        ut2 = uaux;
        ut0 = ut0 & 0x3f3f3f3f;
        byte_out(sc, 0 as i64, ut0);
        byte_out(sc, 4 as i64, ut1);
        byte_out(sc, 8 as i64, ut2);
        byte_out(sc, 12 as i64, ut3);

        // sumi = sum over the 16 sub-blocks of bsum * min
        var sumi:i64 = 0 as i64;
        var j:i64 = 0 as i64;
        while j < 16 {
            let bsum:i64 = ldi16(q8 + (i*292) as u64, 4 as i64 + j*2 as i64);
            sumi = sumi + bsum * (ldu8(sc, 8 as i64 + (j/2 as i64)) as i64);
            j = j + 1 as i64;
        }

        // unpack the nibbles, then accumulate scale * q8 * q4 into eight lanes
        var k:i64 = 0 as i64;
        while k < 256 {
            let b:i64 = ldu8(q4 + (i*144) as u64, 16 as i64 + (k/2 as i64)) as i64;
            let v:i64 = if (k % 2) == 0 { b & 0x0f } else { (b >> 4) & 0x0f };
            stw(sc, 64 as i64 + k, v);
            k = k + 1 as i64;
        }
        var acc:i64[8] = default<i64[8]>();
        while li < 8 { acc[li] = 0 as i64; li = li + 1 as i64; }
        j = 0 as i64;
        while j < 8 {
            let scale:i64 = ldu8(sc, j) as i64;
            li = 0 as i64;
            while li < 32 {
                let idx:i64 = j*32 as i64 + li;
                let qv:i64 = ld(sc, 64 as i64 + idx);
                let xv:i64 = ldi8(q8 + (i*292) as u64, 4 as i64 + idx) as i64;
                let lane:i64 = li % 8;
                acc[lane] = acc[lane] + scale * qv * xv;
                li = li + 1 as i64;
            }
            j = j + 1 as i64;
        }
        li = 0 as i64;
        while li < 8 {
            sums[li] = sums[li] + d * (acc[li] as f32);
            li = li + 1 as i64;
        }
        sumf = sumf - dmin * (sumi as f32);
        i = i + 1 as i64;
    }
    li = 0 as i64;
    while li < 8 {
        sumf = sumf + sums[li];
        li = li + 1 as i64;
    }
    put(packet,2,0, f32b(sumf) as u64);
    return true;
}

fn byte_out(base:u64, off:i64, v:i64) {
    for bi in 4 {
        stw(base, off + bi as i64, (v >> ((bi*8) as i64)) & 0xff);
    }
}

fn main(){run(0);}
