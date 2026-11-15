// P10-12 S2: the carrier's pseudo-file readers.
//
// Native source: runtime/core/src/io/platform_io.cpp at base commit aa9109b5.
// The file wraps kernel primitives, but three of its parts are text and
// arithmetic decisions, not syscalls, and no accepted slice covers them:
//
//   scan_kb_file: "Key: <n> kB" lines, first non-zero hit wins    274-296
//   process_memory / device_memory key tables and field mapping   298-315
//   file_mapped_regions: the /proc/self/maps line grammar,
//     the pathname trim and the basename suffix match             317-357
//   vm_resident_sample: which pages count (clipping) and the
//     512-page syscall chunking                                  231-256
//   fault_bytes: one major fault is one page                     267-272
//   direct_needs_alignment / pinned_max_bytes platform policy    360-365, 379-383
//
// The prefix is pr_. The syscalls themselves (fopen, mincore, mmap, sched_yield)
// stay native: this module decides what the text means and which pages are in
// range, the carrier still performs the read.
//
// Decisions reproduced exactly, because the native code has them:
//   - a line longer than 255 bytes arrives in fgets chunks, and a chunk that
//     starts mid-line cannot match a key (the 256-byte line buffer);
//   - the key must be followed immediately by ':';
//   - sscanf(" %llu kB") succeeds on the number alone: the trailing "kB" is
//     never required, so "MemFree: 12abytes" sets 12 (sscanf returns 1 as soon
//     as the conversion is assigned), and "MemFree: 0x10 kB" sets 0;
//   - a "-" sign is accepted and negates in u64 (two's complement);
//   - out[i] doubles as the found flag, so a 0 kB line does not lock the key
//     and a later occurrence may overwrite it, while a non-zero one does;
//   - the maps pathname is matched by suffix after trimming trailing '\n' and
//     ' ', and a name shorter than the basename never matches;
//   - residency counts only pages fully inside the range, and an empty range
//     is a measured zero, not a failure.

// ── character classes ──────────────────────────────────────────────────────
fn pr_is_space(c: u8) -> bool {
    return c == 32 || c == 9 || c == 10 || c == 11 || c == 12 || c == 13;
}

fn pr_is_digit(c: u8) -> bool {
    return c >= 48 && c <= 57;
}

fn pr_hex_val(c: u8) -> i64 {
    if c >= 48 && c <= 57 {
        return c - 48;
    }
    if c >= 97 && c <= 102 {
        return c - 87;
    }
    if c >= 65 && c <= 70 {
        return c - 55;
    }
    return 0 - 1;
}

// ── scanf conversions over a slice ─────────────────────────────────────────
// " %llu": skip whitespace, optional sign, at least one digit. out[0] is the
// position after the number, out[1] the value. 1 on success, 0 when there is no
// digit to convert (sscanf does not assign and returns 0).
fn pr_scan_uint(text: const u8[:], at0: i64, out: i64[:]) -> i64 {
    var at: i64 = at0;
    while at < text.len && pr_is_space(text[at]) {
        at = at + 1;
    }
    var neg: i64 = 0;
    if at < text.len && (text[at] == 43 || text[at] == 45) {
        if text[at] == 45 {
            neg = 1;
        }
        at = at + 1;
    }
    if at >= text.len || !pr_is_digit(text[at]) {
        return 0;
    }
    let start: i64 = at;
    var v: i64 = 0;
    while at < text.len && pr_is_digit(text[at]) {
        v = v * 10 + (text[at] - 48);
        at = at + 1;
    }
    // glibc clamps an out-of-range %llu to UINT64_MAX; more than 20 digits is
    // always out of range, and 20 digits only when above 18446744073709551615.
    let nd: i64 = at - start;
    var over: i64 = 0;
    if nd > 20 {
        over = 1;
    }
    if nd == 20 {
        let max: const u8[:] = "18446744073709551615";
        var i: i64 = 0;
        while i < 20 && over == 0 {
            if text[start + i] > max[i] {
                over = 1;
            }
            if text[start + i] < max[i] {
                i = 19;
            }
            i = i + 1;
        }
    }
    if over != 0 {
        v = 0 - 1;
    }
    if neg != 0 {
        v = 0 - v;
    }
    out[0] = at;
    out[1] = v;
    return 1;
}

