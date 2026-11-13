// Apex KV batch initialisation in Goose (llama_kv_cache::init_batch, P5-09).
//
// init_batch is a policy around the batch allocator, and the allocator itself is out of scope here —
// it is an input: a stream of ubatches, each with a token count and a unique-sequence count, ending
// at a zero-token ubatch.
//
// The split strategy is chosen once: a single stream splits simply, a single-assignment batch with
// several streams splits by sequence, and everything else splits equally. The ubatches are drained
// until the zero one, then two things can still fail: the allocator may not have placed every token
// (used < total), and slot preparation may come back empty. Both give the same "failed to prepare"
// result — an empty context, never a partial batch.
//
// A successful batch reports the strategy that was used and the ubatches it holds.
//
// Packet, int64 slots:
//   k=0  0 n_stream, 1 n_ubatch, 2 embd_all, 3 ub_tokens, 4 ub_seqs, 5 n_ub, 6 n_used, 7 n_tokens,
//        8 prepare_ok
//   k=1  0 out
//   k=2  0 count ptr
//   k=3  0 done, 1 ret (0 = ok, 1 = failed to prepare)
//
// Records, three slots: 0 strategy chosen (a = 0 simple, 1 seq, 2 equal), 1 ubatch taken
// (a = tokens, b = unique sequences), 2 tokens placed (a = used, b = total), 3 preparation
// refused, 4 preparation accepted.
extern "fg331k_get" fn get(p: i64, k: i64, i: i64) -> u64;
extern "fg331k_put" fn put(p: i64, k: i64, i: i64, v: u64);
extern "fg331k_ld_i64" fn ld(base: u64, byte: i64) -> i64;
extern "fg331k_stw" fn stw(base: u64, byte: i64, v: i64);
extern "fg331k_fail" fn fail(code: i64);

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
    let n_stream: i64 = get(packet, 0, 0) as i64;
    let embd_all: i64 = get(packet, 0, 2) as i64;
    let ub_tokens: u64 = get(packet, 0, 3);
    let ub_seqs: u64 = get(packet, 0, 4);
    let n_ub: i64 = get(packet, 0, 5) as i64;
    let n_used: i64 = get(packet, 0, 6) as i64;
    let n_tokens: i64 = get(packet, 0, 7) as i64;
    let prepare_ok: i64 = get(packet, 0, 8) as i64;
    let out: u64 = get(packet, 1, 0);
    let cntp: u64 = get(packet, 2, 0);
    ensure(n_stream > (0 as i64) && n_ub >= (0 as i64), 60);
    ensure(ub_tokens != (0 as u64) && ub_seqs != (0 as u64), 61);
    var strategy: i64 = 2;

    if n_stream == (1 as i64) {
        strategy = 0;
    } else {
        if embd_all != (0 as i64) {
            strategy = 1;
        }
    }
    emit(out, cntp, 0 as i64, strategy, 0 as i64);
    var taken: i64 = 0;
    var i: i64 = 0;

    while i < n_ub {
        let t: i64 = at(ub_tokens, i);

        if t == (0 as i64) {
            i = n_ub;
            // the zero ubatch ends the drain
        } else {
            emit(out, cntp, 1 as i64, t, at(ub_seqs, i));
            taken = taken + 1 as i64;
            i = i + 1 as i64;
        }
    }

    if n_used < n_tokens {
        emit(out, cntp, 2 as i64, n_used, n_tokens);
        emit(out, cntp, 3 as i64, 0 as i64, 0 as i64);
        put(packet, 3, 0, 1 as u64);
        put(packet, 3, 1, 1 as i64 as u64);
        return true;
    }

    if prepare_ok == (0 as i64) {
        emit(out, cntp, 3 as i64, 1 as i64, 0 as i64);
        put(packet, 3, 0, 1 as u64);
        put(packet, 3, 1, 1 as i64 as u64);
        return true;
    }
    emit(out, cntp, 4 as i64, taken, 0 as i64);
    put(packet, 3, 0, 1 as u64);
    put(packet, 3, 1, 0 as i64 as u64);
    return true;
}

fn main() {
    run(0);
}

