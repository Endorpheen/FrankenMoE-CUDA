const VK_LOADER_HANDLE: u64 = 0;
const VK_LOADER_ENTRY: u64 = 8;

export fn fg_vk_load(vk: u64) -> i32 {
    let library = vk_loader_open();
    vk_upload_put(vk + VK_LOADER_HANDLE, library);
    if library != 0 {
        var name: u8[22];
        let symbol: const u8[:] = "vkGetInstanceProcAddr";
        for i in symbol.len { name[i] = symbol[i]; }
        name[symbol.len] = 0;
        let address = vk_queue_address(unsafe_c_address<c_ptr<const void>>(name[0]));
        vk_upload_put(vk + VK_LOADER_ENTRY, vk_loader_symbol(library, address));
    }
    return if vk_queue_word(vk + VK_LOADER_ENTRY) != 0 { 1 } else { 0 };
}

export fn fg_vk_unload(vk: u64) {
    let library = vk_queue_word(vk + VK_LOADER_HANDLE);
    if library != 0 { vk_loader_close(library); }
}
