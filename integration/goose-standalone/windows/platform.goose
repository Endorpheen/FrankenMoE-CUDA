extern "fg_win_api_MultiByteToWideChar" fn MultiByteToWideChar(code_page: u32, flags: u32, input: u64, size: i32, output: u64, capacity: i32) -> i32;
extern "fg_win_api_WideCharToMultiByte" fn WideCharToMultiByte(code_page: u32, flags: u32, input: u64, size: i32,
    output: u64, capacity: i32, replacement: u64, used_replacement: u64) -> i32;
extern "fg_win_api_CreateFileW" fn CreateFileW(path: u64, access: u32, share: u32, security: u64, disposition: u32, flags: u32, template: u64) -> u64;
extern "fg_win_api_GetFileType" fn GetFileType(file: u64) -> u32;
extern "fg_win_api_GetFileSizeEx" fn GetFileSizeEx(file: u64, out: c_ptr<void>) -> i32;
extern "fg_win_api_CreateFileMappingW" fn CreateFileMappingW(file: u64, security: u64, protection: u32, high: u32, low: u32, name: u64) -> u64;
extern "fg_win_api_MapViewOfFile" fn MapViewOfFile(mapping: u64, access: u32, high: u32, low: u32, bytes: u64) -> u64;
extern "fg_win_api_CloseHandle" fn CloseHandle(handle: u64) -> i32;
extern "fg_win_api_GetSystemInfo" fn GetSystemInfo(out: c_ptr<void>);
extern "fg_win_api_QueryWorkingSetEx" fn QueryWorkingSetEx(process: u64, entries: c_ptr<void>, bytes: u32) -> i32;
extern "fg_win_api_GetCurrentProcess" fn GetCurrentProcess() -> u64;
extern "fg_win_api_GlobalMemoryStatusEx" fn GlobalMemoryStatusEx(out: c_ptr<void>) -> i32;
extern "fg_win_api_WSAStartup" fn WSAStartup(version: u16, data: c_ptr<void>) -> i32;
extern "fg_win_api_GetThreadGroupAffinity" fn GetThreadGroupAffinity(thread: u64, out: c_ptr<void>) -> i32;
extern "fg_win_api_GetCurrentThread" fn GetCurrentThread() -> u64;
extern "fg_win_api_SetThreadAffinityMask" fn SetThreadAffinityMask(thread: u64, mask: u64) -> u64;
extern "fg_win_api_GetCurrentProcessorNumber" fn GetCurrentProcessorNumber() -> u32;
extern "malloc" fn win_alloc(bytes: u64) -> c_ptr<void>;
extern "free" fn win_free(pointer: c_ptr<void>);

const WIN_CP_UTF8: u32 = 65001;
const WIN_MB_ERR_INVALID_CHARS: u32 = 8;
const WIN_WC_ERR_INVALID_CHARS: u32 = 128;
const WIN_INVALID_HANDLE: u64 = 0xffffffffffffffff;

fn win_address(pointer: c_ptr<void>) -> u64 {
    var address: u64 = 0;
    socket_copy(unsafe_c_address<c_ptr<void>>(address), unsafe_c_address<c_ptr<const void>>(pointer), 8);
    return address;
}

fn win_write_word(address: u64, value: u64) {
    socket_copy(unsafe_c_ptr<c_ptr<void>>(address), unsafe_c_address<c_ptr<const void>>(value), 8);
}

fn win_read_word(address: u64) -> u64 {
    var value: u64 = 0;
    socket_copy(unsafe_c_address<c_ptr<void>>(value), unsafe_c_ptr<c_ptr<const void>>(address), 8);
    return value;
}

export fn fg_win_to_wide(input: u64) -> u64 {
    let count = MultiByteToWideChar(WIN_CP_UTF8, WIN_MB_ERR_INVALID_CHARS, input, -1, 0, 0);
    if count == 0 { return 0; }
    let buffer = win_alloc((count as! u64) * 2);
    if buffer == null { return 0; }
    let result = win_address(buffer);
    if MultiByteToWideChar(WIN_CP_UTF8, WIN_MB_ERR_INVALID_CHARS, input, -1, result, count) == 0 {
        win_free(buffer);
        return 0;
    }
    return result;
}

