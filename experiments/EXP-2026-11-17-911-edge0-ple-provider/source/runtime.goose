// Requires the accepted e0_ple_desc and e0_ple_decode_row exports.
// File descriptor and both buffers belong to the caller for the whole call.
// Only the syscall ABI is native. No shared seek position, retained pointer,
// allocator, cache, thread or file lifetime is hidden in this provider.
extern "e0_pread_bytes" fn positioned_read(fd: i64, dst: u8[:], offset: i64) -> i64;

// Returns completed rows, or -1 arguments, -2 row, -3 I/O, -4 EOF,
// -5 retry budget, -6 unexpected decoder failure. Outputs commit after all I/O.
export fn e0_ple_read_rows(fd: i64, rows: i64[:], scale: f32, scratch: u8[:], out: f32[:], desc: i64[:]) -> i64 {
    if fd < 0 || fd > 2147483647 || rows.len < 0 || rows.len > 16 || desc.len < 8 || scale != e0_ple_supported_scale() {
        return -1;
    }
    let n: i64 = rows.len * 160;
    if scratch.len < n || out.len < n {
        return -1;
    }
    var i: i64 = 0;
    while i < rows.len {
        if e0_ple_desc(rows[i], desc) != 0 {
            return -2;
        }
        i = i + 1;
    }
    i = 0;
    while i < rows.len {
        if e0_ple_desc(rows[i], desc) != 0 {
            return -2;
        }
        var done: i64 = 0;
        var attempts: i64 = 0;
        while done < 160 && attempts < 192 {
            let got: i64 = positioned_read(fd, scratch[i * 160 + done .. i * 160 + 160], desc[1] + done);
            attempts = attempts + 1;
            if got != -4 {
                if got < 0 || got > 160 - done {
                    return -3;
                }
                if got == 0 {
                    return -4;
                }
                done = done + got;
            }
        }
        if done != 160 {
            return -5;
        }
        i = i + 1;
    }
    i = 0;
    while i < rows.len {
        if e0_ple_decode_row(scratch[i * 160 .. i * 160 + 160], scale, out[i * 160 .. i * 160 + 160]) != 0 {
            return -6;
        }
        i = i + 1;
    }
    return rows.len;
}
