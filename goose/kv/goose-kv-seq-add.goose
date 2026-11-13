// llama_memory_recurrent::seq_add in Goose (APX: P5).
//
// Native (llama-memory-recurrent.cpp:334):
//     if (shift == 0) return;                       // no shift, no work
//     if (p0 < 0) p0 = 0;                           // clamp both bounds
//     if (p1 < 0) p1 = INT64_MAX;
//     if (p0 == p1) return;                         // empty range
//     if (0 <= seq_id && seq_id < size) {
//         tail = cells[seq_id].tail;
//         if (tail >= 0) {
//             cell = cells[tail];
//             if (cell.has_seq_id(seq_id) && p0 <= cell.pos && cell.pos < p1) cell.pos += shift;
//         }
//     }
//
// The port returns the NEW position and a flag saying whether to write it, so the caller keeps ownership
// of the cell. Three traps the gate covers: a negative p1 means "no upper bound" rather than an empty
// range; p0 == p1 after clamping is an early exit, so a zero-width range must not write; and the shift
// is only applied when the cell also still belongs to this sequence.
extern "fg231a_fail" fn fail(code: i64);

fn ensure(v: bool, c: i64) {
    if !v {
        fail(c);
    }
}
// seq_add decides: returns 1 when pos should be replaced by out[0], 0 when nothing changes.

fn seq_add(
    shift: i64,
    p0_in: i64,
    p1_in: i64,
    seq_id: i64,
    size: i64,
    has_tail: i64,
    tail_pos: i64,
    tail_has_seq: i64,
    out: u64) -> i64 {
    ensure(size >= 0, 1000);
    var p0: i64 = p0_in;
    var p1: i64 = p1_in;

    if shift == 0 as i64 {
        return 0 as i64;
    }

    if p0 < 0 as i64 {
        p0 = 0 as i64;
    }

    if p1 < 0 as i64 {
        p1 = 9223372036854775807 as i64;
    }

    if p0 == p1 {
        return 0 as i64;
    }

    if seq_id < 0 as i64 {
        return 0 as i64;
    }

    if seq_id >= size {
        return 0 as i64;
    }

    if has_tail == 0 as i64 {
        return 0 as i64;
    }

    if tail_has_seq == 0 as i64 {
        return 0 as i64;
    }

    if p0 <= tail_pos && tail_pos < p1 {
        extern_call_st_i64(out, tail_pos + shift);
        return 1 as i64;
    }
    return 0 as i64;
}
extern "fg231a_st_i64" fn extern_call_st_i64(base: u64, v: i64);

fn main() {
    seq_add(0 as i64, 0 as i64, 0 as i64, 0 as i64, 0 as i64, 0 as i64, 0 as i64, 0 as i64, 0 as u64);
    seq_add(5 as i64, 0 as i64, 0 as i64, 0 as i64, 4 as i64, 1 as i64, 2 as i64, 1 as i64, 16 as u64);
    seq_add(5 as i64, -1 as i64, -1 as i64, 1 as i64, 4 as i64, 1 as i64, 9 as i64, 1 as i64, 32 as u64);
}

