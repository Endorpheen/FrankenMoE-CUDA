extern fn fg_win_network_ready() -> bool;
extern fn fg_win_socket_timeout(fd: i64) -> i64;

fn cli_network_ready() -> bool { return fg_win_network_ready(); }

export fn fg_http_set_timeout(fd: i64) -> i64 {
    return fg_win_socket_timeout(fd);
}
