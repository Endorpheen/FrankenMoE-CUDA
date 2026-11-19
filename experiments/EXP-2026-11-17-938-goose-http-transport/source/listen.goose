extern "socket" fn gx_socket(domain: i64, kind: i64, proto: i64) -> i64;
extern "bind" fn gx_bind(fd: i64, addr: u8[:], len: i64) -> i64;
extern "listen" fn gx_listen(fd: i64, backlog: i64) -> i64;
extern "gx_close" fn gx_close(fd: i64) -> i64;
extern "gx_read" fn gx_read(fd: i64, buf: u8[:], n: i64) -> i64;
extern "gx_write" fn gx_write(fd: i64, buf: const u8[:], n: i64) -> i64;

export fn probe_listen(port: i64) -> i64 {
    let fd: i64 = gx_socket(2, 1, 0);
    if fd < 0 { return -1; }
    var a: u8[16] = [2,0,((port / 256) % 256) as! u8,(port % 256) as! u8,127,0,0,1,0,0,0,0,0,0,0,0];
    if gx_bind(fd, a[0 .. 16], 16) != 0 { gx_close(fd); return -2; }
    if gx_listen(fd, 8) != 0 { gx_close(fd); return -3; }
    gx_close(fd);
    return 0;
}
