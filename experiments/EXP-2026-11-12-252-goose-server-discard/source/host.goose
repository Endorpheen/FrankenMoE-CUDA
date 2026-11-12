// APEX.REQUEST/SESSION: context-shift policy (server_context.cpp, the n_discard decision).
//
// When the context is full the server keeps n_keep tokens and discards a window from there:
//   n_discard = params.n_discard ? params.n_discard : n_left / 2
//   n_discard = clamp(n_discard, 0, max(0, n_left - 1))
// Pure integer logic, and a wrong port here silently corrupts the prompt history.

extern "fg252s_get" fn get(p:i64,k:i64,i:i64)->u64;
extern "fg252s_put" fn put(p:i64,k:i64,i:i64,v:u64);
extern "fg252s_fail" fn fail(c:i64);

fn ensure(v:bool,c:i64){if !v{fail(c);}}

fn run(packet:i64)->bool {
    let n_keep:i64 = get(packet,0,0) as i64;
    let n_left:i64 = get(packet,0,1) as i64;
    let param:i64  = get(packet,0,2) as i64;   // params.n_discard, 0 = automatic
    ensure(n_keep >= 0 as i64, 1);
    ensure(n_left >= 0 as i64, 2);
    ensure(param >= 0 as i64, 3);

    var d:i64 = param;
    if d == 0 as i64 { d = n_left / 2 as i64; }
    let hi:i64 = if n_left - 1 as i64 > 0 as i64 { n_left - 1 as i64 } else { 0 as i64 };
    if d < 0 as i64 { d = 0 as i64; }
    if d > hi { d = hi; }
    put(packet,1,0, d as u64);
    true
}

fn main(){run(0);}
