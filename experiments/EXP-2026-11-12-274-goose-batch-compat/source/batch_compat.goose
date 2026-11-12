// P8-03: the batch-compatibility predicate, in Goose.
//
// Native reference: tools/server/server-context.cpp, server_slot::can_batch_with(), plus are_lora_equal() in
// tools/server/server-common.cpp:
//
//   - a joint decision head reads the whole batch, so any task carrying one disables batching;
//   - otherwise the tasks must have the same type and the same embedding-input size;
//   - and their LoRA lists must match: same length, and at every index the same adapter pointer and the same scale.
//     The path is deliberately not compared in the native code, and the comparison is a float one (-0.0 equals +0.0,
//     a NaN compares unequal to itself), so the scales travel as their f32 bits and are compared as floats here.

extern "fg274b_fail" fn fail(c:i64);
extern "fg274b_ld_i64" fn ld_i64(base:u64, byte:i64)->i64;
extern "fg274b_from_bits" fn from_bits(x:u32)->f32;

fn ensure(v:bool,c:i64){if !v{fail(c);}}
fn at(a:u64, i:i64)->i64 {return ld_i64(a, i*8 as i64);}

fn can_batch(type_a:i64, type_b:i64, embd_a:i64, embd_b:i64, dec_a:i64, dec_b:i64,
             ptr_a:u64, sc_a:u64, n_a:i64, ptr_b:u64, sc_b:u64, n_b:i64) -> bool {
    ensure(n_a >= 0 as i64 && n_b >= 0 as i64, 10);
    if dec_a != 0 as i64 || dec_b != 0 as i64 {return false;}
    if type_a != type_b {return false;}
    if embd_a != embd_b {return false;}
    if n_a != n_b {return false;}
    var i:i64 = 0 as i64;
    while i < n_a {
        if at(ptr_a, i) != at(ptr_b, i) {return false;}
        let sa:f32 = from_bits((at(sc_a, i) & 0xffffffff as i64) as! u32);
        let sb:f32 = from_bits((at(sc_b, i) & 0xffffffff as i64) as! u32);
        if sa != sb {return false;}
        i = i + 1 as i64;
    }
    return true;
}

fn main(){ can_batch(0 as i64,0 as i64,0 as i64,0 as i64,0 as i64,0 as i64,0 as u64,0 as u64,0 as i64,0 as u64,0 as u64,0 as i64); }
