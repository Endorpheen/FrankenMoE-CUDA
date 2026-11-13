// Apex KV tensor-layout arithmetic in Goose (state_write_data / state_read_data).
//
// Three spans cover every case this cache writes or reads:
//   row       a contiguous run of cells, offset = first * row, size = count * row
//   col       the transposed value layout, one row of elements across the cell range:
//             offset = (first + j * size) * el, size = count * el
//   outer     how many element rows the transposed layout visits
//
// Packet:
//   k=0  0 op, 1 a, 2 b, 3 c, 4 d, 5 e
//   k=1  0 out (two int64)
//   k=2  0 done, 1 ret
//
// Records: 0 span (a = op, b = offset, c = size)
extern "fg394k_get" fn get(p: i64, k: i64, i: i64) -> u64;
extern "fg394k_put" fn put(p: i64, k: i64, i: i64, v: u64);
extern "fg394k_ld_i64" fn ld(base: u64, byte: i64) -> i64;
extern "fg394k_stw" fn stw(base: u64, byte: i64, v: i64);
extern "fg394k_fail" fn fail(code: i64);

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

fn emit(out: u64, cntp: u64, kind: i64, a: i64, b: i64, c: i64) {
    let n: i64 = at(cntp, 0 as i64);
    setat(out, n * (4 as i64), kind);
    setat(out, n * (4 as i64) + 1 as i64, a);
    setat(out, n * (4 as i64) + 2 as i64, b);
    setat(out, n * (4 as i64) + 3 as i64, c);
    setat(cntp, 0 as i64, n + (1 as i64));
}

fn run(packet: i64) -> bool {
    let op: i64 = get(packet, 0, 0) as i64;
    let a: i64 = get(packet, 0, 1) as i64;
    let b: i64 = get(packet, 0, 2) as i64;
    let c: i64 = get(packet, 0, 3) as i64;
    let d: i64 = get(packet, 0, 4) as i64;
    let e: i64 = get(packet, 0, 5) as i64;
    let out: u64 = get(packet, 1, 0);
    let cntp: u64 = get(packet, 2, 0);
    var off: i64 = 0 as i64;
    var size: i64 = 0 as i64;

    if op == (0 as i64) {
        // row span: first, count, row size
        off = a * c;
        size = b * c;
    } else {
        if op == (1 as i64) {
            // transposed span: first, count, element size, j, cache size
            off = (a + d * e) * c;
            size = b * c;
        } else {
            if op == (2 as i64) {
                // element rows visited by the transposed layout
                off = a;
                size = a;
            } else {
                ensure(false, 60);
            }
        }
    }
    emit(out, cntp, 0 as i64, op, off, size);
    put(packet, 2, 0, 1 as u64);
    put(packet, 2, 1, off as u64);
    put(packet, 2, 2, size as u64);
    return true;
}

fn main() {
    run(0);
}

