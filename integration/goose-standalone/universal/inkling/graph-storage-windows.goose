extern "malloc" fn graph_storage_malloc(bytes: u64) -> c_ptr<void>;
extern fn fg_win_memory(available: bool) -> u64;

export fn fg143_storage_alloc(bytes: u64) -> u64 {
    let pointer=graph_storage_malloc(bytes);
    var address: u64=0;
    graph_host_copy(unsafe_c_address<c_ptr<void>>(address),unsafe_c_address<c_ptr<const void>>(pointer),8);
    return address;
}

export fn fg143_storage_ram_available() -> u64 { return fg_win_memory(true); }
