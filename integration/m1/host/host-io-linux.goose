extern fn host_runtime_read(fd: i32, data: u64, bytes: u64) -> i64;
extern fn host_runtime_write(fd: i32, data: u64, bytes: u64) -> i64;
extern fn host_runtime_close(fd: i32) -> i32;

export fn qz_rd(fd: i64, buffer: u64, bytes: i64) -> i64 {
    return host_runtime_read(fd as! i32, buffer, bytes as! u64);
}

export fn qz_wr(fd: i64, buffer: u64, bytes: i64) -> i64 {
    return host_runtime_write(fd as! i32, buffer, bytes as! u64);
}

export fn qz_cl(fd: i64) -> i64 { return host_runtime_close(fd as! i32) as! i64; }

extern fn host_posix_memalign(output: c_ptr<u64>, alignment: u64, bytes: u64) -> i32;
extern fn host_pool_free(address: c_ptr<void>);

export fn ggml_aligned_malloc(bytes: u64) -> u64 {
    if bytes == 0 {
        common_memory_error("Behavior may be unexpected when allocating 0 bytes for ggml_aligned_malloc!\n");
        return 0;
    }
    var address: u64 = 0;
    let status = host_posix_memalign(unsafe_c_address<c_ptr<u64>>(address), 64, bytes);
    if status == 0 { return address; }
    let error = if status == 22 { "invalid alignment value" } else {
        if status == 12 { "insufficient memory" } else { "unknown allocation error" }
    };
    common_memory_error(str("ggml_aligned_malloc: ", error, " (attempted to allocate ", bytes as! i64, " bytes)\n"));
    return 0;
}

export fn ggml_aligned_free(address: u64, bytes: u64) {
    host_pool_free(unsafe_c_ptr<c_ptr<void>>(address));
}

extern fn trace_open(path: u64, mode: u64) -> u64;
extern fn trace_line(output: u64, size: i32, file: u64) -> u64;
extern fn trace_close(file: u64) -> i32;
extern fn trace_pipe(output: u64) -> i32;
extern fn trace_pid() -> i32;
extern fn trace_fork() -> i32;
extern fn trace_wait(pid: i32, result: u64, options: i32) -> i32;
extern fn trace_exec(name: u64, args: u64) -> i32;
extern fn trace_exit(status: i32);
extern fn trace_frames(output: u64, limit: i32) -> i32;
extern fn trace_symbols(frames: u64, count: i32, fd: i32);
type HostPrctl = c_fnptr(i32, ...) -> i32;
extern fn fg_host_prctl_address() -> u64;

fn fg_ggml_allow_debugger(child: i32) {
    let call = unsafe_c_fnptr<HostPrctl>(fg_host_prctl_address());
    call(0x59616d61, child as u64, 0 as u64, 0 as u64, 0 as u64);
}

fn trace_text(text: const u8[:]) -> u64 {
    let pointer = unsafe_c_slice_data<c_ptr<const void>>(text);
    var address: u64 = 0;
    arena_error_copy(unsafe_c_address<c_ptr<void>>(address), unsafe_c_address<c_ptr<const void>>(pointer), 8);
    return address;
}

fn trace_debugger_attached() -> bool {
    let file = trace_open(trace_text("/proc/self/status\0"), trace_text("r\0"));
    if file == 0 { return false; }
    var line: u8[128];
    let buffer = trace_text(line);
    var attached = false;
    while trace_line(buffer, 128, file) != 0 {
        if line[0 .. 10] == "TracerPid:" {
            attached = line[10] != 9 || line[11] != 48 || line[12] != 10;
            break;
        }
    }
    trace_close(file);
    return attached;
}

fn trace_child(parent: i32) {
    var pid_buffer: u8[16];
    var digits: u8[16];
    var pid_size: i64 = 0;
    var value = parent as! u32;
    while value != 0 || pid_size == 0 {
        digits[pid_size] = (48 + value % 10) as! u8;
        pid_size += 1;
        value /= 10;
    }
    for index in pid_size { pid_buffer[index] = digits[pid_size - index - 1]; }
    pid_buffer[pid_size] = 0;
    let pid_arg = trace_text(pid_buffer);
    var attach: u8[24];
    let prefix: const u8[:] = "attach ";
    for index in prefix.len { attach[index] = prefix[index]; }
    for index in pid_size + 1 { attach[prefix.len + index] = pid_buffer[index]; }
    let gdb = trace_text("gdb\0");
    let option = trace_text("-ex\0");
    let quit = trace_text("quit\0");
    let gdb_args: u64[13] = [gdb, trace_text("--batch\0"), option, trace_text("set style enabled on\0"),
        option, trace_text(attach), option, trace_text("bt -frame-info source-and-location\0"),
        option, trace_text("detach\0"), option, quit, 0];
    let args = unsafe_c_address<c_ptr<const void>>(gdb_args[0]);
    var argv: u64 = 0;
    arena_error_copy(unsafe_c_address<c_ptr<void>>(argv), unsafe_c_address<c_ptr<const void>>(args), 8);
    trace_exec(gdb, argv);
    let lldb = trace_text("lldb\0");
    let command = trace_text("-o\0");
    let lldb_args: u64[9] = [lldb, trace_text("--batch\0"), command, trace_text("bt\0"),
        command, quit, trace_text("-p\0"), pid_arg, 0];
    let next_args = unsafe_c_address<c_ptr<const void>>(lldb_args[0]);
    arena_error_copy(unsafe_c_address<c_ptr<void>>(argv), unsafe_c_address<c_ptr<const void>>(next_args), 8);
    trace_exec(lldb, argv);
    var frames: u64[100];
    let frame_pointer = unsafe_c_address<c_ptr<const void>>(frames[0]);
    var output: u64 = 0;
    arena_error_copy(unsafe_c_address<c_ptr<void>>(output), unsafe_c_address<c_ptr<const void>>(frame_pointer), 8);
    trace_symbols(output, trace_frames(output, 100), 2);
    trace_exit(0);
}

export fn ggml_print_backtrace() {
    if graph_crt_getenv(trace_text("GGML_NO_BACKTRACE\0") as! i64) != 0 || trace_debugger_attached() { return; }
    var lock: i32[2] = [-1, -1];
    let lock_pointer = unsafe_c_address<c_ptr<const void>>(lock[0]);
    var output: u64 = 0;
    arena_error_copy(unsafe_c_address<c_ptr<void>>(output), unsafe_c_address<c_ptr<const void>>(lock_pointer), 8);
    trace_pipe(output);
    let parent = trace_pid();
    let child = trace_fork();
    if child == 0 {
        host_runtime_close(lock[1]);
        host_runtime_read(lock[0], output, 1);
        host_runtime_close(lock[0]);
        trace_child(parent);
        return;
    }
    if child > 0 { fg_ggml_allow_debugger(child); }
    host_runtime_close(lock[1]);
    host_runtime_close(lock[0]);
    if child > 0 { trace_wait(child, 0, 0); }
}

fn ggml_stderr_write(address: u64, bytes: u64) {
    var written: u64 = 0;
    while written < bytes {
        let count = host_runtime_write(2, address + written, bytes - written);
        if count <= 0 { return; }
        written += count as! u64;
    }
}
