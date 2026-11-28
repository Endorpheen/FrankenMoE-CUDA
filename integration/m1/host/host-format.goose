type HostSnprintf = c_fnptr(c_ptr<u8>, u64, c_ptr<const u8>, ...) -> i32;
type HostSortCompare = c_fnptr(c_ptr<const void>, c_ptr<const void>) -> i32;

extern fn fg_host_snprintf_address() -> u64;
extern fn qsort(pairs: c_ptr<void>, count: u64, size: u64, compare: HostSortCompare);

fn host_snprintf() -> HostSnprintf {
    return unsafe_c_fnptr<HostSnprintf>(fg_host_snprintf_address());
}

fn host_format_at(out: u8[:], at: i64, value: f64, format: const u8[:]) -> i64 {
    if at < 0 || at >= out.len { return 0; }
    let call = host_snprintf();
    return call(unsafe_c_address<c_ptr<u8>>(out[at]), (out.len - at) as u64,
        unsafe_c_slice_data<c_ptr<const u8>>(format), value) as i64;
}

export fn fg637_fmt2(out: u8[:], value: f64) -> i64 {
    let call = host_snprintf();
    let format = "%.2f\0";
    return call(unsafe_c_slice_data<c_ptr<u8>>(out), out.len as u64,
        unsafe_c_slice_data<c_ptr<const u8>>(format), value) as i64;
}

export fn gs_json_fmt_e(out: u8[:], precision: i64, value: f64) -> i64 {
    let call = host_snprintf();
    let format = "%.*e\0";
    return call(unsafe_c_slice_data<c_ptr<u8>>(out), out.len as u64,
        unsafe_c_slice_data<c_ptr<const u8>>(format), precision as! i32, value) as i64;
}

export fn gs_to_string_f64(out: u8[:], value: f64) -> i64 {
    let call = host_snprintf();
    let format = "%f\0";
    return call(unsafe_c_slice_data<c_ptr<u8>>(out), out.len as u64,
        unsafe_c_slice_data<c_ptr<const u8>>(format), value) as i64;
}

export fn mp_fmt_f1(out: u8[:], at: i64, value: f64) -> i64 {
    return host_format_at(out, at, value, "%.1f\0");
}

export fn mt_fmt_g6(out: u8[:], at: i64, value: f64) -> i64 {
    return host_format_at(out, at, value, "%.6g\0");
}

export fn host_iq_compare(left: c_ptr<const void>, right: c_ptr<const void>) -> i32 {
    var a: f32 = 0.0;
    var b: f32 = 0.0;
    host_quant_copy(unsafe_c_address<c_ptr<void>>(a), left, 4);
    host_quant_copy(unsafe_c_address<c_ptr<void>>(b), right, 4);
    return if a < b { -1 } else { if a > b { 1 } else { 0 } };
}

fn host_sort_pairs(address: u64, count: i64) {
    let compare = unsafe_c_callback<HostSortCompare>(host_iq_compare);
    qsort(unsafe_c_ptr<c_ptr<void>>(address), count as! u64, 8, compare);
}

export fn qz2_sort16(address: u64, count: i64) {
    host_sort_pairs(address, count);
}

export fn qz_sort16(address: u64, count: i64) {
    host_sort_pairs(address, count);
}
