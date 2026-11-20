// Serial request handling for the standalone model host. CLI and HTTP share
// the tokenizer, model state, executor and sampler through hose_generate.
extern "fg_http_complete" fn ih_complete_abi(prompt: const u8[:], max_tokens: i64,
    ids: i64[:], text: u8[:], result: i64[:]) -> i64;
extern "fg_http_stats" fn ih_stats_abi(stats: i64[:]);
extern "fg_http_listening" fn ih_listening(port: i64);
extern "fg_http_set_timeout" fn ih_timeout(fd: i64) -> i64;

fn ih_line_end(text: const u8[:], start: i64) -> i64 {
    var end = start;
    while end + 1 < text.len {
        if text[end] == 13 && text[end + 1] == 10 { return end + 2; }
        end += 1;
    }
    return -1;
}

// Meta: body offset and length, parsed route, JSON content-type flag.
// Return zero on success or a response status for invalid framing.
fn ih_read(fd: i64, request: u8[:], meta: i64[:]) -> i64 {
    var filled: i64 = 0;
    var header: i64 = -1;
    while header < 0 {
        if filled >= request.len || filled >= HT_MAX_HEADER { return 400; }
        let got = gx_read(fd, request[filled .. request.len], request.len - filled);
        if got <= 0 { return 400; }
        filled += got;
        header = ht_header_end(request[0 .. filled]);
    }
    if header > HT_MAX_HEADER { return 400; }

    let headers = request[0 .. header];
    let first = ih_line_end(headers, 0);
    var line: i64[8] = [0; 8];
    if first <= 0 || rq_parse_line_impl(headers[0 .. first], line) != 0 { return 400; }
    let verb = headers[line[1] .. line[1] + line[2]];
    if !rq_is_token_impl(verb) || line[4] > rq_uri_max() { return 400; }
    let method: i64 = if rq_eq(verb, "GET") { 1 } else if rq_eq(verb, "POST") { 2 } else { 0 };
    let target = headers[line[3] .. line[3] + line[4]];
    var path_len: i64 = 0;
    while path_len < target.len && target[path_len] != '?' { path_len += 1; }
    let route = route_id(method, target[0 .. path_len]);

    var hosts: i64 = 0;
    var host_ok: i64 = 1;
    var length: i64 = -1;
    var json: i64 = 0;
    var at = first;
    var field: i64[5] = [0; 5];
    while at < header - 2 {
        let end = ih_line_end(headers, at);
        if end < 0 || rq_parse_header_impl(headers[at .. end], field) != 0 { return 400; }
        let value = headers[at + field[3] .. at + field[3] + field[4]];
        if hl_ci_eq(headers, at + field[1], field[2], "Host") {
            hosts += 1;
            for i in value.len {
                if !rq_is_host_char(value[i] as i64) { host_ok = 0; }
            }
        } else if hl_ci_eq(headers, at + field[1], field[2], "Content-Length") {
            var size: i64[1] = [0];
            if length >= 0 || rq_decimal_impl(value, size) != 0 { return 400; }
            length = size[0];
        } else if hl_ci_eq(headers, at + field[1], field[2], "Transfer-Encoding") ||
                  hl_ci_eq(headers, at + field[1], field[2], "Expect") {
            return 400;
        } else if hl_ci_eq(headers, at + field[1], field[2], "Content-Type") {
            var type_len: i64 = 0;
            while type_len < value.len && value[type_len] != ';' &&
                  value[type_len] != 32 && value[type_len] != 9 { type_len += 1; }
            if hl_ci_eq(value, 0, type_len, "application/json") { json = 1; }
        }
        at = end;
    }

    if rq_host_violation(hosts, line[7], host_ok, if hosts > 0 { 1 } else { 0 }) != 0 { return 400; }
    if route != 1 && route != 3 && route != 11 { return 404; }
    if route == 11 && length <= 0 { return 400; }
    if length < 0 { length = 0; }
    if length > request.len - header { return 413; }
    let end = header + length;
    while filled < end {
        let got = gx_read(fd, request[filled .. end], end - filled);
        if got <= 0 { return 400; }
        filled += got;
    }
    meta[0] = header;
    meta[1] = length;
    meta[2] = route;
    meta[3] = json;
    return 0;
}

fn ih_response(fd: i64, status: i64, body: const u8[:]) {
    var head: u8[256] = [0; 256];
    let n = ht_response_head_impl(status, body.len, 0, head);
    if n > 0 && ht_write_all_impl(fd, head[0 .. n]) == 0 {
        ht_write_all_impl(fd, body);
    }
}

fn ih_generate(fd: i64, prompt: const u8[:], limit: i64, output: u8[:], response: u8[:], ids: i64[:]) {
    if prompt.len == 0 || limit <= 0 || limit > ids.len {
        ih_response(fd, 400, "{\"error\":\"invalid prompt or n_predict\"}");
        return;
    }
    var result: i64[1] = [0];
    let bytes = ih_complete_abi(prompt, limit, ids, output, result);
    let count = result[0];
    if bytes < 0 || bytes > output.len || count <= 0 || count > limit {
        ih_response(fd, 500, "{\"error\":\"generation failed\"}");
        return;
    }
    var n = cp_put(response, 0, "{\"ids\":[");
    for i in count {
        if i != 0 { n = cp_put(response, n, ","); }
        n = cp_uint(response, n, ids[i]);
    }
    n = cp_put(response, n, "],\"text\":");
    n = esc_string(response, n, output, 0, bytes, 1);
    n = cp_put(response, n, ",\"count\":");
    n = cp_uint(response, n, count);
    n = cp_put(response, n, "}");
    if n <= response.len {
        ih_response(fd, 200, response[0 .. n]);
    } else {
        ih_response(fd, 500, "{\"error\":\"response too large\"}");
    }
}

