// APEX.REQUEST/SESSION: request-field decisions in Goose.
//
// JSON parsing itself stays native (it is a library, not inference logic). The bridge exposes the fields this
// path reads, and every decision that follows -- bounds, defaults, derived values -- is Goose.

extern "fg251s_get" fn get(p:i64,k:i64,i:i64)->u64;
extern "fg251s_put" fn put(p:i64,k:i64,i:i64,v:u64);
extern "fg251s_fail" fn fail(c:i64);
extern "fg251s_present" fn present(p:i64,key:i64)->i64;
extern "fg251s_int" fn int_of(p:i64,key:i64,def:i64)->i64;
extern "fg251s_flt_bits" fn flt_bits(p:i64,key:i64,defbits:i64)->i64;

fn ensure(v:bool,c:i64){if !v{fail(c);}}

fn run(packet:i64)->bool {
    let req:i64 = get(packet,0,0) as i64;

    var n_predict:i64 = int_of(req, 1 as i64, -1 as i64);
    ensure(n_predict >= -1 as i64, 10);

    var top_k:i64 = int_of(req, 2 as i64, 40 as i64);
    if top_k < 0 as i64 { top_k = 0 as i64; }

    let tb:i64 = flt_bits(req, 3 as i64, 0x3f000000 as i64);
    put(packet,1,0, n_predict as u64);
    put(packet,1,1, top_k as u64);
    put(packet,1,2, tb as u64);
    put(packet,1,3, present(req, 1 as i64) as u64);
    true
}

fn main(){run(0);}
