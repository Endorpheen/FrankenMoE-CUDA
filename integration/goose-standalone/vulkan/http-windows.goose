extern fn fg_win_network_ready() -> bool;
extern fn fg_win_socket_timeout(fd: i64) -> i32;
extern fn WSACleanup() -> i32;

fn vk_http_network_ready() -> bool { return fg_win_network_ready(); }
fn vk_http_network_close() { WSACleanup(); }

export fn fg_vk_http_timeout(fd: i64) -> i64 { return fg_win_socket_timeout(fd) as! i64; }
