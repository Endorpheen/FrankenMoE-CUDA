// APEX.REQUEST/SESSION: the greedy-vs-distribution decision of common_sampler_init.
//
//   greedy = n_probs == 0 && !samplers.empty() &&
//            ((last == TEMPERATURE && temp == 0 && dynatemp_range == 0) ||
//             (last == TOP_K && top_k == 1))
// Pure integer/float-compare logic, and a wrong port silently changes sampling.

extern "fg254s_get" fn get(p:i64,k:i64,i:i64)->u64;
extern "fg254s_put" fn put(p:i64,k:i64,i:i64,v:u64);
extern "fg254s_fail" fn fail(c:i64);
extern "fg254s_flt_bits" fn flt_bits(p:i64,k:i64,defbits:i64)->i64;

fn ensure(v:bool,c:i64){if !v{fail(c);}}

// last_type: 0 TEMPERATURE, 1 TOP_K, 2 other
fn run(packet:i64)->bool {
    let n_probs:i64   = get(packet,0,0) as i64;
    let n_samplers:i64 = get(packet,0,1) as i64;
    let last:i64      = get(packet,0,2) as i64;
    let top_k:i64     = get(packet,0,3) as i64;
    let tb:i64        = get(packet,0,4) as i64;
    let db:i64        = get(packet,0,5) as i64;
    ensure(n_probs >= 0 as i64 && n_samplers >= 0 as i64, 1);

    var g:i64 = 0 as i64;
    if n_probs == 0 as i64 && n_samplers > 0 as i64 {
        if last == 0 as i64 && tb == 0 as i64 && db == 0 as i64 { g = 1 as i64; }
        if last == 1 as i64 && top_k == 1 as i64 { g = 1 as i64; }
    }
    put(packet,1,0, g as u64);
    true
}

fn main(){run(0);}
