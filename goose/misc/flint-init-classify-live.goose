// Apex batch entry classification in Goose (llama_batch_allocr::init, content-type section).
//
// Every entry carries a token id, an embedding, or both, and all entries must agree about the state
// embedding. The rules, in native order:
//
//   1 an entry with neither a token id nor an embedding is refused;
//   2 a batch must be uniform about the state embedding;
//   3 entries carrying **both** a token and an embedding cannot be mixed with the others — it is all
//     of them or none;
//   4 a batch mixing token-only and embedding-only entries is refused when the model does not allow
//     mixed batches;
//   5 then `mixed` (both kinds present) decides whether a per-entry embedding flag array is built, and
//     `has_token` / `has_embd` follow from which kinds are present.
//
// Two later checks belong to the same decision and are included: a token id must lie in
// `[0, n_vocab)` — skipped for an embedding-marked entry, which stores a placeholder instead — and a
// sequence id must lie in `[0, n_seq_max)`.
//
// Packet:
//   k=0  0 n, 1 is_tok, 2 has_embd, 3 has_state, 4 id, 5 n_vocab, 6 nseqid, 7 seqid, 8 n_seq_max,
//        9 allow_mixed
//   k=1  0 out, 1 cnt
//   k=2  0 done, 1 ret (0 ok, negative rejection code)
//
// Records: 0 counts (a = 0 tok-only / 1 embd-only / 2 both, b = value), 1 flags (a = 0 mixed /
// 1 has_token / 2 has_embd / 3 has_state, b = value), 2 embedding flag (a = entry, b = value),
// 3 token placeholder (a = entry), 4 placement (a = entry), 5 seq check (a = entry, b = seq id).
extern "fg352k_get" fn get(p: i64, k: i64, i: i64) -> u64;
extern "fg352k_put" fn put(p: i64, k: i64, i: i64, v: u64);
extern "fg352k_ld_i64" fn ld(base: u64, byte: i64) -> i64;
extern "fg352k_stw" fn stw(base: u64, byte: i64, v: i64);
extern "fg352k_fail" fn fail(code: i64);

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

fn done(packet: i64, cntp: u64, code: i64) {
    put(packet, 2, 0, 1 as u64);
    put(packet, 2, 1, ( (0 as i64) - code as i64) as u64);
}

fn run(packet: i64) -> bool {
    let n: i64 = get(packet, 0, 0) as i64;
    let is_tok: u64 = get(packet, 0, 1);
    let has_embd: u64 = get(packet, 0, 2);
    let hs_ptr: u64 = get(packet, 0, 3);
    let id: u64 = get(packet, 0, 4);
    let n_vocab: i64 = get(packet, 0, 5) as i64;
    let nseqid: u64 = get(packet, 0, 6);
    let seqid: u64 = get(packet, 0, 7);
    let n_seq_max: i64 = get(packet, 0, 8) as i64;
    let allow_mixed: i64 = get(packet, 0, 9) as i64;
    let out: u64 = get(packet, 1, 0);
    let cntp: u64 = get(packet, 1, 1);
    ensure(n > (0 as i64), 60);
    // the batch is uniform about the state embedding, so entry zero defines it
    let state_expected: i64 = at(hs_ptr, 0 as i64);
    // 1..3: per-entry kinds and the uniformity rules
    var n_tok_only: i64 = 0 as i64;
    var n_embd_only: i64 = 0 as i64;
    var n_both: i64 = 0 as i64;
    var i: i64 = 0;

    while i < n {
        let t: i64 = at(is_tok, i);
        let e: i64 = at(has_embd, i);

        if t == (0 as i64) && e == (0 as i64) {
            done(packet, cntp, 1 as i64);
            return true;
        }

        if at(hs_ptr, i) != state_expected {
            done(packet, cntp, 2 as i64);
            return true;
        }

        if t != (0 as i64) && e == (0 as i64) {
            n_tok_only = n_tok_only + (1 as i64);
        }

        if e != (0 as i64) && t == (0 as i64) {
            n_embd_only = n_embd_only + (1 as i64);
        }

        if t != (0 as i64) && e != (0 as i64) {
            n_both = n_both + (1 as i64);
        }
        i = i + 1 as i64;
    }
    emit(out, cntp, 0 as i64, 0 as i64, n_tok_only);
    emit(out, cntp, 0 as i64, 1 as i64, n_embd_only);
    emit(out, cntp, 0 as i64, 2 as i64, n_both);

    if n_both > (0 as i64) && n_both != n {
        done(packet, cntp, 3 as i64);
        return true;
    }
    var mixed: i64 = 0 as i64;

    if n_tok_only > (0 as i64) && n_embd_only > (0 as i64) {
        mixed = 1 as i64;
    }

    if mixed != (0 as i64) && allow_mixed == (0 as i64) {
        done(packet, cntp, 4 as i64);
        return true;
    }
    var has_token: i64 = 0 as i64;

    if n_tok_only > (0 as i64) || n_both > (0 as i64) {
        has_token = 1 as i64;
    }
    var has_embd_f: i64 = 0 as i64;

    if n_embd_only > (0 as i64) || n_both > (0 as i64) {
        has_embd_f = 1 as i64;
    }
    emit(out, cntp, 1 as i64, 0 as i64, mixed);
    emit(out, cntp, 1 as i64, 1 as i64, has_token);
    emit(out, cntp, 1 as i64, 2 as i64, has_embd_f);
    emit(out, cntp, 1 as i64, 3 as i64, state_expected);
    // 5: the per-entry embedding flags exist only for a mixed batch

    if mixed != (0 as i64) {
        var i2: i64 = 0;

        while i2 < n {
            emit(out, cntp, 2 as i64, i2, at(has_embd, i2));
            i2 = i2 + 1 as i64;
        }
    }
    // token ids are only read where a token is actually used

    if has_token != (0 as i64) {
        var i3: i64 = 0;

        while i3 < n {
            var emb_marked: i64 = 0 as i64;

            if mixed != (0 as i64) {
                emb_marked = at(has_embd, i3);
            }

            if emb_marked != (0 as i64) {
                emit(out, cntp, 3 as i64, i3, 0 as i64);
            } else {
                let v: i64 = at(id, i3);

                if v < (0 as i64) || v >= n_vocab {
                    done(packet, cntp, 5 as i64);
                    return true;
                }
                emit(out, cntp, 4 as i64, i3, v);
            }
            i3 = i3 + 1 as i64;
        }
    }
    // sequence ids
    var i4: i64 = 0;

    while i4 < n {
        var j: i64 = 0;

        while j < at(nseqid, i4) {
            let sid: i64 = at(seqid, i4 * (4 as i64) + j);

            if sid < (0 as i64) || sid >= n_seq_max {
                done(packet, cntp, 6 as i64);
                return true;
            }
            emit(out, cntp, 5 as i64, i4, sid);
            j = j + 1 as i64;
        }
        i4 = i4 + 1 as i64;
    }
    put(packet, 2, 0, 1 as u64);
    put(packet, 2, 1, 0 as i64 as u64);
    return true;
}

fn main() {
    run(0);
}