export fn fg_win_to_utf8(input: u64) -> u64 {
    let count = WideCharToMultiByte(WIN_CP_UTF8, WIN_WC_ERR_INVALID_CHARS, input, -1, 0, 0, 0, 0);
    if count == 0 { return 0; }
    let buffer = win_alloc(count as! u64);
    if buffer == null { return 0; }
    let result = win_address(buffer);
    if WideCharToMultiByte(WIN_CP_UTF8, WIN_WC_ERR_INVALID_CHARS, input, -1, result, count, 0, 0) == 0 {
        win_free(buffer);
        return 0;
    }
    return result;
}

export fn fg_win_map_model(path: u64, bytes: u64) -> u64 {
    let wide = fg_win_to_wide(path);
    if wide == 0 { return 0; }
    let file = CreateFileW(wide, 0x80000000, 1, 0, 3, 128, 0);
    win_free(unsafe_c_ptr<c_ptr<void>>(wide));
    if file == WIN_INVALID_HANDLE { return 0; }
    var size: i64 = 0;
    if GetFileType(file) != 1 || GetFileSizeEx(file, unsafe_c_address<c_ptr<void>>(size)) == 0 || size <= 0 {
        CloseHandle(file);
        return 0;
    }
    let mapping = CreateFileMappingW(file, 0, 2, 0, 0, 0);
    CloseHandle(file);
    if mapping == 0 { return 0; }
    let view = MapViewOfFile(mapping, 4, 0, 0, size as! u64);
    CloseHandle(mapping);
    if view != 0 { win_write_word(bytes, size as! u64); }
    return view;
}

export fn fg_win_page_size() -> u64 {
    var info: u64[6];
    GetSystemInfo(unsafe_c_address<c_ptr<void>>(info[0]));
    return info[0] >> 32;
}

export fn fg_win_resident(address: u64, bytes: u64, pages: u64) -> i32 {
    let page_size = fg_win_page_size();
    let count = bytes / page_size + (if bytes % page_size != 0 { 1 } else { 0 });
    var entries: u64[512];
    var at: u64 = 0;
    while at < count {
        let n = if count - at < 256 { count - at } else { 256 };
        for i in (n as! i64) {
            entries[i * 2] = address + (at + (i as! u64)) * page_size;
            entries[i * 2 + 1] = 0;
        }
        if QueryWorkingSetEx(GetCurrentProcess(), unsafe_c_address<c_ptr<void>>(entries[0]), (n * 16) as! u32) == 0 {
            return -1;
        }
        for i in (n as! i64) {
            let valid = (entries[i * 2 + 1] & 1) as! u8;
            socket_copy(unsafe_c_ptr<c_ptr<void>>(pages + at + (i as! u64)),
                unsafe_c_address<c_ptr<const void>>(valid), 1);
        }
        at += n;
    }
    return 0;
}

export fn fg_win_memory(available: bool) -> u64 {
    var info: u64[8] = [64, 0, 0, 0, 0, 0, 0, 0];
    if GlobalMemoryStatusEx(unsafe_c_address<c_ptr<void>>(info[0])) == 0 { return 0; }
    return if available { info[2] } else { info[1] };
}

export fn fg_win_network_ready() -> bool {
    var data: u64[51];
    return WSAStartup(514, unsafe_c_address<c_ptr<void>>(data[0])) == 0;
}

export fn fg_win_socket_timeout(fd: i64) -> i32 {
    let timeout: u32 = 30000;
    let value = unsafe_c_address<c_ptr<const void>>(timeout);
    if setsockopt(fd as! u64, 65535, 0x1006, value, 4) != 0 { return -1; }
    return setsockopt(fd as! u64, 65535, 0x1005, value, 4);
}

export fn sched_getaffinity(pid: i32, bytes: u64, output: u64) -> i32 {
    if pid != 0 || bytes < 8 { return -1; }
    var affinity: u64[2];
    if GetThreadGroupAffinity(GetCurrentThread(), unsafe_c_address<c_ptr<void>>(affinity[0])) == 0 { return -1; }
    win_write_word(output, affinity[0]);
    return 0;
}

export fn sched_setaffinity(pid: i32, bytes: u64, input: u64) -> i32 {
    if pid != 0 || bytes < 8 { return -1; }
    let mask = win_read_word(input);
    if mask == 0 { return -1; }
    return if SetThreadAffinityMask(GetCurrentThread(), mask) != 0 { 0 } else { -1 };
}

export fn sched_getcpu() -> i32 { return GetCurrentProcessorNumber() as! i32; }
