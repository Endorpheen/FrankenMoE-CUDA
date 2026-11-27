extern "gs_os_write_stderr" fn write_stderr(text: const u8[:]);

fn giga_server_log_placement(kind: i32, bytes: i64, available: i64) {
    if kind == 1 {
        write_stderr(str("GigaChat4: expert VRAM=", bytes, " bytes, available=", available, " bytes\n"));
    } else {
        let label = if kind == 0 { "dense" } else { "state logical" };
        write_stderr(str("GigaChat4: ", label, " VRAM=", bytes, " bytes\n"));
    }
}

fn giga_server_log_open(status: i32, finished: bool) {
    if finished { write_stderr(str("GigaChat4: model ready status=", status, "\n")); }
    else { write_stderr("GigaChat4: binding model\n"); }
}

fn giga_server_trace_execution(value: i64, draft: bool, finished: bool) {
    if finished { write_stderr(str("GigaChat4: step status=", value, "\n")); }
    else {
        let flag = if draft { 1 } else { 0 };
        write_stderr(str("GigaChat4: computing ", value, " tokens, draft=", flag, "\n"));
    }
}
