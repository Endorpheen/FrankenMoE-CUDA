extern "fg_win_api_GetEnvironmentVariableW" fn GetEnvironmentVariableW(key: u64, buffer: u64, capacity: u32) -> u32;
extern "fg_win_api_ReadFile" fn ReadFile(file: u64, buffer: u64, size: u32, read_count: u64, overlapped: u64) -> i32;
extern "fg_win_api_GetStdHandle" fn GetStdHandle(kind: u32) -> u64;
extern "fg_win_api_GetConsoleMode" fn GetConsoleMode(handle: u64, mode: u64) -> i32;
extern "fg_win_api_SetConsoleOutputCP" fn SetConsoleOutputCP(page: u32) -> i32;
extern "fg_win_api_SetConsoleMode" fn SetConsoleMode(handle: u64, mode: u32) -> i32;
extern "fg_win_api__wfopen" fn web_open_log(path: u64, mode: u64) -> u64;
extern "fg_win_api_fwrite" fn web_write_file(data: u64, size: u64, count: u64, file: u64) -> u64;
extern "fg_win_api_fclose" fn web_close_file(file: u64) -> i32;
extern "fg_win_api_getenv" fn web_getenv(key: u64) -> u64;
extern "gs_os_write_stderr" fn web_stderr(text: const u8[:]);

var web_method: i64;
var web_route: i64;
var web_warned: u32;
var web_console_initialized: bool;
var web_console_color: bool;

fn web_address(pointer: c_ptr<const void>) -> u64 {
    var address: u64 = 0;
    socket_copy(unsafe_c_address<c_ptr<void>>(address), unsafe_c_address<c_ptr<const void>>(pointer), 8);
    return address;
}

fn web_send(fd: i64, data: const u8[:]) -> bool {
    var at: i64 = 0;
    while at < data.len {
        let left = data.len - at;
        let size: i64 = if left < 65536 { left } else { 65536 };
        let written = gx_write(fd, data[at .. at + size], size);
        if written <= 0 { return false; }
        at += written;
    }
    return true;
}

fn web_header(fd: i64, mime: const u8[:], size: u64) -> bool {
    let text = str("HTTP/1.1 200 OK\r\nContent-Type: ", mime, "\r\nContent-Length: ", size,
        "\r\nX-Content-Type-Options: nosniff\r\nCache-Control: no-cache\r\nConnection: close\r\n\r\n");
    return text.len < 256 && web_send(fd, text);
}

fn web_mime(path: const u8[:]) -> const u8[:] {
    var at = path.len;
    while at > 0 {
        at -= 1;
        if path[at] != 46 { continue; }
        let extension = path[at..];
        if extension == ".html" { return "text/html; charset=utf-8"; }
        if extension == ".js" { return "text/javascript; charset=utf-8"; }
        if extension == ".css" { return "text/css; charset=utf-8"; }
        if extension == ".svg" { return "image/svg+xml"; }
        if extension == ".png" { return "image/png"; }
        if extension == ".ico" { return "image/x-icon"; }
        if extension == ".woff2" { return "font/woff2"; }
        if extension == ".webmanifest" { return "application/manifest+json"; }
        if extension == ".json" { return "application/json"; }
        if extension == ".wasm" { return "application/wasm"; }
        break;
    }
    return "application/octet-stream";
}

fn web_valid_path(request: const u8[:]) -> bool {
    if request.len < 1 || request.len >= 512 || unsafe_c_slice_data<c_ptr<const void>>(request) == null || request[0] != 47 { return false; }
    if request.len == 1 { return true; }
    var first = true;
    for i in 1 .. request.len {
        let byte = request[i];
        if byte == 47 {
            if first || request[i - 1] == 46 { return false; }
            first = true;
        } else {
            let alphanumeric = (byte >= 97 && byte <= 122) || (byte >= 65 && byte <= 90) ||
                (byte >= 48 && byte <= 57);
            if !alphanumeric && byte != 95 && byte != 45 && (byte != 46 || first) { return false; }
            first = false;
        }
    }
    return !first && request[request.len - 1] != 46;
}

fn web_widen_ascii(text: const u8[:], out: u16[:]) {
    for i in text.len { out[i] = text[i] as! u16; }
    out[text.len] = 0;
}

fn web_asset_open(relative: const u8[:]) -> u64 {
    var key: u16[32];
    var path: u16[4608];
    let path_addr = web_address(unsafe_c_address<c_ptr<const void>>(path[0]));
    web_widen_ascii("GOOSEINFER_WEB_ROOT", key);
    let key_addr = web_address(unsafe_c_address<c_ptr<const void>>(key[0]));
    var count = GetEnvironmentVariableW(key_addr, path_addr, 4096);
    if count == 0 {
        web_widen_ascii("FRANKENMOE_WEB_ROOT", key);
        count = GetEnvironmentVariableW(key_addr, path_addr, 4096);
    }
    if count == 0 || count >= 4096 { return WIN_INVALID_HANDLE; }
    let at = count as! i64;
    path[at] = 92;
    for i in relative.len {
        path[at + i + 1] = if relative[i] == 47 { 92 } else { relative[i] as! u16 };
    }
    path[at + relative.len + 1] = 0;
    return CreateFileW(path_addr, 0x80000000, 1, 0, 3, 128, 0);
}

