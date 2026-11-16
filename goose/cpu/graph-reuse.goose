// Graph-input reuse decision (llama-graph.cpp `llm_graph_input_*::can_reuse`).
//
// Every instance of this family is the same question asked over a different field list: the graph's input
// tensor may be reused for the incoming batch when, for each field, the input is absent or its extent equals
// the batch's. The native bodies spell that out as `res &= (!present) || (tensor->ne[k] == want);` repeated
// five to eight times, once per architecture.
//
// Here the fields are data. The caller gathers three arrays of equal length - whether the input exists, what
// extent the incoming batch needs, and what extent the existing tensor has - and Goose answers. Adding a field
// for a new architecture becomes an extra row on the host side, not another copy of the same conjunction.
//
// Packet:
//   k=0  0 n_fields
//   k=1  0 present[], 1 want[], 2 have[]   (three int64 arrays of n_fields)
//   k=2  0 out (one int64: 1 reuse, 0 rebuild)
//
// Specialisation: absent field never blocks reuse, whatever its wanted or actual extent says.

extern "grk_ld_i64" fn ld(base: u64, byte: i64) -> i64;
extern "grk_stw" fn stw(base: u64, byte: i64, v: i64);
extern "grk_fail" fn fail(code: i64);

fn ensure(v: bool, c: i64) {
    if !v {
        fail(c);
    }
}

fn check(n: i64, present: u64, want: u64, have: u64) -> i64 {
    ensure(n >= (0 as i64), 10);
    ensure(n <= (64 as i64), 11);

    var ok: i64 = 1;
    var i: i64 = 0;
    while i < n {
        let p: i64 = ld(present, i * (8 as i64));
        ensure(p == (0 as i64) || p == (1 as i64), 12);
        if p == (1 as i64) {
            let w: i64 = ld(want, i * (8 as i64));
            let h: i64 = ld(have, i * (8 as i64));
            if w != h {
                ok = 0;
            }
        }
        i = i + (1 as i64);
    }
    return ok;
}

fn run(packet: i64) -> bool {
    let g: u64 = (packet as u64);
    let n: i64 = ld(g + (0 as u64) * (128 as u64), 0 as i64);
    let present: u64 = ld(g + (1 as u64) * (128 as u64), 0 as i64) as u64;
    let want: u64 = ld(g + (1 as u64) * (128 as u64), 8 as i64) as u64;
    let have: u64 = ld(g + (1 as u64) * (128 as u64), 16 as i64) as u64;
    let out: u64 = ld(g + (2 as u64) * (128 as u64), 0 as i64) as u64;

    stw(out, 0 as i64, check(n, present, want, have));
    return true;
}

// Packet entry point, the pattern EXP-377 used: the caller fills a small fixed-layout packet and Goose
// answers. Returns 1 when the input can be reused, 0 when the graph must be rebuilt.
export fn gr_run(packet: i64) -> i64 {
    let ignored: bool = run(packet);
    if ignored {
        let out: u64 = ld((packet as u64) + (2 as u64) * (128 as u64), 0 as i64) as u64;
        return ld(out, 0 as i64);
    }
    return -1;
}
