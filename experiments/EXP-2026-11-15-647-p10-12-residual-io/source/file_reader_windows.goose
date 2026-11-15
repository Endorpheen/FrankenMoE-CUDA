// P10-12 S1: the alignment decisions of the carrier's positioned file reader.
//
// Native source: runtime/core/src/io/file_reader.cpp at base commit aa9109b5.
// The file is the last unported piece of the carrier's I/O substrate: the
// expert streamer and the dense-weight loader both read through it. Its
// syscalls (open/pread/fstat/posix_memalign) are kernel work, but the
// arithmetic that surrounds them is a decision set that never needs a model:
//
//   default alignment, lane clamp                        open(), 33-35
//   the O_DIRECT verify block offset                     open(), 60-63
//   plain-read EOF clip and returned length              read(), 100-113
//   the aligned window and its three boundaries          read(), 115-119
//   the read_end / bulk_end split (the EOF tail)         read(), 134-135
//   the returned window (what bandwidth is judged on)    read(), 152
//   the bounce growth test                               read(), 120-132
//
// The prefix is fr_. Everything here is integer arithmetic over i64; the
// native uses uint64_t, and every case in the gate stays below 2^63, the
// domain the reader is used in (file sizes and offsets of real models).
// `end - off` is computed in i64 exactly as the native computes it in u64 and
// casts to `long long`: two's complement subtraction produces the same bit
// pattern, so even the "read from past EOF" negative result agrees.
//
// Not owned (kernel and hardware, kept native): the descriptor itself, pread
// loops, the O_DIRECT probe read, the per-lane allocation. This module decides
// WHERE to read, the carrier still performs the read.

const FR_ALIGN: i64 = 4096;

// ~(align - 1) for a power-of-two alignment, the only kind the reader accepts.
fn fr_mask(align: i64) -> i64 {
    return 0 - align;
}

// align_ = align ? align : 4096
export fn fr_align(align: i64) -> i64 {
    if align == 0 {
        return FR_ALIGN;
    }
    return align;
}

// const int n = lanes < 1 ? 1 : lanes;
export fn fr_lanes(lanes: i64) -> i64 {
    if lanes < 1 {
        return 1;
    }
    return lanes;
}

// The offset the open-time O_DIRECT verify compares: the middle of the file,
// rounded down to the alignment, or 0 when that window would run past the end.
// -1 means "no verify": the reader is not in an alignment-constrained direct
// mode, or the file is shorter than one block.
export fn fr_verify_off(fsize: i64, align0: i64, aligned: i64) -> i64 {
    let align: i64 = fr_align(align0);
    if aligned == 0 {
        return 0 - 1;
    }
    if fsize < align {
        return 0 - 1;
    }
    var voff: i64 = (fsize / 2) & fr_mask(align);
    if voff + align > fsize {
        voff = 0;
    }
    return voff;
}

// The plain (non-alignment-constrained) read: the EOF clip and the returned
// length. The native returns `(long long)(end - off)`, i.e. the bytes it moved.
export fn fr_plain_len(off: i64, nbytes: i64, fsize: i64) -> i64 {
    var end: i64 = off + nbytes;
    if fsize != 0 && off + nbytes > fsize {
        end = fsize;
    }
    return end - off;
}

// The aligned window, written into out:
//   0 a0        start rounded down to the alignment
//   1 a1        end rounded up (what O_DIRECT is asked for)
//   2 len       the bounce-buffer size this read needs
//   3 read_end  a1 clipped to the file size (the EOF case)
//   4 bulk_end  read_end rounded down: the part the direct fd may read
//   5 window    read_end - a0, the value read() returns
//   6 tail      read_end - bulk_end, the sub-alignment EOF tail read buffered
// Returns the window, which is also read()'s return value in this mode.
fn fr_win(off: i64, nbytes: i64, align0: i64, fsize: i64, out: i64[:]) -> i64 {
    let align: i64 = fr_align(align0);
    let m: i64 = fr_mask(align);
    let a0: i64 = off & m;
    let a1: i64 = (off + nbytes + align - 1) & m;
    var read_end: i64 = a1;
    if fsize != 0 && a1 > fsize {
        read_end = fsize;
    }
    let bulk_end: i64 = read_end & m;
    if out.len >= 7 {
        out[0] = a0;
        out[1] = a1;
        out[2] = a1 - a0;
        out[3] = read_end;
        out[4] = bulk_end;
        out[5] = read_end - a0;
        out[6] = read_end - bulk_end;
    }
    return read_end - a0;
}

// The exported form of the same window, for a caller that wants the fields.
export fn fr_aligned_win(off: i64, nbytes: i64, align0: i64, fsize: i64, out: i64[:]) -> i64 {
    return fr_win(off, nbytes, align0, fsize, out);
}

// read()'s return value for either mode. `aligned` is the reader's effective
// aligned_reads_ flag: direct AND the platform's direct mode demands
// alignment (pio::direct_needs_alignment()).
export fn fr_read_len(off: i64, nbytes: i64, align0: i64, fsize: i64, aligned: i64) -> i64 {
    if nbytes == 0 {
        return 0;
    }
    if aligned == 0 {
        return fr_plain_len(off, nbytes, fsize);
    }
    var win: i64[7] = [0, 0, 0, 0, 0, 0, 0];
    return fr_win(off, nbytes, align0, fsize, win);
}

// The bounce test: 1 when the lane's buffer must be reallocated to `len`.
export fn fr_bounce_grow(len: i64, cap: i64) -> i64 {
    if cap < len {
        return 1;
    }
    return 0;
}
