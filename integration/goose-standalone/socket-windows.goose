extern fn closesocket(fd: u64) -> i32;
extern fn recv(fd: u64, buffer: c_ptr<void>, size: i32, flags: i32) -> i32;
extern fn send(fd: u64, buffer: c_ptr<const void>, size: i32, flags: i32) -> i32;
extern fn accept(fd: u64, address: c_ptr<void>, length: c_ptr<i32>) -> u64;
extern fn socket(domain: i32, kind: i32, protocol: i32) -> u64;
extern fn bind(fd: u64, address: c_ptr<const void>, length: i32) -> i32;
extern fn listen(fd: u64, backlog: i32) -> i32;
extern fn setsockopt(fd: u64, level: i32, name: i32, value: c_ptr<const void>, length: i32) -> i32;
extern fn getsockname(fd: u64, address: c_ptr<void>, length: c_ptr<i32>) -> i32;
extern "memcpy" fn socket_copy(dst: c_ptr<void>, src: c_ptr<const void>, size: u64) -> c_ptr<void>;

const SOCKET_MAX_LENGTH: i64 = 2147483647;
const SOCKET_INVALID: u64 = 0xffffffffffffffff;

export fn gx_close(fd: i64) -> i64 { return closesocket(fd as! u64) as! i64; }

export fn gx_read(fd: i64, buffer: u8[:], length: i64) -> i64 {
    let address = unsafe_c_slice_data<c_ptr<void>>(buffer);
    if length < 0 || length > buffer.len || length > SOCKET_MAX_LENGTH || (length != 0 && address == null) { return -1; }
    return recv(fd as! u64, address, length as! i32, 0) as! i64;
}

export fn gx_write(fd: i64, buffer: const u8[:], length: i64) -> i64 {
    let address = unsafe_c_slice_data<c_ptr<const void>>(buffer);
    if length < 0 || length > buffer.len || length > SOCKET_MAX_LENGTH || (length != 0 && address == null) { return -1; }
    return send(fd as! u64, address, length as! i32, 0) as! i64;
}

export fn gx_accept(fd: i64, buffer: u8[:], length: i64[:]) -> i64 {
    let address = unsafe_c_slice_data<c_ptr<void>>(buffer);
    if address == null { return accept(fd as! u64, null, null) as! i64; }
    if length.len < 1 || unsafe_c_slice_data<c_ptr<void>>(length) == null { return -1; }
    if length[0] < 0 || length[0] > buffer.len || length[0] > SOCKET_MAX_LENGTH { return -1; }
    var n = length[0] as! i32;
    let client = accept(fd as! u64, address, unsafe_c_address<c_ptr<i32>>(n));
    if client != SOCKET_INVALID { length[0] = n as! i64; }
    return client as! i64;
}

export fn gx_socket(domain: i64, kind: i64, protocol: i64) -> i64 {
    return socket(domain as! i32, kind as! i32, protocol as! i32) as! i64;
}

export fn hl_bind(fd: i64, address: u64, length: i64) -> i64 {
    if address == 0 || length < 0 || length > SOCKET_MAX_LENGTH { return -1; }
    return bind(fd as! u64, unsafe_c_ptr<c_ptr<const void>>(address), length as! i32) as! i64;
}

export fn gx_bind(fd: i64, buffer: const u8[:], length: i64) -> i64 {
    let address = unsafe_c_slice_data<c_ptr<const void>>(buffer);
    if address == null || length < 0 || length > buffer.len || length > SOCKET_MAX_LENGTH { return -1; }
    return bind(fd as! u64, address, length as! i32) as! i64;
}

export fn gx_listen(fd: i64, backlog: i64) -> i64 { return listen(fd as! u64, backlog as! i32) as! i64; }

export fn gx_setsockopt(fd: i64, level: i64, name: i64, value: const u8[:], length: i64) -> i64 {
    if level != 1 || name != 2 || length != 4 || value.len < 4 ||
        unsafe_c_slice_data<c_ptr<const void>>(value) == null { return -1; }
    const SOL_SOCKET: i32 = 65535;
    const SO_EXCLUSIVEADDRUSE: i32 = -5;
    let exclusive: i32 = 1;
    return setsockopt(fd as! u64, SOL_SOCKET, SO_EXCLUSIVEADDRUSE,
        unsafe_c_address<c_ptr<const void>>(exclusive), 4) as! i64;
}

export fn hl_socket(domain: i64, kind: i64, protocol: i64) -> i64 { return gx_socket(domain, kind, protocol); }
export fn hl_listen(fd: i64, backlog: i64) -> i64 { return gx_listen(fd, backlog); }

export fn hl_accept(fd: i64, address: u64, size: u64) -> i64 {
    if size > (SOCKET_MAX_LENGTH as! u64) { return -1; }
    if address == 0 { return accept(fd as! u64, null, null) as! i64; }
    var n = size as! i32;
    return accept(fd as! u64, unsafe_c_ptr<c_ptr<void>>(address), unsafe_c_address<c_ptr<i32>>(n)) as! i64;
}

export fn hl_getsockname(fd: i64, address: u64, length: u64) -> i64 {
    if address == 0 || length == 0 { return -1; }
    var size: i64 = 0;
    socket_copy(unsafe_c_address<c_ptr<void>>(size), unsafe_c_ptr<c_ptr<const void>>(length), 8);
    if size < 0 || size > SOCKET_MAX_LENGTH { return -1; }
    var native_size = size as! i32;
    let result = getsockname(fd as! u64, unsafe_c_ptr<c_ptr<void>>(address), unsafe_c_address<c_ptr<i32>>(native_size));
    if result == 0 {
        size = native_size as! i64;
        socket_copy(unsafe_c_ptr<c_ptr<void>>(length), unsafe_c_address<c_ptr<const void>>(size), 8);
    }
    return result as! i64;
}