// Completion options are scalars in one object. Check that shape before
// entering the recursive JSON reader, while leaving string escapes to it.
fn ih_flat_object(body: const u8[:]) -> bool {
    var opened: bool = false;
    var quoted: bool = false;
    var escaped: bool = false;
    for i in body.len {
        let byte = body[i];
        if quoted {
            if escaped { escaped = false; }
            else if byte == 92 { escaped = true; }
            else if byte == '"' { quoted = false; }
        } else if byte == '"' {
            quoted = true;
        } else if byte == '{' {
            if opened { return false; }
            opened = true;
        } else if byte == '[' {
            return false;
        }
    }
    return opened;
}

fn ih_generate_json(fd: i64, body: const u8[:], output: u8[:], response: u8[:], ids: i64[:]) {
    if !ih_flat_object(body) {
        ih_response(fd, 400, "{\"error\":\"expected an object with scalar completion options\"}");
        return;
    }
    // The flat parser needs no more nodes, edges or decoded string bytes
    // than the input byte count. Reuse that bound for the request workspace.
    let n = body.len + 1;
    var table: i64[>..] = [];
    table.resize(14*n, 0);
    var doubles: f64[>..] = [];
    doubles.resize(n, 0.0);
    var strings: u8[>..] = [];
    strings.resize(n, 0);
    var pst: i64[16] = [0; 16];
    let root = json_parse_abi(body, table[0..n], table[n..2*n], doubles[..], table[2*n..3*n],
        table[3*n..4*n], table[4*n..5*n], table[5*n..6*n], table[6*n..7*n],
        table[7*n..8*n], table[8*n..9*n], table[9*n..10*n], table[10*n..11*n],
        strings[..], table[11*n..12*n], table[12*n..13*n], table[13*n..14*n], pst);
    if root < 0 || table[root] != PJ_K_OBJECT {
        ih_response(fd, 400, "{\"error\":\"expected a JSON object\"}");
        return;
    }
    let p = PNode { jk: table[0..n], jv: table[n..2*n], js: table[2*n..3*n], jl: table[3*n..4*n],
        jf: table[4*n..5*n], jn: table[5*n..6*n], jkey: table[11*n..12*n],
        jkl: table[12*n..13*n], jchild: table[13*n..14*n], sbuf: strings[..] };
    let off, len = pget_str(p, root, "prompt");
    var limit: i64 = 1;
    let first = p.jf[root];
    for i in p.jn[root] {
        let edge = first + i;
        let key = p.sbuf[p.jkey[edge] .. p.jkey[edge]+p.jkl[edge]];
        let child = p.jchild[edge];
        if rq_eq(key, "prompt") { continue; }
        if rq_eq(key, "n_predict") && (p.jk[child] == PJ_K_INT || p.jk[child] == PJ_K_UINT) {
            limit = p.jv[child];
            continue;
        }
        if rq_eq(key, "stream") && p.jk[child] == PJ_K_BOOL && p.jv[child] == 0 { continue; }
        if rq_eq(key, "temperature") &&
           ((p.jk[child] == PJ_K_INT && p.jv[child] == 0) || (p.jk[child] == 4 && doubles[child] == 0.0)) {
            continue;
        }
        ih_response(fd, 400, "{\"error\":\"unsupported completion option\"}");
        return;
    }
    if off < 0 || len <= 0 {
        ih_response(fd, 400, "{\"error\":\"expected a nonempty prompt string\"}");
        return;
    }
    ih_generate(fd, p.sbuf[off .. off + len], limit, output, response, ids);
}

fn ih_request(fd: i64, request: u8[:], output: u8[:], response: u8[:], ids: i64[:]) {
    var meta: i64[4] = [0; 4];
    let status = ih_read(fd, request, meta);
    if status == 404 {
        ih_response(fd, status, "{\"error\":\"route not found\"}");
    } else if status != 0 {
        ih_response(fd, status, "{\"error\":\"invalid request framing\"}");
    } else if meta[2] == 1 {
        ih_response(fd, 200, "{\"status\":\"ok\"}");
    } else if meta[2] == 3 {
        var stats: i64[5] = [0; 5];
        ih_stats_abi(stats);
        var n = cp_put(response, 0, "{\"n_vocab\":");
        n = cp_uint(response, n, stats[0]);
        n = cp_put(response, n, ",\"n_special\":");
        n = cp_uint(response, n, stats[1]);
        n = cp_put(response, n, ",\"n_layer\":");
        n = cp_uint(response, n, stats[4]);
        n = cp_put(response, n, "}");
        if n <= response.len { ih_response(fd, 200, response[0 .. n]); }
    } else {
        let body = request[meta[0] .. meta[0]+meta[1]];
        if meta[3] != 0 {
            ih_generate_json(fd, body, output, response, ids);
        } else {
            ih_generate(fd, body, 1, output, response, ids);
        }
    }
}

export fn ih_serve(port: i64, limit: i64, request: u8[:], output: u8[:], response: u8[:], ids: i64[:]) -> i64 {
    if port <= 0 || port > 65535 || limit < 0 { return -1; }
    var opts: i64[1] = [0];
    let listener = ht_open_impl(port, opts);
    if listener < 0 { return listener; }
    ih_listening(port);
    var served: i64 = 0;
    while limit == 0 || served < limit {
        var address: u8[16] = [0; 16];
        var length: i64[1] = [16];
        let fd = gx_accept(listener, address, length);
        if fd < 0 {
            gx_close(listener);
            return -1;
        }
        if ih_timeout(fd) == 0 { ih_request(fd, request, output, response, ids); }
        gx_close(fd);
        served += 1;
    }
    gx_close(listener);
    return served;
}
