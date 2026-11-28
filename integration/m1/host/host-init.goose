extern fn fg_host_model_initialize(argc: i32, argv: u64);
extern fn fg_host_native_iq_provider() -> u64;
extern fn fg_host_iq_load(address: u64) -> u64;
extern fn fg_host_iq_publish(address: u64, table: u64) -> u64;
extern fn fg_cuda_host_load_acquire_u32(address: u64) -> u32;
extern fn fg_cuda_host_store_release_u32(address: u64, value: u32);

var host_iq_descriptor: u64;
var host_initialized: u32;
var host_init_mutex: u64[5] = [HOST_INIT_MUTEX, 0, 0, 0, 0];

fn host_init_pointer(pointer: c_ptr<const void>) -> u64 {
    var address: u64 = 0;
    arena_error_copy(unsafe_c_address<c_ptr<void>>(address), unsafe_c_address<c_ptr<const void>>(pointer), 8);
    return address;
}

fn host_iq_slot() -> u64 { return host_init_pointer(unsafe_c_address<c_ptr<const void>>(host_iq_descriptor)); }

export fn fg_goose_host_iq_tables_pointer() -> u64 {
    return fg_host_iq_load(host_iq_slot());
}

export fn fg_goose_host_iq_tables_bound() -> i32 {
    return if fg_goose_host_iq_tables_pointer() != 0 { 1 } else { 0 };
}

export fn fg_goose_host_bind_iq_tables(table: u64) -> i32 {
    if table == 0 { return -1; }
    if (builder_word(table as! i64) as! u64) != 0x0000004000000001 { return -2; }
    for field in 6 {
        if builder_word((table + 16 + (field as! u64) * 8) as! i64) == 0 { return -3; }
    }
    let old = fg_host_iq_publish(host_iq_slot(), table);
    return if old == 0 || old == table { 0 } else { -4 };
}

fn host_init_status() -> u64 {
    return host_init_pointer(unsafe_c_address<c_ptr<const void>>(host_initialized));
}

export fn fg_goose_host_ensure_init() {
    let status = host_init_status();
    if fg_cuda_host_load_acquire_u32(status) != 0 { return; }
    let lock = unsafe_c_address<c_ptr<void>>(host_init_mutex[0]);
    assert(pthread_mutex_lock(lock) == 0);
    if fg_cuda_host_load_acquire_u32(status) == 0 {
        if fg_goose_host_iq_tables_pointer() == 0 {
            let table = fg_host_native_iq_provider();
            if table != 0 { fg_goose_host_bind_iq_tables(table); }
        }
        fg_host_model_initialize(0, 0);
        fg_cuda_host_store_release_u32(status, 1);
    }
    assert(pthread_mutex_unlock(lock) == 0);
}

export fn fg_goose_host_init_with_tables(table: u64) -> i32 {
    let status = fg_goose_host_bind_iq_tables(table);
    if status != 0 { return status; }
    fg_goose_host_ensure_init();
    return 0;
}

export fn fg_goose_host_init_runs() -> i64 {
    return fg_cuda_host_load_acquire_u32(host_init_status()) as! i64;
}
