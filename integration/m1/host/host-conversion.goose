extern "malloc" fn host_conv_malloc(size: u64) -> c_ptr<void>;
extern "free" fn host_conv_free(pointer: c_ptr<void>);
extern "frexp" fn host_conv_frexp(value: f64, exponent: c_ptr<i32>) -> f64;
extern fn host_conv_strtod(text: u64, end: u64) -> f64;
extern fn host_conv_time(timer: u64) -> i64;
extern fn host_conv_localtime(timer: u64) -> u64;
extern fn host_conv_strftime(out: u64, capacity: u64, format: u64, calendar: u64) -> u64;

fn host_conv_address(pointer: c_ptr<const void>) -> u64 {
    return builder_address(pointer) as! u64;
}

fn host_conv_cstring(text: const u8[:]) -> u64 {
    if text.len < 0 { return 0; }
    let bytes = text.len as! u64;
    let pointer = host_conv_malloc(bytes + 1);
    if pointer == null { return 0; }
    var address: u64 = 0;
    host_quant_copy(unsafe_c_address<c_ptr<void>>(address), unsafe_c_address<c_ptr<const void>>(pointer), 8);
    if bytes != 0 {
        host_quant_copy(pointer, unsafe_c_slice_data<c_ptr<const void>>(text), bytes);
    }
    let nul: u8 = 0;
    host_quant_copy(unsafe_c_ptr<c_ptr<void>>(address + bytes), unsafe_c_address<c_ptr<const void>>(nul), 1);
    return address;
}

export fn gs_frexp_mant(value: f64) -> f64 {
    var exponent: i32 = 0;
    return host_conv_frexp(value, unsafe_c_address<c_ptr<i32>>(exponent));
}

export fn gs_json_strtod(text: const u8[:]) -> f64 {
    let address = host_conv_cstring(text);
    if address == 0 { return 0.0; }
    let value = host_conv_strtod(address, 0);
    host_conv_free(unsafe_c_ptr<c_ptr<void>>(address));
    return value;
}

export fn gs_strftime_epoch(out: u8[:], epoch: i64, format: const u8[:]) -> i64 {
    let text = host_conv_cstring(format);
    if text == 0 { return -1; }
    let instant = if epoch < 0 { host_conv_time(0) } else { epoch };
    let calendar = host_conv_localtime(host_conv_address(unsafe_c_address<c_ptr<const void>>(instant)));
    var written: i64 = -1;
    if calendar != 0 {
        written = host_conv_strftime(host_conv_address(unsafe_c_slice_data<c_ptr<const void>>(out)),
            out.len as! u64, text, calendar) as! i64;
    }
    host_conv_free(unsafe_c_ptr<c_ptr<void>>(text));
    return written;
}
