extern fn setsockopt(fd: i32, level: i32, option: i32, value: c_ptr<const void>, length: u32) -> i32;

fn cli_network_ready() -> bool { return true; }

export fn fg_http_set_timeout(fd: i64) -> i64 {
    let timeout: i64[2] = [30, 0];
    let value = unsafe_c_address<c_ptr<const void>>(timeout[0]);
    if setsockopt(fd as! i32, 1, 20, value, 16) != 0 { return -1; }
    return setsockopt(fd as! i32, 1, 21, value, 16) as! i64;
}