// "%llx": skip whitespace, optional sign, optional 0x, at least one hex digit.
fn pr_scan_hex(text: const u8[:], at0: i64, out: i64[:]) -> i64 {
    var at: i64 = at0;
    while at < text.len && pr_is_space(text[at]) {
        at = at + 1;
    }
    var neg: i64 = 0;
    if at < text.len && (text[at] == 43 || text[at] == 45) {
        if text[at] == 45 {
            neg = 1;
        }
        at = at + 1;
    }
    if at + 1 < text.len && text[at] == 48 && (text[at + 1] == 120 || text[at + 1] == 88) {
        at = at + 2;
    }
    if at >= text.len || pr_hex_val(text[at]) < 0 {
        return 0;
    }
    var v: i64 = 0;
    while at < text.len && pr_hex_val(text[at]) >= 0 {
        v = v * 16 + pr_hex_val(text[at]);
        at = at + 1;
    }
    if neg != 0 {
        v = 0 - v;
    }
    out[0] = at;
    out[1] = v;
    return 1;
}

// "%*s": skip whitespace, then consume at least one non-whitespace byte.
// Returns the position after the token, or 0 - 1 when there is no token.
fn pr_scan_word(text: const u8[:], at0: i64) -> i64 {
    var at: i64 = at0;
    while at < text.len && pr_is_space(text[at]) {
        at = at + 1;
    }
    if at >= text.len || pr_is_space(text[at]) {
        return 0 - 1;
    }
    while at < text.len && !pr_is_space(text[at]) {
        at = at + 1;
    }
    return at;
}

// ── the "Key: <n> kB" grammar ──────────────────────────────────────────────
// One fgets line: the native buffer holds at most 255 bytes and stops after a
// newline, so a longer line is delivered (and parsed) in chunks.
fn pr_line_end(text: const u8[:], pos: i64) -> i64 {
    var i: i64 = pos;
    while i < text.len && (i - pos) < 255 {
        if text[i] == 10 {
            return i + 1;
        }
        i = i + 1;
    }
    return i;
}

// One line against one key: the value in bytes, or -1 when the line is not a
// "key: number" line for that key.
fn pr_line_kb(line: const u8[:], key: const u8[:]) -> i64 {
    if line.len <= key.len {
        return 0 - 1;
    }
    if line[0..key.len] != key {
        return 0 - 1;
    }
    if line[key.len] != 58 {
        return 0 - 1;
    }
    var o: i64[2] = [0, 0];
    let body: const u8[:] = line[key.len + 1..line.len];
    if pr_scan_uint(body, 0, o) == 0 {
        return 0 - 1;
    }
    return o[1] * 1024;
}

// The whole file for one key: value in bytes, -1 when no line matched.
// out[i] is the native found flag, so a 0 kB hit does not lock the key.
fn pr_scan_hit(text: const u8[:], key: const u8[:]) -> i64 {
    var v: i64 = 0;
    var hit: i64 = 0;
    var pos: i64 = 0;
    while pos < text.len {
        let le: i64 = pr_line_end(text, pos);
        if v == 0 {
            let r: i64 = pr_line_kb(text[pos..le], key);
            if r != 0 - 1 {
                v = r;
                hit = 1;
            }
        }
        pos = le;
    }
    if hit == 0 {
        return 0 - 1;
    }
    return v;
}

// process_memory: VmRSS, RssAnon, RssFile, VmSwap. Returns 1 when the file was
// parsed (any key seen), 0 otherwise; values are 0 for absent keys.
export fn pr_proc_memory(text: const u8[:], out: i64[:]) -> i64 {
    let a: i64 = pr_scan_hit(text, "VmRSS");
    let b: i64 = pr_scan_hit(text, "RssAnon");
    let c: i64 = pr_scan_hit(text, "RssFile");
    let d: i64 = pr_scan_hit(text, "VmSwap");
    var hit: i64 = 0;
    if a >= 0 {
        out[0] = a;
        hit = 1;
    } else {
        out[0] = 0;
    }
    if b >= 0 {
        out[1] = b;
        hit = 1;
    } else {
        out[1] = 0;
    }
    if c >= 0 {
        out[2] = c;
        hit = 1;
    } else {
        out[2] = 0;
    }
    if d >= 0 {
        out[3] = d;
        hit = 1;
    } else {
        out[3] = 0;
    }
    return hit;
}

// device_memory: MemAvailable, MemFree, SwapFree.
export fn pr_proc_device(text: const u8[:], out: i64[:]) -> i64 {
    let a: i64 = pr_scan_hit(text, "MemAvailable");
    let b: i64 = pr_scan_hit(text, "MemFree");
    let c: i64 = pr_scan_hit(text, "SwapFree");
    var hit: i64 = 0;
    if a >= 0 {
        out[0] = a;
        hit = 1;
    } else {
        out[0] = 0;
    }
    if b >= 0 {
        out[1] = b;
        hit = 1;
    } else {
        out[1] = 0;
    }
    if c >= 0 {
        out[2] = c;
        hit = 1;
    } else {
        out[2] = 0;
    }
    return hit;
}

