// Apex KV batch preparation in Goose (llama_kv_cache::prepare, P5-09).
//
// prepare is a transaction. Each ubatch asks for slots first and only then writes: the slot layout is
// kept, the touched cells are copied aside, the ubatch is applied, and the next ubatch sees the
// mutated cache. If any slot search comes back empty, everything is undone — in reverse order, so the
// first snapshot wins — and the result is empty rather than partial.
//
// The head of every stream is restored from the copy taken before that ubatch, not from the value the
// last applied ubatch left behind. That is why the whole per-stream head vector is snapshotted, not
// only the heads of the streams the ubatch touched.
//
// Packet, int64 slots:
//   k=0  0 n_ub, 1 ub_ncells, 2 ub_find_ok, 3 cell_stream, 4 cell_idx, 5 cell_old, 6 cell_new,
//        7 cell_off, 8 heads_old, 9 head_stride, 10 n_stream
//   k=1  0 out
//   k=2  0 count ptr
//   k=3  0 done, 1 ret (number of prepared ubatches, -1 when the result is empty)
//
// Records, three slots:
//   0 slot search succeeded (a = ubatch)
//   1 snapshot (a = cell ordinal), 2 apply (a = ubatch), 3 restore (a = cell ordinal),
//   4 head restored (a = stream, b = old value)
extern "fg330k_get" fn get(p: i64, k: i64, i: i64) -> u64;
extern "fg330k_put" fn put(p: i64, k: i64, i: i64, v: u64);
extern "fg330k_ld_i64" fn ld(base: u64, byte: i64) -> i64;
extern "fg330k_stw" fn stw(base: u64, byte: i64, v: i64);
extern "fg330k_fail" fn fail(code: i64);

fn ensure(v: bool, c: i64) {
    if !v {
        fail(c);
    }
}

fn at(arr: u64, i: i64) -> i64 {
    return ld(arr, i * (8 as i64));
}

fn setat(arr: u64, i: i64, v: i64) {
    stw(arr, i * (8 as i64), v);
}

fn emit(out: u64, cntp: u64, kind: i64, a: i64, b: i64) {
    let n: i64 = at(cntp, 0 as i64);
    setat(out, n * (3 as i64), kind);
    setat(out, n * (3 as i64) + 1 as i64, a);
    setat(out, n * (3 as i64) + 2 as i64, b);
    setat(cntp, 0 as i64, n + (1 as i64));
}

fn run(packet: i64) -> bool {
    let n_ub: i64 = get(packet, 0, 0) as i64;
    let ub_ncells: u64 = get(packet, 0, 1);
    let ub_find: u64 = get(packet, 0, 2);
    let cell_off: u64 = get(packet, 0, 7);
    let heads: u64 = get(packet, 0, 8);
    let hs: i64 = get(packet, 0, 9) as i64;
    let n_stream: i64 = get(packet, 0, 10) as i64;
    let out: u64 = get(packet, 1, 0);
    let cntp: u64 = get(packet, 2, 0);
    ensure(n_ub >= (0 as i64) && n_stream > (0 as i64) && hs >= n_stream, 60);
    ensure(ub_ncells != (0 as u64) && ub_find != (0 as u64) && cell_off != (0 as u64), 61);
    var done_ub: i64 = 0;
    var failed: i64 = 0;
    var u: i64 = 0;

    while u < n_ub {
        if at(ub_find, u) == (0 as i64) {
            failed = 1 as i64;
            u = n_ub;
        } else {
            emit(out, cntp, 0 as i64, u, 0 as i64);
            let off: i64 = at(cell_off, u);
            let nc: i64 = at(ub_ncells, u);
            var c: i64 = 0;

            while c < nc {
                emit(out, cntp, 1 as i64, off + c, 0 as i64);
                c = c + 1 as i64;
            }
            emit(out, cntp, 2 as i64, u, 0 as i64);
            done_ub = done_ub + 1 as i64;
            u = u + 1 as i64;
        }
    }

    if failed != (0 as i64) {
        // undo in reverse order
        var ru: i64 = done_ub - (1 as i64);

        while ru >= (0 as i64) {
            let off: i64 = at(cell_off, ru);
            let nc: i64 = at(ub_ncells, ru);
            var c: i64 = 0;

            while c < nc {
                emit(out, cntp, 3 as i64, off + c, 0 as i64);
                c = c + 1 as i64;
            }
            var s: i64 = 0;

            while s < n_stream {
                emit(out, cntp, 4 as i64, s, at(heads, ru * hs + s));
                s = s + 1 as i64;
            }
            ru = ru - 1 as i64;
        }
        put(packet, 3, 0, 1 as u64);
        put(packet, 3, 1, ( (0 as i64) - 1 as i64) as u64);
        return true;
    }
    put(packet, 3, 0, 1 as u64);
    put(packet, 3, 1, done_ub as u64);
    return true;
}

fn main() {
    run(0);
}

