// APEX.REQUEST/SESSION: the slot's remaining-token budget, in Goose.
//
// server_slot::n_remaining() and has_budget() run on every decode step of the agreed request:
//   n_remaining()  = n_predict_max == -1 ? -1 : n_predict_max - n_gen
//   has_budget()   = n_predict_max == -1 || n_remaining() > 0
// Both are pure integer logic, so they cross the bridge as scalars.
extern "fg250s_get" fn get(p: i64, k: i64, i: i64) -> u64;
extern "fg250s_put" fn put(p: i64, k: i64, i: i64, v: u64);

fn ensure(v: bool, c: i64) {
    if !v {
        fail(c);
    }
}
extern "fg250s_fail" fn fail(c: i64);

fn run(packet: i64) -> bool {
    let n_predict_max: i64 = get(packet, 0, 0) as i64;
    let n_gen: i64 = get(packet, 0, 1) as i64;
    ensure(n_gen >= 0 as i64, 1);
    let remaining: i64 = if n_predict_max == -1 as i64 {
        -1 as i64
    } else {
        n_predict_max - n_gen
    }
    ;
    let has_budget: i64 = if n_predict_max == -1 as i64 {
        1 as i64
    } else {
        if remaining > 0 as i64 {
            1 as i64
        } else {
            0 as i64
        }
    }
    ;
    put(packet, 1, 0, remaining as u64);
    put(packet, 1, 1, has_budget as u64);
    true
}

fn main() {
    run(0);
}

