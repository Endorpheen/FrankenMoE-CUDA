extern fn host_runtime_read(fd: i32, data: u64, bytes: u64) -> i64;
extern fn host_runtime_write(fd: i32, data: u64, bytes: u64) -> i64;
extern fn host_runtime_close(fd: i32) -> i32;

export fn qz_rd(fd: i64, buffer: u64, bytes: i64) -> i64 {
    return host_runtime_read(fd as! i32, buffer, bytes as! u64);
}

export fn qz_wr(fd: i64, buffer: u64, bytes: i64) -> i64 {
    return host_runtime_write(fd as! i32, buffer, bytes as! u64);
}

export fn qz_cl(fd: i64) -> i64 { return host_runtime_close(fd as! i32) as! i64; }
