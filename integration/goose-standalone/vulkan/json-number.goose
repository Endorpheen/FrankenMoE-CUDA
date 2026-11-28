extern "fg_vulkan_crt_strtod" fn vk_parse_decimal(text: u64, end: u64) -> f64;
extern "malloc" fn vk_json_alloc(size: u64) -> c_ptr<void>;
extern "free" fn vk_json_free(pointer: c_ptr<void>);

export fn gs_json_strtod(source: const u8[:]) -> f64 {
    let count = source.len;
    if count < 0 { return 0.0; }
    if count < 64 {
        var text: u8[64];
        for i in count { text[i] = source[i]; }
        text[count] = 0;
        return vk_parse_decimal(vk_queue_address(unsafe_c_address<c_ptr<const void>>(text[0])), 0);
    }

    let target = vk_json_alloc((count as! u64) + 1);
    if target == null { return 0.0; }
    var address: u64 = 0;
    vk_queue_memcpy(unsafe_c_address<c_ptr<void>>(address), unsafe_c_address<c_ptr<const void>>(target), 8);
    vk_queue_memcpy(target, unsafe_c_slice_data<c_ptr<const void>>(source), count as! u64);
    let nul: u8 = 0;
    vk_queue_memcpy(unsafe_c_ptr<c_ptr<void>>(address + (count as! u64)),
        unsafe_c_address<c_ptr<const void>>(nul), 1);
    let value = vk_parse_decimal(address, 0);
    vk_json_free(target);
    return value;
}