export fn fg_http_windows_asset(fd: i64, request: const u8[:]) -> bool {
    if !web_valid_path(request) { return false; }
    let relative = if request.len == 1 { "index.html" } else { request[1..] };
    let file = web_asset_open(relative);
    if file == WIN_INVALID_HANDLE { return false; }
    var size: i64 = 0;
    if GetFileType(file) != 1 || GetFileSizeEx(file, unsafe_c_address<c_ptr<void>>(size)) == 0 || size < 0 {
        CloseHandle(file);
        return false;
    }
    if web_header(fd, web_mime(relative), size as! u64) {
        var buffer: u8[16384];
        let buffer_addr = web_address(unsafe_c_address<c_ptr<const void>>(buffer[0]));
        var remaining = size;
        while remaining > 0 {
            var count: u32 = 0;
            let chunk = (if remaining < buffer.len { remaining } else { buffer.len }) as! u32;
            if ReadFile(file, buffer_addr, chunk, web_address(unsafe_c_address<c_ptr<const void>>(count)), 0) == 0 ||
                count == 0 || !web_send(fd, buffer[0 .. count as! i64]) { break; }
            remaining -= count as! i64;
        }
    }
    CloseHandle(file);
    return true;
}

export fn fg_http_windows_sse(fd: i64, body: const u8[:]) {
    let prefix: const u8[:] = "data: ";
    let suffix: const u8[:] = "\n\ndata: [DONE]\n\n";
    if web_header(fd, "text/event-stream; charset=utf-8", (prefix.len + body.len + suffix.len) as! u64) &&
        web_send(fd, prefix) && web_send(fd, body) { web_send(fd, suffix); }
}

fn web_log(text: const u8[:]) {
    var name: u16[20];
    web_widen_ascii("gooseinfer-http.log", name);
    let mode: u16[3] = [97,98,0];
    let log = web_open_log(web_address(unsafe_c_address<c_ptr<const void>>(name[0])),
        web_address(unsafe_c_address<c_ptr<const void>>(mode[0])));
    if log == 0 { return; }
    web_write_file(web_address(unsafe_c_slice_data<c_ptr<const void>>(text)), 1, text.len as! u64, log);
    web_close_file(log);
}

fn web_route_name(route: i64) -> const u8[:] {
    if route == 1 { return "/health"; }
    if route == 3 { return "/props"; }
    if route == 5 { return "/v1/models"; }
    if route == 11 { return "/completion"; }
    if route == 12 { return "/v1/completions"; }
    if route == 13 { return "/v1/chat/completions"; }
    if route == 15 { return "/v1/responses"; }
    if route == 36 { return "/v1/streams/lookup"; }
    if route == 39 { return "/tools"; }
    if route == 101 { return "static-asset"; }
    return "unmatched";
}

export fn fg_http_windows_request(method: i64, route: i64) {
    web_method = method;
    web_route = route;
}

export fn fg_http_windows_response(status: i64, body: const u8[:]) {
    let method = if web_method == 1 { "GET" } else { if web_method == 2 { "POST" } else { "unknown" } };
    let route = web_route_name(web_route);
    if status >= 400 && body.len > 0 && body.len <= 160 && unsafe_c_slice_data<c_ptr<const void>>(body) != null {
        var length: i64 = 0;
        while length < body.len && body[length] != 0 { length += 1; }
        web_log(str(method, " ", route, " ", status, " ", body[0 .. length], "\n"));
    } else {
        web_log(str(method, " ", route, " ", status, "\n"));
    }
}

export fn fg_http_windows_ignored(field: i64) {
    if field < 1 || field > 3 { return; }
    let bit = (1 as! u32) << (field as! u32);
    if (web_warned & bit) != 0 { return; }
    web_warned |= bit;
    let name = if field == 1 { "tools" } else { if field == 2 { "tool_choice" } else { "response_format" } };
    web_stderr(str("GooseInfer HTTP: ", name, " is unsupported and ignored\n"));
    web_log(str("warning: ", name, " is unsupported and ignored\n"));
}

export fn fg_http_console(line: const u8[:]) {
    if !web_console_initialized {
        let terminal = GetStdHandle(0xfffffff4);
        var mode: u32 = 0;
        if GetConsoleMode(terminal, web_address(unsafe_c_address<c_ptr<const void>>(mode))) != 0 {
            SetConsoleOutputCP(WIN_CP_UTF8);
            let no_color: u8[9] = [78,79,95,67,79,76,79,82,0];
            web_console_color = web_getenv(web_address(unsafe_c_address<c_ptr<const void>>(no_color[0]))) == 0 &&
                SetConsoleMode(terminal, mode | 4) != 0;
        }
        web_console_initialized = true;
    }
    if line.len <= 0 || unsafe_c_slice_data<c_ptr<const void>>(line) == null { return; }
    if !web_console_color {
        web_stderr(line);
        return;
    }
    var at: i64 = 0;
    while at < line.len {
        var end = at;
        var label: i64 = -1;
        while end < line.len {
            if label < 0 && line[end] == 93 { label = end; }
            end += 1;
            if line[end - 1] == 10 { break; }
        }
        if label >= 0 {
            web_stderr(str("\x1b[33m", line[at .. label + 1], "\x1b[0m"));
            at = label + 1;
        }
        web_stderr(line[at .. end]);
        at = end;
    }
}