// The faithful multi-key form, for the differential gate: keys is a blob,
// koff/klen index it, out is zeroed by the caller. Returns the native `found`.
export fn pr_kb_scan(text: const u8[:], keys: const u8[:], koff: i64[:], klen: i64[:], out: i64[:]) -> i64 {
    var pos: i64 = 0;
    var found: i64 = 0;
    while found < out.len && pos < text.len {
        let le: i64 = pr_line_end(text, pos);
        var i: i64 = 0;
        var done: i64 = 0;
        while i < out.len && done == 0 {
            if out[i] == 0 {
                let ko: i64 = koff[i];
                let kl: i64 = klen[i];
                let r: i64 = pr_line_kb(text[pos..le], keys[ko..ko + kl]);
                if r != 0 - 1 {
                    out[i] = r;
                    found = found + 1;
                    done = 1;
                }
            }
            i = i + 1;
        }
        pos = le;
    }
    return found;
}

// ── /proc/self/maps line grammar ───────────────────────────────────────────
// sscanf("%llx-%llx %*s %llx %*s %*s %n") over one fgets line, then the
// pathname trim and the suffix match. out[0..2] = start, end, file offset.
// 1 when the line maps a path ending with `basename`, 0 otherwise.
export fn pr_maps_line(line: const u8[:], basename: const u8[:], out: i64[:]) -> i64 {
    var p: i64[2] = [0, 0];
    if pr_scan_hex(line, 0, p) == 0 {
        return 0;
    }
    if p[0] >= line.len || line[p[0]] != 45 {
        return 0;
    }
    var q: i64[2] = [0, 0];
    if pr_scan_hex(line, p[0] + 1, q) == 0 {
        return 0;
    }
    // perms, then the file offset
    let t1: i64 = pr_scan_word(line, q[0]);
    if t1 < 0 {
        return 0;
    }
    var o: i64[2] = [0, 0];
    if pr_scan_hex(line, t1, o) == 0 {
        return 0;
    }
    // dev, then inode
    let t2: i64 = pr_scan_word(line, o[0]);
    if t2 < 0 {
        return 0;
    }
    let t3: i64 = pr_scan_word(line, t2);
    if t3 < 0 {
        return 0;
    }
    // the format's trailing " " directive: skip the whitespace run, then %n
    var at: i64 = t3;
    while at < line.len && pr_is_space(line[at]) {
        at = at + 1;
    }
    var plen: i64 = line.len - at;
    while plen > 0 && (line[at + plen - 1] == 10 || line[at + plen - 1] == 32) {
        plen = plen - 1;
    }
    if plen < basename.len {
        return 0;
    }
    if basename.len > 0 {
        if line[at + plen - basename.len..at + plen] != basename {
            return 0;
        }
    }
    out[0] = p[1];
    out[1] = q[1];
    out[2] = o[1];
    return 1;
}

// ── residency and platform policy ──────────────────────────────────────────
// Pages fully inside [p, p+sz): the window starts at the first page boundary at
// or after p and ends at the first boundary at or before p+sz. off_in_page is
// p mod page. Returns 0 for an empty range (the native returns true early).
export fn pr_resident_pages(off_in_page: i64, sz: i64, page: i64) -> i64 {
    let mask: i64 = 0 - page;
    let d0: i64 = (0 - off_in_page) & (page - 1);
    let d1: i64 = (off_in_page + sz) & mask;
    if d1 <= d0 {
        return 0;
    }
    return (d1 - d0) / page;
}

// mincore() serves at most 512 pages per call (one byte per page).
export fn pr_resident_calls(pages: i64) -> i64 {
    if pages <= 0 {
        return 0;
    }
    return (pages + 511) / 512;
}

// One major fault brings back one page.
export fn pr_fault_bytes(page: i64) -> i64 {
    return page;
}

// F_NOCACHE (Apple) is a caching hint, not an alignment-constrained I/O mode.
export fn pr_direct_needs_alignment(apple: i64) -> i64 {
    if apple != 0 {
        return 0;
    }
    return 1;
}

// AHardwareBuffer_lock fails at exactly 2^31; nothing else has such an
// allocation at all.
export fn pr_pinned_max(android: i64) -> i64 {
    if android != 0 {
        return 2147483647;
    }
    return 0;
}
