// P9-03 S2: dense (non-expert) weight placement and source selection, from
// runtime/core/src/moe/dense_weights.cpp.
//
// The module owns ONE question — which bytes are resident, where, and by whose
// policy — and answers it in a fixed order:
//
//   byte_ranges           20  the dense set is the complement of the expert
//                              ranges in [0, file_size), trailing tail included
//   hold_back_oversized  128  a tensor larger than MemAvailable cannot be
//                              resident under any mode: leave it mmap'd,
//                              subtract it from the dense ranges
//   take_row_gathered    172  a table the graph only row-gathers leaves the
//                              resident set entirely and is served from flash
//   init verdict          52  anon/pinned read, warm sweep, or nothing at all
//
// What is NOT here is the disk and the kernel: FileReader, O_DIRECT, read(),
// vm_page(), vm_advise()/madvise, mincore, /proc/self/maps and the pinned
// dma-buf allocator. `avail`, `pinned_ok` and `takeover_ok` are arguments,
// because on the native side they are the kernel's answers, not decisions.
//
// Range encoding: two parallel i64 slices, [a, b) half-open, ascending.
// Tensor codes from dw_classify: 0 resident, 1 oversized-mmap, 2 row-stream.

const DW_MMAP: i64 = 0;
const DW_WARMED: i64 = 1;
const DW_ANON: i64 = 2;
const DW_PINNED: i64 = 3;

// std::sort on std::pair<uint64_t, uint64_t>: ascending by first, then second.
fn dw_sort_pairs(a: i64[:], b: i64[:], n: i64) {
    var i: i64 = 1;

    while i < n {
        let ka: i64 = a[i];
        let kb: i64 = b[i];
        var j: i64 = i;

        while j > 0 && (a[j - 1] > ka || (a[j - 1] == ka && b[j - 1] > kb)) {
            a[j] = a[j - 1];
            b[j] = b[j - 1];
            j = j - 1;
        }
        a[j] = ka;
        b[j] = kb;
        i = i + 1;
    }
}

// byte_ranges: the gaps before each expert range plus the trailing tail. The
// input is sorted in place, as the native function sorts its by-value argument.
export fn dw_byte_ranges(ea: i64[:], eb: i64[:], n: i64, file_size: i64,
                         oa: i64[:], ob: i64[:]) -> i64 {
    dw_sort_pairs(ea, eb, n);
    var pos: i64 = 0;
    var k: i64 = 0;
    var i: i64 = 0;

    while i < n {
        if ea[i] > pos {
            oa[k] = pos;
            ob[k] = ea[i];
            k = k + 1;
        }
        if eb[i] > pos {
            pos = eb[i];
        }
        i = i + 1;
    }
    if pos < file_size {
        oa[k] = pos;
        ob[k] = file_size;
        k = k + 1;
    }
    return k;
}

// subtract_range: remove [a, b) from a sorted list of disjoint ranges,
// splitting the one range it falls in and keeping every disjoint range.
export fn dw_subtract(ra: i64[:], rb: i64[:], n: i64, a: i64, b: i64,
                      oa: i64[:], ob: i64[:]) -> i64 {
    var k: i64 = 0;
    var i: i64 = 0;

    while i < n {
        if b <= ra[i] || a >= rb[i] {
            oa[k] = ra[i];
            ob[k] = rb[i];
            k = k + 1;
        } else {
            if ra[i] < a {
                oa[k] = ra[i];
                ob[k] = a;
                k = k + 1;
            }
            if b < rb[i] {
                oa[k] = b;
                ob[k] = rb[i];
                k = k + 1;
            }
        }
        i = i + 1;
    }
    return k;
}

// hold_back_oversized + take_row_gathered, in that order. `avail` = 0 means the
// platform could not measure MemAvailable: nothing is held back, as natively.
// A wanted table is taken only if the takeover succeeded (`takeover_ok`), and a
// failed takeover leaves the tables with the mode the run asked for.
// Returns the number of tensors that left the resident set (held + taken).
export fn dw_classify(size: const i64[:], rowwant: const i64[:], n: i64, avail: i64,
                      takeover_ok: i64, code: i64[:]) -> i64 {
    var left: i64 = 0;
    var i: i64 = 0;

    while i < n {
        code[i] = 0;
        if avail != 0 && size[i] > avail {
            code[i] = 1;
            left = left + 1;
        }
        i = i + 1;
    }
    if takeover_ok != 0 {
        i = 0;
        while i < n {
            if rowwant[i] != 0 {
                if code[i] == 0 {
                    left = left + 1;
                }
                code[i] = 2;
            }
            i = i + 1;
        }
    }
    return left;
}

// The init verdict after the two passes above. 0 = fail (pinned memory is
// unavailable on this platform), 1 = the anonymous read+rebind, 2 = the warm
// sweep, 3 = mmap with random-access advice (nothing to rebind), 4 = mmap as
// asked. The empty-set case is checked BEFORE the pinned capability, exactly as
// the native init orders them, so a Pinned run with nothing left to rebind is a
// success, not a failure.
export fn dw_verdict(mode: i64, n_resident: i64, pinned_ok: i64) -> i64 {
    if mode == DW_ANON || mode == DW_PINNED {
        if n_resident == 0 {
            return 3;
        }
        if mode == DW_PINNED && pinned_ok == 0 {
            return 0;
        }
        return 1;
    }
    if mode == DW_WARMED {
        return 2;
    }
    return 4;
}

// The address-space span a row-served table reserves: its bytes rounded up to
// the page, because the reservation must mirror the tensor's own layout.
export fn dw_span(size: i64, page: i64) -> i64 {
    if page <= 0 {
        return size;
    }
    return (size + page - 1) / page * page;
}

// Bytes a dense tensor contributes to the warm sweep and the sensor: the ones
// still resident after the two passes. This is the number the module reports as
// "left out of the resident set", so it is part of the decision, not telemetry.
export fn dw_resident_bytes(size: const i64[:], code: const i64[:], n: i64) -> i64 {
    var b: i64 = 0;
    var i: i64 = 0;

    while i < n {
        if code[i] == 0 {
            b = b + size[i];
        }
        i = i + 1;
    }
    return b;
}
