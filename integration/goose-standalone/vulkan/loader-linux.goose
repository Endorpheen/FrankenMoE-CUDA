extern "fg_vk_loader_api_dlopen" fn vk_linux_open(name: u64, flags: i32) -> u64;
extern "fg_vk_loader_api_dlsym" fn vk_loader_symbol(library: u64, name: u64) -> u64;
extern "fg_vk_loader_api_dlclose" fn vk_linux_close(library: u64) -> i32;

fn vk_loader_open() -> u64 {
    const RTLD_NOW_LOCAL: i32 = 2;
    var name: u8[15];
    let filename: const u8[:] = "libvulkan.so.1";
    for i in filename.len { name[i] = filename[i]; }
    name[filename.len] = 0;
    return vk_linux_open(vk_queue_address(unsafe_c_address<c_ptr<const void>>(name[0])), RTLD_NOW_LOCAL);
}

fn vk_loader_close(library: u64) { vk_linux_close(library); }
