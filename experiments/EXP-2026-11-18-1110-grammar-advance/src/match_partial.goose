// match_partial_char: can a partial UTF-8 sequence still satisfy the class at pos?
//   n_remain = number of continuation bytes still expected, value = bits so far
fn g_match_partial(etype: i64[:], eval: i64[:], pos_in: i64, value_in: i64, n_remain: i64) -> i64 {
    var pos: i64 = pos_in;
    let t0: i64 = etype[pos];
    let positive: bool = t0 == 3 || t0 == 7;
    let partial_value: i64 = value_in;
    if n_remain < 0 || (n_remain == 1 && partial_value < 2) {
        return 0;
    }
    var shift: i64 = n_remain * 6;
    var low: i64 = partial_value * pow2(shift);
    var high: i64 = low + (pow2(shift) - 1);
    if low == 0 {
        if n_remain == 2 {
            low = 2048;
        } else if n_remain == 3 {
            low = 65536;
        }
    }
    var done: bool = false;
    while !done {
        if etype[pos + 1] == 5 {
            if eval[pos] <= high && low <= eval[pos + 1] {
                if positive { return 1; }
                return 0;
            }
            pos = pos + 2;
        } else if etype[pos] == 7 {
            return 1;
        } else {
            if low <= eval[pos] && eval[pos] <= high {
                if positive { return 1; }
                return 0;
            }
            pos = pos + 1;
        }
        if etype[pos] != 6 {
            done = true;
        }
    }
    if positive { return 0; }
    return 1;
}

fn pow2(n: i64) -> i64 {
    var v: i64 = 1;
    var i: i64 = 0;
    while i < n {
        v = v * 2;
        i = i + 1;
    }
    return v;
}

fn main() {
    let et: i64[8] = [3, 5, 0, 0, 0, 0, 0, 0];
    let ev: i64[8] = [97, 122, 0, 0, 0, 0, 0, 0];
    let r = g_match_partial(et, ev, 0, 13, 1);
    if r < 0 {
        return;
    }
}
