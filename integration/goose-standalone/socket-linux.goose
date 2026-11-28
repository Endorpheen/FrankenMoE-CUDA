extern fn close(fd: i32) -> i32;
extern fn read(fd: i32, data: c_ptr<void>, size: u64) -> i64;
extern fn send(fd: i32, data: c_ptr<const void>, size: u64, flags: i32) -> i64;
extern fn accept(fd: i32, address: c_ptr<void>, length: c_ptr<u32>) -> i32;
extern fn socket(domain: i32, kind: i32, protocol: i32) -> i32;
extern fn bind(fd: i32, address: c_ptr<const void>, length: u32) -> i32;
extern fn listen(fd: i32, backlog: i32) -> i32;
extern fn setsockopt(fd: i32, level: i32, name: i32, value: c_ptr<const void>, size: u32) -> i32;
extern fn getsockname(fd: i32, address: c_ptr<void>, length: c_ptr<u32>) -> i32;

export fn gx_close(fd: i64) -> i64 { return close(fd as! i32) as! i64; }

export fn gx_read(fd: i64, buffer: u8[:], size: i64) -> i64 {
    return read(fd as! i32, unsafe_c_slice_data<c_ptr<void>>(buffer), size as! u64);
}

export fn gx_write(fd: i64, buffer: const u8[:], size: i64) -> i64 {
    const MSG_NOSIGNAL: i32 = 16384;
    if size < 0 || size > buffer.len { return -1; }
    return send(fd as! i32, unsafe_c_slice_data<c_ptr<const void>>(buffer), size as! u64, MSG_NOSIGNAL);
}

export fn gx_accept(fd: i64, address: u8[:], length: i64[:]) -> i64 {
    var native_length: u32 = 16;
    if length.len > 0 { native_length = length[0] as! u32; }
    let client = accept(fd as! i32, unsafe_c_slice_data<c_ptr<void>>(address),
        unsafe_c_address<c_ptr<u32>>(native_length));
    if length.len > 0 { length[0] = native_length as! i64; }
    return client as! i64;
}

export fn gx_socket(domain: i64, kind: i64, protocol: i64) -> i64 {
    return socket(domain as! i32, kind as! i32, protocol as! i32) as! i64;
}

export fn gx_bind(fd: i64, address: const u8[:], size: i64) -> i64 {
    return bind(fd as! i32, unsafe_c_slice_data<c_ptr<const void>>(address), size as! u32) as! i64;
}

export fn gx_listen(fd: i64, backlog: i64) -> i64 {
    return listen(fd as! i32, backlog as! i32) as! i64;
}

export fn gx_setsockopt(fd: i64, level: i64, name: i64, value: const u8[:], size: i64) -> i64 {
    return setsockopt(fd as! i32, level as! i32, name as! i32,
        unsafe_c_slice_data<c_ptr<const void>>(value), size as! u32) as! i64;
}

export fn hl_socket(domain: i64, kind: i64, protocol: i64) -> i64 { return gx_socket(domain, kind, protocol); }
export fn hl_listen(fd: i64, backlog: i64) -> i64 { return gx_listen(fd, backlog); }

export fn hl_bind(fd: i64, address: u64, size: i64) -> i64 {
    return bind(fd as! i32, unsafe_c_ptr<c_ptr<const void>>(address), size as! u32) as! i64;
}

export fn hl_accept(fd: i64, address: u64, size: u64) -> i64 {
    var length = size as! u32;
    return accept(fd as! i32, unsafe_c_ptr<c_ptr<void>>(address), unsafe_c_address<c_ptr<u32>>(length)) as! i64;
}

export fn hl_getsockname(fd: i64, address: u64, length: u64) -> i64 {
    return getsockname(fd as! i32, unsafe_c_ptr<c_ptr<void>>(address), unsafe_c_ptr<c_ptr<u32>>(length)) as! i64;
}
