const BONSAI_DRIVER_LIBRARY: i64 = 19;
var bonsai_cuda_table: u64[20];

type BonsaiErrorString = c_fnptr(i32, u64) -> i32;
extern fn fg_bonsai_bind_cuda_once() -> i32;
extern "fg_cuda_host_view_u8" fn bonsai_driver_text(address: u64, length: i64) -> u8[:];

fn bonsai_driver_string(address: u64) -> const u8[:] {
    return bonsai_driver_text(address, vocab_strlen(address) as! i64);
}

export fn fg_bonsai_bind_cuda() {
    let library = bonsai_open_cuda_library();
    if library == 0 {
        vocab_log("Bonsai CUDA driver unavailable\n");
        return;
    }
    let names = vocab_text("cuInit\0cuDeviceGet\0cuCtxCreate_v2\0cuCtxDestroy_v2\0cuCtxPushCurrent_v2\0cuCtxPopCurrent_v2\0cuCtxSynchronize\0cuModuleLoad\0cuModuleLoadData\0cuModuleGetFunction\0cuMemAlloc_v2\0cuMemGetInfo_v2\0cuMemcpyHtoD_v2\0cuMemcpyDtoDAsync_v2\0cuMemcpyDtoH_v2\0cuMemsetD8_v2\0cuFuncSetAttribute\0cuGetErrorString\0cuLaunchKernel\0");
    var entry = names.address;
    for index in BONSAI_DRIVER_LIBRARY {
        let name = bonsai_driver_string(entry);
        let symbol = unsafe_c_library_symbol(library, name);
        if symbol == 0 {
            unsafe_c_library_close(library);
            vocab_log(str("Bonsai CUDA driver missing ", name, "\n"));
            return;
        }
        bonsai_cuda_table[index] = symbol;
        entry += (name.len as! u64) + 1;
    }
    bonsai_cuda_table[BONSAI_DRIVER_LIBRARY] = library;
}

fn bonsai_cuda_ready() -> bool {
    return fg_bonsai_bind_cuda_once() == 0 && bonsai_cuda_table[BONSAI_DRIVER_LIBRARY] != 0;
}

fn fg_bonsai_cuda_dispatch() -> u64 {
    return vocab_address(unsafe_c_address<c_ptr<const void>>(bonsai_cuda_table[0]));
}

fn fg_bonsai_cuda_check(status: i32, where: u64) -> i32 {
    if status == 0 { return 0; }
    var message: u64 = 0;
    unsafe_c_fnptr<BonsaiErrorString>(bonsai_cuda_table[17])(status,
        vocab_address(unsafe_c_address<c_ptr<const void>>(message)));
    let reason = if message == 0 { "unknown" } else { bonsai_driver_string(message) };
    vocab_log(str(bonsai_driver_string(where), ": CUDA ", status as! i64, " (", reason, ")\n"));
    return -1;
}
