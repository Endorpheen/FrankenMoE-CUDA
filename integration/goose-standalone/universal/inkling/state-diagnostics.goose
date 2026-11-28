extern fn fg_cuda_host_view_u8(address: u64, length: i64) -> u8[:];

fn i2_check_state(name: const u8[:], position: i32, address: u64, count: i64) {
    if i2_prefill_env_pointer("I2_CHECK_STATE\0") == 0 { return; }
    let data = fg_cuda_host_view_u8(address, count * 4);
    var hash: u64 = 14695981039346656037;
    for index in data.len { hash = (hash ^ (data[index] as! u64)) * 1099511628211; }
    let digits: const u8[:] = "0123456789abcdef";
    var rendered: u8[16];
    for index in 16 {
        let shift = ((15 - index) * 4) as! u64;
        rendered[index] = digits[((hash >> shift) & 15) as! i64];
    }
    i2d_log(str("I2 state ", name, " token ", position, " ", rendered[0 .. 16], "\n"));
}

fn i2_check_finite(address: u64, count: i64) {
    let values = fg_cuda_host_view_f32(address, count);
    for index in count {
        let value = values[index];
        assert(value <= 3.402823466e38 && value >= -3.402823466e38);
    }
}

export fn i2_serial_trace(visit: i32, current: I2DraftSpan) {
    i2_check_state("draft-serial-visit", visit, current.address, current.len);
}
