extern fn host_runtime_recv(fd: u64, data: u64, size: i32, flags: i32) -> i32;
extern fn host_runtime_send(fd: u64, data: u64, size: i32, flags: i32) -> i32;
extern fn host_runtime_close(fd: u64) -> i32;

export fn qz_rd(fd: i64, buffer: u64, bytes: i64) -> i64 {
    if bytes < 0 || bytes > 2147483647 { return -1; }
    return host_runtime_recv(fd as! u64, buffer, bytes as! i32, 0) as! i64;
}

export fn qz_wr(fd: i64, buffer: u64, bytes: i64) -> i64 {
    if bytes < 0 || bytes > 2147483647 { return -1; }
    return host_runtime_send(fd as! u64, buffer, bytes as! i32, 0) as! i64;
}

export fn qz_cl(fd: i64) -> i64 { return host_runtime_close(fd as! u64) as! i64; }
