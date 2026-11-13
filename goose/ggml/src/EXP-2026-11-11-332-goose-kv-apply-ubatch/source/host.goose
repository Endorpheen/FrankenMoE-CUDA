// Apex KV ubatch application in Goose (llama_kv_cache::apply_ubatch, P5-06/P5-09 core).
//
// Applying a ubatch writes cells, then repairs the cache invariant it just broke.
//
// Per cell: an occupied destination is emptied first, and the highest position it held is recorded
// per sequence. Then the new position is set, the extension is written when the batch carries 2-D
// positions, tokens or PLE heads, and the batch's sequences are added to the cell.
//
// The extension rule is the subtle part: an embedding batch has no token ids, so the token is only
// stored when the batch has one and the entry is not an embedding; for PLE the missing id is filled
// from the image token, and from EOS when no image token is configured.
//
// Afterwards, for every sequence whose position was overwritten, everything below that position is
// purged: the native invariant is that all positions between the sequence minimum and maximum exist,
// so a hole below the overwritten point must be removed. The purge reuses seq_rm, range end exclusive.
//
// Finally the head of each touched stream moves past the last slot of the batch.
//
// Records, three slots: 0 cell emptied (a = global cell, b = old position), 1 position set,
// 2 extension set (a = global cell, b = token), 3 sequence added (a = global cell, b = seq_id),
// 4 purge (a = seq_id, b = from), 5 purge end (a = to), 6 head (a = stream, b = value).
//
// Packet:
//   k=0  0 n_stream, 1 size, 2 strm, 3 idxs, 4 cells_pos, 5 cells_mask, 6 head, 7 s2s,
//        8 ub_pos, 9 ub_nseq, 10 ub_seqids, 11 ub_token, 12 ub_type, 13 cell_ext, 14 cells_per_stream
//   k=1  0 out, 1 is_pos2d, 2 is_mixed, 3 token_present, 4 ple_n_heads, 5 ple_image_tok,
//        6 ple_eos_tok, 7 n_tok
//   k=2  0 count ptr
//   k=3  0 done, 1 ret
extern "fg332k_get" fn get(p: i64, k: i64, i: i64) -> u64;
extern "fg332k_put" fn put(p: i64, k: i64, i: i64, v: u64);
extern "fg332k_ld_i64" fn ld(base: u64, byte: i64) -> i64;
extern "fg332k_stw" fn stw(base: u64, byte: i64, v: i64);
extern "fg332k_fail" fn fail(code: i64);

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
    let size: i64 = get(packet, 0, 1) as i64;
    let strm: u64 = get(packet, 0, 2);
    let idxs: u64 = get(packet, 0, 3);
    let cpos: u64 = get(packet, 0, 4);
    let cmask: u64 = get(packet, 0, 5);
    let head: u64 = get(packet, 0, 6);
    let s2s: u64 = get(packet, 0, 7);
    let ub_pos: u64 = get(packet, 0, 8);
    let ub_nseq: u64 = get(packet, 0, 9);
    let ub_seqids: u64 = get(packet, 0, 10);
    let ub_token: u64 = get(packet, 0, 11);
    let ub_type: u64 = get(packet, 0, 12);
    let ext: u64 = get(packet, 0, 13);
    let cps: i64 = get(packet, 0, 14) as i64;
    let out: u64 = get(packet, 1, 0);
    let pos2d: i64 = get(packet, 1, 1) as i64;
    let is_mixed: i64 = get(packet, 1, 2) as i64;
    let tok_present: i64 = get(packet, 1, 3) as i64;
    let ple_n: i64 = get(packet, 1, 4) as i64;
    let ple_img: i64 = get(packet, 1, 5) as i64;
    let ple_eos: i64 = get(packet, 1, 6) as i64;
    let n_tok: i64 = get(packet, 1, 7) as i64;
    let cntp: u64 = get(packet, 2, 0);
    ensure(n_stream > (0 as i64) && size >= (0 as i64) && cps > (0 as i64), 60);
    ensure(n_tok == n_stream * size, 61);
    // highest overwritten position per sequence, -1 when none
    var maxrm: u64 = get(packet, 3, 7);
    ensure(maxrm != (0 as u64), 62);
    var q: i64 = 0;

    while q < 64 as i64 {
        setat(maxrm, q, (0 as i64) - 1 as i64);
        q = q + 1 as i64;
    }
    // write the batch
    var s: i64 = 0;

    while s < n_stream {
        let stream: i64 = at(strm, s);
        let base: i64 = stream * cps;
        var ii: i64 = 0;

        while ii < size {
            let i: i64 = s * size + ii;
            let idx: i64 = at(idxs, i);
            let g: i64 = base + idx;
            let p: i64 = at(cpos, g);
            let mask: i64 = at(cmask, g);

            if p != (0 as i64) - 1 as i64 {
                // occupied: exactly one sequence, remember its position, empty the cell
                ensure( (mask & (mask - (1 as i64))) == (0 as i64), 63);
                var sid: i64 = 0;
                var found: i64 = (0 as i64) - 1 as i64;

                while sid < 64 as i64 {
                    if ( (mask & ( (1 as i64) << sid)) != (0 as i64)) {
                        found = sid;
                    }
                    sid = sid + 1 as i64;
                }
                ensure(found >= (0 as i64), 64);
                emit(out, cntp, 0 as i64, g, p);
                let cur: i64 = at(maxrm, found);

                if p > cur {
                    setat(maxrm, found, p);
                }
                setat(cmask, g, 0 as i64);
                setat(cpos, g, (0 as i64) - 1 as i64);
            }
            setat(cpos, g, at(ub_pos, i));
            emit(out, cntp, 1 as i64, g, at(ub_pos, i));

            if pos2d != (0 as i64) || tok_present != (0 as i64) || ple_n > (0 as i64) {
                var x: i64 = 0 as i64;
                var y: i64 = 0 as i64;

                if pos2d != (0 as i64) {
                    x = at(ub_pos, i + n_tok * (2 as i64));
                    y = at(ub_pos, i + n_tok);
                }
                var is_embd: bool = false;

                if tok_present == (0 as i64) {
                    is_embd = true;
                } else {
                    if is_mixed != (0 as i64) {
                        if at(ub_type, i) != (0 as i64) {
                            is_embd = true;
                        }
                    }
                }
                var tok: i64 = 0 as i64;

                if !is_embd {
                    tok = at(ub_token, i);
                } else {
                    if ple_n > (0 as i64) {
                        if ple_img != (0 as i64) {
                            tok = ple_img;
                        } else {
                            tok = ple_eos;
                        }
                    }
                }
                setat(ext, g * (3 as i64), x);
                setat(ext, g * (3 as i64) + 1 as i64, y);
                setat(ext, g * (3 as i64) + 2 as i64, tok);
                emit(out, cntp, 2 as i64, g, tok);
            }
            var j: i64 = 0;

            while j < at(ub_nseq, i) {
                let sid2: i64 = at(ub_seqids, i * (4 as i64) + j);
                ensure(sid2 >= (0 as i64) && sid2 < (64 as i64), 65);
                ensure( (at(cmask, g) & ( (1 as i64) << sid2)) == (0 as i64), 66);
                setat(cmask, g, at(cmask, g) | ( (1 as i64) << sid2));
                emit(out, cntp, 3 as i64, g, sid2);
                j = j + 1 as i64;
            }
            ii = ii + 1 as i64;
        }
        s = s + 1 as i64;
    }
    // purge below every overwritten position
    var sq: i64 = 0;

    while sq < 64 as i64 {
        let m: i64 = at(maxrm, sq);

        if m != (0 as i64) - 1 as i64 {
            let stream2: i64 = at(s2s, sq);
            let base2: i64 = stream2 * cps;
            // seq_pos_min over the cells that hold this sequence
            var mn: i64 = (1 as i64) << 62 as i64;
            var found2: i64 = 0 as i64;
            var c: i64 = 0;

            while c < cps {
                let g2: i64 = base2 + c;

                if at(cpos, g2) >= (0 as i64) {
                    if (at(cmask, g2) & ( (1 as i64) << sq)) != (0 as i64) {
                        if at(cpos, g2) < mn {
                            mn = at(cpos, g2);
                        }
                        found2 = 1 as i64;
                    }
                }
                c = c + 1 as i64;
            }

            if found2 != (0 as i64) {
                if mn <= m {
                    emit(out, cntp, 4 as i64, sq, mn);
                    emit(out, cntp, 5 as i64, m + (1 as i64), 0 as i64);
                    // seq_rm(sq, mn, m+1)
                    var c2: i64 = 0;

                    while c2 < cps {
                        let g3: i64 = base2 + c2;
                        let p3: i64 = at(cpos, g3);

                        if p3 >= mn && p3 < m + (1 as i64) {
                            var mask3: i64 = at(cmask, g3);

                            if (mask3 & ( (1 as i64) << sq)) != (0 as i64) {
                                mask3 = mask3 ^ ( ( (1 as i64) << sq));

                                if mask3 == (0 as i64) {
                                    setat(cpos, g3, (0 as i64) - 1 as i64);
                                }
                                setat(cmask, g3, mask3);
                            }
                        }
                        c2 = c2 + 1 as i64;
                    }
                }
            }
        }
        sq = sq + 1 as i64;
    }
    // head moves past the last slot of each stream
    s = 0;

    while s < n_stream {
        let stream4: i64 = at(strm, s);
        let v: i64 = at(idxs, (s + 1 as i64) * size - (1 as i64)) + (1 as i64);
        setat(head, stream4, v);
        emit(out, cntp, 6 as i64, stream4, v);
        s = s + 1 as i64;
    }
    put(packet, 3, 0, 1 as u64);
    put(packet, 3, 1, 0 as i64 as u64);
    return true;
}

fn main() {
    run(0);
}

