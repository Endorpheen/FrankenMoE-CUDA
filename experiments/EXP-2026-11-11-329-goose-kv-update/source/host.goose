// Apex KV update in Goose (llama_kv_cache::update, P5-09).
//
// A mirrored cache reports success without doing anything.
//
// Stream copies come first: for every requested pair, every layer copies its key tensor and — only
// where a value tensor exists — its value tensor. A pair never copies a stream onto itself and both
// ends must be valid streams.
//
// The shift pass is guarded by can_shift: Step35 and multi-position-per-embedding models cannot
// shift, and a shift requested there is a hard abort, not a false return. A shift graph is only built
// when RoPE is enabled and at least one layer uses it; a failed allocation or a failed compute returns
// the current "updated" flag unchanged. The shift counters are reset on every stream in either case.
//
// The return value is whether a shift graph actually ran — a pure stream copy returns false.
//
// Packet, int64 slots:
//   k=0  0 op, 1 other, 2 n_stream, 3 n_layers, 4 pairs, 5 n_pairs, 6 do_shift, 7 can_shift,
//        8 rope_enabled, 9 any_layer_rope, 10 alloc_ok, 11 compute_ok, 12 v_present, 13 shifts
//   k=1  0 out
//   k=2  0 count ptr
//   k=3  0 done, 1 ret
//
// Records, three slots: 0 key copied (a = src stream, b = dst stream), 1 value copied (same),
// 2 shift graph computed, 3 shift counters reset (a = stream).
extern "fg329k_get" fn get(p: i64, k: i64, i: i64) -> u64;
extern "fg329k_put" fn put(p: i64, k: i64, i: i64, v: u64);
extern "fg329k_ld_i64" fn ld(base: u64, byte: i64) -> i64;
extern "fg329k_stw" fn stw(base: u64, byte: i64, v: i64);
extern "fg329k_fail" fn fail(code: i64);

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
    let op: i64 = get(packet, 0, 0) as i64;
    let other: i64 = get(packet, 0, 1) as i64;
    let n_stream: i64 = get(packet, 0, 2) as i64;
    let n_layers: i64 = get(packet, 0, 3) as i64;
    let pairs: u64 = get(packet, 0, 4);
    let n_pairs: i64 = get(packet, 0, 5) as i64;
    let do_shift: i64 = get(packet, 0, 6) as i64;
    let can_shift: i64 = get(packet, 0, 7) as i64;
    let rope_enabled: i64 = get(packet, 0, 8) as i64;
    let any_rope: i64 = get(packet, 0, 9) as i64;
    let alloc_ok: i64 = get(packet, 0, 10) as i64;
    let compute_ok: i64 = get(packet, 0, 11) as i64;
    let v_present: i64 = get(packet, 0, 12) as i64;
    let out: u64 = get(packet, 1, 0);
    let cntp: u64 = get(packet, 2, 0);
    var ret: i64 = 0;

    if op == (0 as i64) {
        if other != (0 as i64) {
            put(packet, 3, 0, 1 as u64);
            put(packet, 3, 1, (1 as i64) as u64);
            return true;
        }
        var updated: i64 = 0;

        if n_pairs > (0 as i64) {
            ensure(n_stream > (1 as i64), 60);
            var p: i64 = 0;

            while p < n_pairs {
                let ssrc: i64 = at(pairs, (p * (2 as i64)));
                let sdst: i64 = at(pairs, (p * (2 as i64) + 1 as i64));
                ensure(ssrc >= (0 as i64) && ssrc < n_stream, 61);
                ensure(sdst >= (0 as i64) && sdst < n_stream, 62);
                ensure(ssrc != sdst, 63);
                var il: i64 = 0;

                while il < n_layers {
                    emit(out, cntp, 0 as i64, ssrc, sdst);

                    if v_present != (0 as i64) {
                        emit(out, cntp, 1 as i64, ssrc, sdst);
                    }
                    il = il + 1 as i64;
                }
                p = p + 1 as i64;
            }
        }

        if do_shift != (0 as i64) {
            if can_shift == (0 as i64) {
                // native aborts; the port reports -7 so the caller cannot miss it
                put(packet, 3, 0, 1 as u64);
                put(packet, 3, 1, ( (0 as i64) - 7 as i64) as u64);
                return true;
            }

            if rope_enabled != (0 as i64) {
                if any_rope != (0 as i64) {
                    if alloc_ok == (0 as i64) {
                        // allocation failed: no shift ran, counters still reset below
                    } else {
                        if compute_ok == (0 as i64) {
                            // compute failed: no shift ran, counters still reset below
                        } else {
                            emit(out, cntp, 2 as i64, 0 as i64, 0 as i64);
                            updated = 1 as i64;
                        }
                    }
                }
            }
            var s: i64 = 0;

            while s < n_stream {
                emit(out, cntp, 3 as i64, s, 0 as i64);
                s = s + 1 as i64;
            }
        }
        ret = updated;
    } else {
        if op == (1 as i64) {
            // can_shift: Step35 and multi-position models cannot shift
            let step35: i64 = get(packet, 0, 13) as i64;
            let n_pos: i64 = get(packet, 0, 14) as i64;
            var ok: i64 = 1 as i64;

            if step35 != (0 as i64) {
                ok = 0 as i64;
            }

            if n_pos > (1 as i64) {
                ok = 0 as i64;
            }
            ret = ok;
        } else {
            if op == (2 as i64) {
                // has_shift: any stream carries a pending shift
                let shifts: u64 = get(packet, 0, 13);
                var any: i64 = 0;
                var s: i64 = 0;

                while s < n_stream {
                    if at(shifts, s) != (0 as i64) {
                        any = 1 as i64;
                    }
                    s = s + 1 as i64;
                }
                ret = any;
            } else {
                ensure(false, 65);
            }
        }
    }
    put(packet, 3, 0, 1 as u64);
    put(packet, 3, 1, ret as u64);
    return true;
}

fn main() {
    run(0);
}

