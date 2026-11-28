extern fn setsockopt(fd: i32, level: i32, option: i32, value: c_ptr<const void>, length: u32) -> i32;

fn vk_http_network_ready() -> bool { return true; }
fn vk_http_network_close() {}

export fn fg_vk_http_timeout(fd: i64) -> i64 {
    let timeout: i64[2] = [30, 0];
    let address = unsafe_c_address<c_ptr<const void>>(timeout[0]);
    if setsockopt(fd as! i32, 1, 20, address, 16) != 0 { return -1; }
    return setsockopt(fd as! i32, 1, 21, address, 16) as! i64;
}
