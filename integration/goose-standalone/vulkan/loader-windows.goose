extern "fg_vk_loader_api_LoadLibraryExW" fn vk_windows_open(name: u64, file: u64, flags: u32) -> u64;
extern "fg_vk_loader_api_GetProcAddress" fn vk_loader_symbol(library: u64, name: u64) -> u64;
extern "fg_vk_loader_api_FreeLibrary" fn vk_windows_close(library: u64) -> i32;

fn vk_loader_open() -> u64 {
    const LOAD_LIBRARY_SEARCH_SYSTEM32: u32 = 2048;
    var name: u16[13];
    let filename: const u8[:] = "vulkan-1.dll";
    for i in filename.len { name[i] = filename[i] as! u16; }
    name[filename.len] = 0;
    return vk_windows_open(vk_queue_address(unsafe_c_address<c_ptr<const void>>(name[0])), 0,
        LOAD_LIBRARY_SEARCH_SYSTEM32);
}

fn vk_loader_close(library: u64) { vk_windows_close(library); }
