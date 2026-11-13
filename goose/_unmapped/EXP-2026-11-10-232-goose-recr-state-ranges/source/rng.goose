// llama_memory_recurrent::state_write cell-range grouping in Goose (APX: P5-08).
//
// Native (llama-memory-recurrent.cpp:789-844): one pass over the cells builds three things at once --
// the count of matching cells, `cell_ranges` (contiguous index runs) and `cell_ranges_data` (contiguous
// logical-row runs).
//
// Per matching cell i the logical row is
//     cell_id = rs_idx_cur * size + (cell.src >= 0 ? cell.src : i)
// and a new data range starts ONLY when the next cell_id is not exactly the previous range's end. So two
// adjacent cells whose src jumps produce two ranges, while adjacent cells with consecutive ids merge into
// one. That discontinuity rule is the part worth gating: a naive "append a range per contiguous index run"
// gets the index ranges right and the data ranges wrong.
//
// The bridge reads the cell metadata (match, rs_idx_cur, src) into flat arrays, because the cell objects
// and their seq_id sets cannot cross the boundary. Everything the function decides is computed here.
extern "fg232r_fail" fn fail(code: i64);
extern "fg232r_ld_i32" fn ld_i32(base: u64, idx: i64) -> i64;
extern "fg232r_st_i32" fn st_i32(base: u64, idx: i64, v: i64);

fn ensure(v: bool, c: i64) {
    if !v {
        fail(c);
    }
}
// plan_ranges writes ranges as pairs (from,to) into out_ranges and out_data.
// Returns the number of matching cells; the range counts are written to counts[0] and counts[1].

fn plan_ranges(size: i64, match_: u64, rs: i64, src: u64, out_ranges: u64, out_data: u64, counts: u64) -> i64 {
    ensure(size >= 0, 1100);
    ensure(rs >= 0, 1101);
    var cell_count: i64 = 0;
    var n_rng: i64 = 0;
    var n_dat: i64 = 0;
    var begin: i64 = size;
    var dbegin: i64 = 0;
    var dend: i64 = 0;
    var have_dat: i64 = 0;
    var i: i64 = 0;

    while i < size {
        if ld_i32(match_, i) != 0 as i64 {
            cell_count = cell_count + 1 as i64;
            let s: i64 = ld_i32(src, i);
            var row: i64 = i;

            if s >= 0 as i64 {
                row = s;
            }
            let rsv: i64 = ld_i32(rs as u64, i);
            let cell_id: i64 = rsv * (size) + row;

            if have_dat == 0 as i64 {
                have_dat = 1 as i64;
                dbegin = cell_id;
                dend = cell_id + 1 as i64;
            } else {
                if dend != cell_id {
                    st_i32(out_data, n_dat * 2 as i64, dbegin);
                    st_i32(out_data, n_dat * 2 as i64 + 1 as i64, dend);
                    n_dat = n_dat + 1 as i64;
                    dbegin = cell_id;
                    dend = cell_id + 1 as i64;
                } else {
                    dend = dend + 1 as i64;
                }
            }

            if begin == size {
                begin = i;
            }
        } else {
            if begin != size {
                st_i32(out_ranges, n_rng * 2 as i64, begin);
                st_i32(out_ranges, n_rng * 2 as i64 + 1 as i64, i);
                n_rng = n_rng + 1 as i64;
                begin = size;
            }
        }
        i = i + 1 as i64;
    }

    if begin != size {
        st_i32(out_ranges, n_rng * 2 as i64, begin);
        st_i32(out_ranges, n_rng * 2 as i64 + 1 as i64, size);
        n_rng = n_rng + 1 as i64;
    }

    if have_dat != 0 as i64 {
        st_i32(out_data, n_dat * 2 as i64, dbegin);
        st_i32(out_data, n_dat * 2 as i64 + 1 as i64, dend);
        n_dat = n_dat + 1 as i64;
    }
    st_i32(counts, 0 as i64, n_rng);
    st_i32(counts, 1 as i64, n_dat);
    cell_count
}

fn main() {
    plan_ranges(0 as i64, 0 as u64, 0 as i64, 0 as u64, 0 as u64, 0 as u64, 0 as u64);
    plan_ranges(4 as i64, 8 as u64, 16 as i64, 24 as u64, 32 as u64, 48 as u64, 64 as u64);
    plan_ranges(6 as i64, 72 as u64, 80 as i64, 88 as u64, 96 as u64, 112 as u64, 128 as u64);
}

