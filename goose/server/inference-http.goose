// Serial request handling for the standalone model host. CLI and HTTP share
// the same model state and Goose generation and sampling code.
extern "fg_http_tokenize" fn ih_tokenize(prompt: const u8[:], ids: i64[:]) -> i64;
extern "fg_http_detokenize" fn ih_detokenize(ids: const i64[:], text: u8[:]) -> i64;
extern "fg_http_eog" fn ih_eog(ids: i64[:]) -> i64;
extern "fg_http_stats" fn ih_stats_abi(stats: i64[:]);
extern "fg_http_listening" fn ih_listening(port: i64);
extern "fg_http_set_timeout" fn ih_timeout(fd: i64) -> i64;
extern "fg_http_windows_asset" fn ih_windows_asset(fd: i64, path: const u8[:]) -> bool;
extern "fg_http_windows_sse" fn ih_windows_sse(fd: i64, body: const u8[:]);
extern "fg_http_windows_request" fn ih_log_request(method: i64, route: i64);
extern "fg_http_windows_response" fn ih_log_response(status: i64, error_body: const u8[:]);
extern "fg_http_windows_ignored" fn ih_log_ignored(field: i64);
extern "fg_http_console" fn ih_console(line: const u8[:]);

export fn ih_log_loading(capacity: i64) {
    var line: u8[144] = [0; 144];
    var n = cp_put(line, 0, "🪿 [goose]  FrankenMoE | loading model | context ");
    n = cp_uint(line, n, capacity);
    n = cp_put(line, n, " tokens\n");
    ih_console(line[0 .. n]);
}

fn ih_log_ready(port: i64) {
    var stats: i64[24] = [0; 24];
    ih_stats_abi(stats[..]);
    if stats[5] <= 0 { return; }
    var line: u8[760] = [0; 760];
    var n = cp_put(line, 0, "    [model]  layers ");
    n = cp_uint(line, n, stats[4]);
    n = cp_put(line, n, " | vocabulary ");
    n = cp_uint(line, n, stats[0]);
    n = cp_put(line, n, " | resident ");
    n = cp_uint(line, n, stats[7] / 1048576);
    n = cp_put(line, n, " / ");
    n = cp_uint(line, n, stats[6] / 1048576);
    n = cp_put(line, n, " MiB\n🔥 [cuda]   dense ");
    n = cp_uint(line, n, stats[10] / 1048576);
    n = cp_put(line, n, " MiB | experts ");
    n = cp_uint(line, n, stats[11] / 1048576);
    n = cp_put(line, n, " MiB | GPU state ");
    n = cp_uint(line, n, stats[12] / 1048576);
    n = cp_put(line, n, " MiB | pinned experts ");
    n = cp_uint(line, n, stats[19] / 1048576);
    n = cp_put(line, n, " MiB\n    [slot]   context ");
    n = cp_uint(line, n, stats[5]);
    n = cp_put(line, n, " | batch/ubatch ");
    n = cp_uint(line, n, stats[13]);
    n = cp_put(line, n, "/");
    n = cp_uint(line, n, stats[14]);
    n = cp_put(line, n, " | threads decode/batch ");
    n = cp_uint(line, n, stats[23]);
    n = cp_put(line, n, "/");
    n = cp_uint(line, n, stats[22]);
    n = cp_put(line, n, " | KV q4_0/q4_0\n    [http]   ready at http://127.0.0.1:");
    n = cp_uint(line, n, port);
    n = cp_put(line, n, "/\n");
    ih_console(line[0 .. n]);
}

fn ih_log_generation(prompt: i64, generated: i64, prefill: f64, decode: f64) {
    var line: u8[420] = [0; 420];
    var n = cp_put(line, 0, "    [slot]   prefill ");
    n = cp_uint(line, n, prompt);
    n = cp_put(line, n, " tokens | ");
    n = cp_fixed(line, n, prefill * 1000.0, 2);
    n = cp_put(line, n, " ms | ");
    n = cp_fixed(line, n, if prefill > 0.0 { (prompt as f64) / prefill } else { 0.0 }, 2);
    n = cp_put(line, n, " tok/s\n    [slot]   decode  ");
    n = cp_uint(line, n, generated);
    n = cp_put(line, n, " tokens | ");
    n = cp_fixed(line, n, decode * 1000.0, 2);
    n = cp_put(line, n, " ms | ");
    n = cp_fixed(line, n, if generated > 1 && decode > 0.0 { ((generated - 1) as f64) / decode } else { 0.0 }, 2);
    n = cp_put(line, n, " tok/s\n");
    ih_console(line[0 .. n]);
}

fn ih_log_prefill_progress(processed: i64, total: i64, seconds: f64) {
    var line: u8[192] = [0; 192];
    var n = cp_put(line, 0, "🪿 [prefill] ");
    n = cp_uint(line, n, processed);
    n = cp_put(line, n, " / ");
    n = cp_uint(line, n, total);
    n = cp_put(line, n, " tokens | ");
    n = cp_fixed(line, n, if seconds > 0.0 { (processed as f64) / seconds } else { 0.0 }, 2);
    n = cp_put(line, n, " tok/s | ");
    n = cp_fixed(line, n, seconds, 2);
    n = cp_put(line, n, if processed == total { " s (done)\n" } else { " s\n" });
    ih_console(line[0 .. n]);
}

fn ih_log_context(positions: i64, capacity: i64) {
    var line: u8[160] = [0; 160];
    var n = cp_put(line, 0, "🧱 [kv]      context ");
    n = cp_uint(line, n, positions);
    n = cp_put(line, n, " / ");
    n = cp_uint(line, n, capacity);
    n = cp_put(line, n, " positions | prompt reused 0 tokens\n");
    ih_console(line[0 .. n]);
}

fn ih_log_decode_progress(generated: i64, limit: i64, speed: f64) {
    var line: u8[192] = [0; 192];
    var n = cp_put(line, 0, "🔥 [decode] ");
    n = cp_uint(line, n, generated);
    n = cp_put(line, n, " / ");
    n = cp_uint(line, n, limit);
    n = cp_put(line, n, " tokens | ");
    n = cp_fixed(line, n, speed, 2);
    n = cp_put(line, n, " tok/s (recent)\n");
    ih_console(line[0 .. n]);
}

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
    var parsed_route = route_id(method, target[0 .. path_len]);
    if method == 2 && parsed_route == 0 {
        let path_end = if path_len > 1 && target[path_len - 1] == '/' { path_len - 1 } else { path_len };
        let path = target[0 .. path_end];
        if rq_eq(path, "/v1/chat/completions") { parsed_route = 13; }
        if rq_eq(path, "/v1/completions") { parsed_route = 12; }
        if rq_eq(path, "/completion") || rq_eq(path, "/v1/completion") { parsed_route = 11; }
    }
    let route: i64 = if parsed_route == 0 && method == 1 { 101 } else { parsed_route };

    ih_log_request(method, route);
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
    let list_tools = route == 39 && method == 1;
    if route != 1 && route != 3 && route != 5 && route != 11 && route != 12 && route != 13 &&
       route != 36 && !list_tools && route != 101 { return 404; }
    if (route == 11 || route == 12 || route == 13 || route == 36) && length <= 0 { return 400; }
    if (route == 12 || route == 13 || route == 36) && json == 0 { return 400; }
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
    meta[4] = line[3];
    meta[5] = path_len;
    return 0;
}

fn ih_response(fd: i64, status: i64, body: const u8[:]) {
    ih_log_response(status, body);
    var head: u8[256] = [0; 256];
    let n = ht_response_head_impl(status, body.len, 0, head);
    if n > 0 && ht_write_all_impl(fd, head[0 .. n]) == 0 {
        ht_write_all_impl(fd, body);
    }
}

struct IhCompletion {
    limit: i64, top: i64, return_tokens: bool, chat: bool, oai_text: bool, stream: bool,
    live_timings: bool, prompt_progress: bool, call_id: i64, sampler: Q35Sample
}

fn ih_completion_defaults() -> IhCompletion {
    let s = q35_sample_defaults();
    return IhCompletion { limit: 1, top: 0, return_tokens: false, chat: false, oai_text: false, stream: false,
        live_timings: false, prompt_progress: false, call_id: 0, sampler: s };
}

fn ih_integer_array(response: u8[:], at: i64, ids: const i64[:]) -> i64 {
    var n = cp_put(response, at, "[");
    for i in ids.len {
        if i != 0 { n = cp_put(response, n, ","); }
        n = cp_uint(response, n, ids[i]);
    }
    return cp_put(response, n, "]");
}

fn ih_probability(response: u8[:], at: i64, token: i64, probability: f32, piece: u8[:],
                  eog: const i64[:]) -> i64 {
    let one: i64[1] = [token];
    let bytes = if bt_is_eog_impl(eog, eog.len, token) != 0 { 0 } else { ih_detokenize(one, piece) };
    if bytes < 0 || bytes > piece.len { return -1; }
    var prefix: i64 = 0;
    while prefix < bytes {
        let kind, size = utf8_len(piece, 0, prefix, bytes);
        if kind != 0 { break; }
        prefix += size;
    }
    var n = cp_put(response, at, "{\"id\":");
    n = cp_uint(response, n, token);
    n = cp_put(response, n, ",\"token\":");
    n = esc_string(response, n, piece, 0, prefix, 1);
    n = cp_put(response, n, ",\"bytes\":[");
    for i in bytes {
        if i != 0 { n = cp_put(response, n, ","); }
        n = cp_uint(response, n, piece[i] as i64);
    }
    n = cp_put(response, n, "],\"logprob\":");
    return fmt_double(response, n, probability as f64);
}

fn ih_probability_rows(response: u8[:], at: i64, steps: i64, top: i64,
                       ids: const i64[:], logp: const f32[:], piece: u8[:], eog: const i64[:]) -> i64 {
    var n = cp_put(response, at, ",\"completion_probabilities\":[");
    for row in steps {
        let base = row * (top + 1);
        if row != 0 { n = cp_put(response, n, ","); }
        n = ih_probability(response, n, ids[base], logp[base], piece, eog);
        if n < 0 || n > response.len { return -1; }
        n = cp_put(response, n, ",\"top_logprobs\":[");
        for col in top {
            if col != 0 { n = cp_put(response, n, ","); }
            let index = base + 1 + col;
            n = ih_probability(response, n, ids[index], logp[index], piece, eog);
            if n < 0 || n > response.len { return -1; }
            n = cp_put(response, n, "}");
        }
        n = cp_put(response, n, "]}");
    }
    return cp_put(response, n, "]");
}

fn ih_timing_fields(response: u8[:], at: i64, count: i64, timing: const f64[:]) -> i64 {
    let prompt = timing[2] as i64;
    let decode_steps = (count - 1) as f64;
    var n = cp_put(response, at, ",\"timings\":{\"cache_n\":0,\"prompt_n\":");
    n = cp_uint(response, n, prompt);
    n = cp_put(response, n, ",\"prompt_ms\":");
    n = fmt_double(response, n, timing[0] * 1000.0);
    n = cp_put(response, n, ",\"prompt_per_token_ms\":");
    n = fmt_double(response, n, timing[0] * 1000.0 / timing[2]);
    n = cp_put(response, n, ",\"prompt_per_second\":");
    n = fmt_double(response, n, if timing[0] > 0.0 { timing[2] / timing[0] } else { 0.0 });
    n = cp_put(response, n, ",\"predicted_n\":");
    n = cp_uint(response, n, count);
    n = cp_put(response, n, ",\"predicted_ms\":");
    n = fmt_double(response, n, timing[1] * 1000.0);
    n = cp_put(response, n, ",\"predicted_per_token_ms\":");
    n = fmt_double(response, n, if decode_steps > 0.0 { timing[1] * 1000.0 / decode_steps } else { 0.0 });
    n = cp_put(response, n, ",\"predicted_per_second\":");
    n = fmt_double(response, n, if timing[1] > 0.0 { decode_steps / timing[1] } else { 0.0 });
    return cp_put(response, n, "}");
}

fn ih_trim(text: const u8[:]) -> i64, i64 {
    var lo: i64 = 0;
    var hi = text.len;
    while lo < hi && (text[lo] == 32 || text[lo] == 9 || text[lo] == 10 || text[lo] == 13) { lo += 1; }
    while hi > lo && (text[hi - 1] == 32 || text[hi - 1] == 9 || text[hi - 1] == 10 || text[hi - 1] == 13) { hi -= 1; }
    return lo, hi;
}

fn ih_tool_identifier(text: const u8[:]) -> bool {
    if text.len <= 0 || text.len > 128 { return false; }
    for i in text.len {
        let c = text[i];
        if !(c >= 'a' && c <= 'z') && !(c >= 'A' && c <= 'Z') &&
           !(c >= '0' && c <= '9') && c != '_' && c != '-' && c != '.' { return false; }
    }
    return true;
}

fn ih_tool_allowed(catalog: const u8[:], name: const u8[:]) -> bool {
    var start: i64 = 0;
    for i in catalog.len {
        if catalog[i] == 10 {
            if rq_eq(catalog[start .. i], name) { return true; }
            start = i + 1;
        }
    }
    return false;
}

fn ih_tool_output(src: const u8[:], catalog: const u8[:], arguments: u8[:], out: u8[:],
                  at: i64, index: i64, stream: bool, call_id: i64, report_unknown: bool) -> i64 {
    let lo, hi = ih_trim(src);
    let tool = src[lo .. hi];
    if tool.len < 23 || !rq_eq(tool[0 .. 10], "<function=") { return -1; }
    let name_end = find_at(tool, ">");
    if name_end <= 10 { return -1; }
    let name = tool[10 .. name_end];
    if !ih_tool_identifier(name) || (!report_unknown && !ih_tool_allowed(catalog, name)) { return -1; }
    var pos = name_end + 1;
    var n = cp_put(arguments, 0, "{");
    var count: i64 = 0;
    while pos < tool.len {
        let left, right = ih_trim(tool[pos .. tool.len]);
        pos += left;
        if right - left == 11 && rq_eq(tool[pos .. tool.len], "</function>") { break; }
        if tool.len - pos < 12 || !rq_eq(tool[pos .. pos + 11], "<parameter=") { return -1; }
        let sep = find_at(tool[pos .. tool.len], ">");
        if sep <= 11 { return -1; }
        let key = tool[pos + 11 .. pos + sep];
        if !ih_tool_identifier(key) { return -1; }
        let start = pos + sep + 1;
        let end = find_at(tool[start .. tool.len], "</parameter>");
        if end < 0 { return -1; }
        var value_start = start;
        var value_end = start + end;
        if value_start < value_end && tool[value_start] == 10 { value_start += 1; }
        if value_start < value_end && tool[value_end - 1] == 10 { value_end -= 1; }
        if count > 0 { n = cp_put(arguments, n, ","); }
        n = esc_string(arguments, n, key, 0, key.len, 1);
        n = cp_put(arguments, n, ":");
        n = esc_string(arguments, n, tool, value_start, value_end - value_start, 1);
        count += 1;
        pos = start + end + 12;
    }
    if pos >= tool.len { return -1; }
    n = cp_put(arguments, n, "}");
    if n > arguments.len { return -1; }
    var end = cp_put(out, at, "{");
    if stream {
        end = cp_put(out, end, "\"index\":");
        end = cp_uint(out, end, index);
        end = cp_put(out, end, ",");
    }
    end = cp_put(out, end, "\"id\":\"call_local_");
    end = cp_uint(out, end, call_id);
    end = cp_put(out, end, "_");
    end = cp_uint(out, end, index);
    end = cp_put(out, end, "\",\"type\":\"function\",\"function\":{\"name\":");
    end = esc_string(out, end, name, 0, name.len, 1);
    end = cp_put(out, end, ",\"arguments\":");
    end = esc_string(out, end, arguments, 0, n, 1);
    return cp_put(out, end, "}}");
}

// A live chat response keeps its current byte offsets across decoder steps.
// Leave room for split markup and UTF-8 before publishing a partial result.
fn ih_apex_chat_bounds(output: const u8[:]) -> i64, i64, i64 {
    let initial, _ = ih_trim(output);
    var thought_start: i64 = 0;
    if output.len - initial >= 7 && rq_eq(output[initial .. initial + 7], "<think>") { thought_start = initial + 7; }
    let closing = find_at(output[thought_start .. output.len], "</think");
    if closing >= 0 {
        let tag = thought_start + closing;
        var end = tag + 7;
        while end < output.len && (output[end] == 32 || output[end] == 9) { end += 1; }
        if end < output.len && output[end] == '>' { return thought_start, tag, end + 1; }
    }
    return thought_start, -1, 0;
}

fn ih_chat_sse_start(fd: i64, stream: i64[:]) -> bool {
    if stream[3] != 0 { return stream[3] > 0; }
    let head = "HTTP/1.1 200 OK\r\nContent-Type: text/event-stream; charset=utf-8\r\nCache-Control: no-cache\r\nConnection: close\r\n\r\n";
    stream[3] = if ht_write_all_impl(fd, head) == 0 { 1 } else { -1 };
    return stream[3] > 0;
}

fn ih_chat_sse_data(fd: i64, body: const u8[:]) -> bool {
    return ht_write_all_impl(fd, "data: ") == 0 && ht_write_all_impl(fd, body) == 0 &&
           ht_write_all_impl(fd, "\n\n") == 0;
}

fn ih_chat_sse_progress_header(response: u8[:]) -> i64 {
    return cp_put(response, 0, "{\"id\":\"chatcmpl-local\",\"created\":0,\"model\":\"frankenmoe\",\"object\":\"chat.completion.chunk\",\"choices\":[{\"index\":0,\"delta\":{},\"finish_reason\":null}]");
}

fn ih_chat_send_progress(stream: i64[:], response: const u8[:], at: i64) {
    if at > response.len || !ih_chat_sse_start(stream[0], stream) { return; }
    if !ih_chat_sse_data(stream[0], response[0 .. at]) { stream[3] = -1; }
}

fn ih_publish_prefill(processed: i64, total: i64, seconds: f64, stream: i64[:], response: u8[:]) {
    ih_log_prefill_progress(processed, total, seconds);
    if stream[9] == 0 || stream[3] < 0 { return; }
    var n = ih_chat_sse_progress_header(response);
    n = cp_put(response, n, ",\"prompt_progress\":{\"total\":");
    n = cp_uint(response, n, total);
    n = cp_put(response, n, ",\"cache\":0,\"processed\":");
    n = cp_uint(response, n, processed);
    n = cp_put(response, n, ",\"time_ms\":");
    n = fmt_double(response, n, seconds * 1000.0);
    n = cp_put(response, n, "}}");
    ih_chat_send_progress(stream, response, n);
}

fn ih_chat_sse_delta(fd: i64, offset: i64, bound: i64, text: const u8[:],
                     field: const u8[:], stream: i64[:], index: i64, response: u8[:]) {
    let start = if stream[index] > offset { stream[index] } else { offset };
    var end = start;
    while end < bound {
        let kind, bytes = utf8_len(text, 0, end, bound);
        if kind != 0 { break; }
        end += bytes;
    }
    if end <= start { return; }
    var n = cp_put(response, 0, "{\"id\":\"chatcmpl-local\",\"created\":0,\"model\":\"frankenmoe\",\"object\":\"chat.completion.chunk\",\"choices\":[{\"index\":0,\"delta\":{");
    n = cp_put(response, n, field);
    n = esc_string(response, n, text, start, end - start, 1);
    n = cp_put(response, n, "},\"finish_reason\":null}]}");
    if n > response.len || !ih_chat_sse_start(fd, stream) { return; }
    if ih_chat_sse_data(fd, response[0 .. n]) { stream[index] = end; }
    else { stream[3] = -1; }
}

fn ih_chat_stream_observe(ids: const i64[:], prompt: i64, start_time: f64, prefill_done: f64,
                          stream: i64[:], output: u8[:], response: u8[:]) {
    let timestamp = q35_generation_now();
    let now = (timestamp * 1000.0) as i64;
    if stream[5] == 0 {
        stream[5] = now;
        stream[6] = ids.len;
    } else if now - stream[5] >= 3000 {
        ih_log_decode_progress(ids.len, stream[7], ((ids.len - stream[6]) as f64) * 1000.0 / ((now - stream[5]) as f64));
        stream[5] = now;
        stream[6] = ids.len;
    }
    if stream[4] == 0 || stream[3] < 0 { return; }
    if stream[8] != 0 {
        let timing: f64[3] = [prefill_done - start_time, timestamp - prefill_done, prompt as f64];
        var n = ih_chat_sse_progress_header(response);
        n = ih_timing_fields(response, n, ids.len, timing[..]);
        n = cp_put(response, n, "}");
        ih_chat_send_progress(stream, response, n);
        if stream[3] < 0 { return; }
    }
    let size = ih_detokenize(ids, output);
    if size <= 0 || size > output.len { return; }
    let text = output[0 .. size];
    let reason, closed, answer = ih_apex_chat_bounds(text);
    let fd = stream[0];
    let lo, _ = ih_trim(text[reason .. size]);
    let start = reason + lo;
    let bound = if closed >= 0 { closed } else { size - 24 };
    let early_tool = find_at(text[reason .. size], "<tool_call>");
    let safe_bound = if early_tool >= 0 && reason + early_tool < bound { reason + early_tool } else { bound };
    ih_chat_sse_delta(fd, start, safe_bound, text, "\"reasoning_content\":", stream, 1, response);
    if closed >= 0 && stream[3] >= 0 {
        let trim, _ = ih_trim(text[answer .. size]);
        let mark = find_at(text[answer .. size], "<tool_call>");
        let limit = if mark >= 0 { answer + mark } else { size - 24 };
        ih_chat_sse_delta(fd, answer + trim, limit, text, "\"content\":", stream, 2, response);
    }
}

fn ih_chat_output(fd: i64, output: const u8[:], response: u8[:], catalog: const u8[:], opts: const IhCompletion &,
                  done: bool, count: i64, prompt_count: i64, timing: const f64[:], stream: i64[:]) {
    let thought_start, closed, answer = ih_apex_chat_bounds(output);
    var thought_end = closed;
    var answer_start = answer;
    if thought_end < 0 && (thought_start > 0 || !done || stream[1] > 0) {
        let tool = find_at(output, "<tool_call>");
        thought_end = if tool >= thought_start { tool } else { output.len };
        answer_start = thought_end;
    }

    var n = cp_put(response, 0, "{\"id\":\"chatcmpl-local\",\"created\":0,\"model\":\"frankenmoe\",\"object\":");
    n = cp_put(response, n, if opts.stream { "\"chat.completion.chunk\"" } else { "\"chat.completion\"" });
    n = cp_put(response, n, if opts.stream { ",\"choices\":[{\"index\":0,\"delta\":" }
                            else { ",\"choices\":[{\"index\":0,\"message\":" });
    n = cp_put(response, n, "{\"role\":\"assistant\"");
    if thought_end > thought_start {
        let low, high = ih_trim(output[thought_start .. thought_end]);
        let sent = if stream[1] > thought_start + low { stream[1] } else { thought_start + low };
        if thought_start + high > sent {
            n = cp_put(response, n, ",\"reasoning_content\":");
            n = esc_string(response, n, output, sent, thought_start + high - sent, 1);
        }
    }

    let answer = output[answer_start .. output.len];
    let first = if catalog.len > 0 { find_at(answer, "<tool_call>") } else { -1 };
    var calls: i64 = 0;
    var tool_suffix = false;
    let fields = n;
    if first >= 0 {
        var arguments: u8[>..] = [];
        arguments.resize(response.len, 0);
        for pass in 2 {
            n = cp_put(response, fields, ",\"tool_calls\":[");
            var pos = first;
            while pos + 11 <= answer.len && rq_eq(answer[pos .. pos + 11], "<tool_call>") {
                let start = pos + 11;
                let end = find_at(answer[start .. answer.len], "</tool_call>");
                if end < 0 { break; }
                let next = ih_tool_output(answer[start .. start + end], catalog, arguments[..], response,
                    if calls == 0 { n } else { cp_put(response, n, ",") }, calls, opts.stream, opts.call_id, pass != 0);
                if next >= 0 {
                    n = next;
                    calls += 1;
                }
                pos = start + end + 12;
                let skip, _ = ih_trim(answer[pos .. answer.len]);
                pos += skip;
            }
            tool_suffix = pos == answer.len;
            if !tool_suffix { calls = 0; break; }
            if calls > 0 { break; }
        }
        n = if calls > 0 { cp_put(response, n, "]") } else { fields };
    }
    n = cp_put(response, n, ",\"content\":");
    let length = if tool_suffix { first } else { answer.len };
    let low, high = ih_trim(answer[0 .. length]);
    let sent = if stream[2] > answer_start + low { stream[2] - answer_start } else { low };
    if tool_suffix && calls == 0 && high <= sent {
        n = cp_put(response, n, "\"The requested tool call could not be executed.\"");
    }
    else if calls > 0 && high <= sent { n = cp_put(response, n, "null"); }
    else if high <= sent { n = cp_put(response, n, "\"\""); }
    else { n = esc_string(response, n, answer, sent, high - sent, 1); }
    n = cp_put(response, n, "},\"finish_reason\":\"");
    n = cp_put(response, n, if calls > 0 { "tool_calls" } else if done { "stop" } else { "length" });
    n = cp_put(response, n, "\"}],\"usage\":{\"prompt_tokens\":");
    n = cp_uint(response, n, prompt_count);
    n = cp_put(response, n, ",\"completion_tokens\":");
    n = cp_uint(response, n, count);
    n = cp_put(response, n, ",\"total_tokens\":");
    n = cp_uint(response, n, prompt_count + count);
    n = cp_put(response, n, "}");
    n = ih_timing_fields(response, n, count, timing);
    n = cp_put(response, n, "}");
    if n > response.len {
        if stream[3] == 0 { ih_response(fd, 500, "{\"error\":\"response too large\"}"); }
    } else if opts.stream {
        if ih_chat_sse_start(fd, stream) && ih_chat_sse_data(fd, response[0 .. n]) {
            ht_write_all_impl(fd, "data: [DONE]\n\n");
            ih_log_response(200, "");
        }
    } else { ih_response(fd, 200, response[0 .. n]); }
}

fn ih_generate(fd: i64, prompt: const i64[:], opts: const IhCompletion &, catalog: const u8[:],
               output: u8[:], response: u8[:], ids: i64[:]) {
    var stats: i64[6] = [0; 6];
    ih_stats_abi(stats);
    if prompt.len <= 0 || prompt.len > stats[5] {
        ih_response(fd, 400, "{\"error\":\"prompt exceeds the model context\"}");
        return;
    }
    if opts.limit <= 0 || opts.top < 0 || !q35_sample_valid(opts.sampler) {
        ih_response(fd, 400, "{\"error\":\"invalid completion options\"}");
        return;
    }
    let available = stats[5] - prompt.len + 1;
    var limit = opts.limit;
    if opts.chat || opts.oai_text {
        if limit > available { limit = available; }
        if limit > 65536 { limit = 65536; }
    } else if limit > ids.len || limit > available {
        ih_response(fd, 400, "{\"error\":\"n_predict exceeds the native completion limit or context\"}");
        return;
    }
    var wide_ids: i64[>..] = [];
    if limit > ids.len { wide_ids.resize(limit, 0); }
    let sampled = if wide_ids.len > 0 { wide_ids[..] } else { ids };
    var wide_text: u8[>..] = [];
    if limit > ids.len { wide_text.resize(limit * 128, 0); }
    let text = if wide_text.len > 0 { wide_text[..] } else { output };
    for i in prompt.len {
        if prompt[i] < 0 || prompt[i] >= stats[0] {
            ih_response(fd, 400, "{\"error\":\"invalid prompt token\"}");
            return;
        }
    }
    let top = if opts.top < stats[0] { opts.top } else { stats[0] };
    var prob_ids: i64[>..] = [];
    var logp: f32[>..] = [];
    if top > 0 {
        // Each probability record consumes more than one response byte.
        if limit > response.len / (top + 1) {
            ih_response(fd, 400, "{\"error\":\"probability output exceeds response capacity\"}");
            return;
        }
        prob_ids.resize(limit * (top + 1), 0);
        logp.resize(limit * (top + 1), 0.0);
    }
    var eog: i64[>..] = [];
    eog.resize(stats[2], 0);
    if ih_eog(eog[..]) != eog.len {
        ih_response(fd, 500, "{\"error\":\"generation failed\"}");
        return;
    }
    var timing: f64[3] = [0.0; 3];
    let pipeline = top > 0 && prob_pipeline_start(stats[0], top, limit);
    let chat_stream: i64 = if opts.chat && opts.stream { 1 } else { 0 };
    let live_timings: i64 = if chat_stream != 0 && opts.live_timings { 1 } else { 0 };
    let prompt_progress: i64 = if chat_stream != 0 && opts.prompt_progress { 1 } else { 0 };
    var stream: i64[10] = [fd, 0, 0, 0, chat_stream, 0, 0, limit, live_timings, prompt_progress];
    let count = q35_sample_generate(prompt, stats[0], stats[5], eog[..], limit, opts.sampler, sampled,
        timing, top, prob_ids[..], logp[..], pipeline, stream[..], text, response);
    var reports_ready = true;
    if pipeline {
        let wait_start = q35_generation_now();
        reports_ready = prob_pipeline_finish(count, prob_ids[..], logp[..]);
        if count > 1 { timing[1] += q35_generation_now() - wait_start; }
    }
    if count <= 0 || count > limit || !reports_ready {
        if stream[3] != 0 { return; }
        ih_response(fd, 500, "{\"error\":\"generation failed\"}");
        return;
    }
    ih_log_generation(prompt.len, count, timing[0], timing[1]);
    ih_log_context(prompt.len + count - 1, stats[5]);
    let stopped_eos = bt_is_eog_impl(eog[..], eog.len, sampled[count - 1]) != 0;
    let visible = if stopped_eos { count - 1 } else { count };
    let bytes = ih_detokenize(sampled[0 .. visible], text);
    if bytes < 0 || bytes > text.len {
        if stream[3] != 0 { return; }
        ih_response(fd, 500, "{\"error\":\"detokenization failed\"}");
        return;
    }
    if opts.chat {
        var wide_response: u8[>..] = [];
        if bytes * 8 + 4096 > response.len { wide_response.resize(bytes * 8 + 4096, 0); }
        let serialized = if wide_response.len > 0 { wide_response[..] } else { response };
        ih_chat_output(fd, text[0 .. bytes], serialized, catalog, opts, stopped_eos, count, prompt.len, timing, stream[..]);
        return;
    }
    if opts.oai_text {
        var wide_response: u8[>..] = [];
        if bytes * 6 + 512 > response.len { wide_response.resize(bytes * 6 + 512, 0); }
        let encoded = if wide_response.len > 0 { wide_response[..] } else { response };
        var n = cp_put(encoded, 0, "{\"id\":\"cmpl-local\",\"object\":\"text_completion\",\"created\":0,\"model\":\"frankenmoe\",\"choices\":[{\"index\":0,\"text\":");
        n = esc_string(encoded, n, text, 0, bytes, 1);
        n = cp_put(encoded, n, if stopped_eos { ",\"finish_reason\":\"stop\"}]}" } else { ",\"finish_reason\":\"length\"}]}" });
        if n < 0 || n > encoded.len {
            ih_response(fd, 500, "{\"error\":\"response too large\"}");
            return;
        }
        if opts.stream {
            ih_log_response(200, "");
            ih_windows_sse(fd, encoded[0 .. n]);
        } else {
            ih_response(fd, 200, encoded[0 .. n]);
        }
        return;
    }
    var n = cp_put(response, 0, "{\"ids\":");
    n = ih_integer_array(response, n, ids[0 .. count]);
    n = cp_put(response, n, ",\"text\":");
    n = esc_string(response, n, output, 0, bytes, 1);
    n = cp_put(response, n, ",\"count\":");
    n = cp_uint(response, n, count);
    if opts.return_tokens || top > 0 || opts.stream {
        n = cp_put(response, n, ",\"content\":");
        n = esc_string(response, n, output, 0, bytes, 1);
        if opts.return_tokens {
            n = cp_put(response, n, ",\"tokens\":");
            n = ih_integer_array(response, n, ids[0 .. count]);
        }
        n = cp_put(response, n, if stopped_eos { ",\"stop\":true,\"stop_type\":\"eos\"" }
                              else { ",\"stop\":true,\"stop_type\":\"limit\"" });
        n = cp_put(response, n, ",\"tokens_evaluated\":");
        n = cp_uint(response, n, prompt.len);
        n = cp_put(response, n, ",\"tokens_predicted\":");
        n = cp_uint(response, n, count);
        n = ih_timing_fields(response, n, count, timing);
    }
    if top > 0 {
        n = ih_probability_rows(response, n, count, top, prob_ids[..], logp[..], output, eog[..]);
    }
    if n < 0 || n >= response.len {
        ih_response(fd, 500, "{\"error\":\"response too large\"}");
        return;
    }
    n = cp_put(response, n, "}");
    if opts.stream {
        ih_log_response(200, "");
        ih_windows_sse(fd, response[0 .. n]);
    } else {
        ih_response(fd, 200, response[0 .. n]);
    }
}

fn ih_generate_text(fd: i64, prompt: const u8[:], opts: const IhCompletion &, catalog: const u8[:],
                    output: u8[:], response: u8[:], ids: i64[:]) {
    var tokens: i64[>..] = [];
    tokens.resize(prompt.len, 0);
    let count = ih_tokenize(prompt, tokens[..]);
    if count <= 0 || count > tokens.len {
        ih_response(fd, 400, "{\"error\":\"invalid prompt\"}");
        return;
    }
    ih_generate(fd, tokens[0 .. count], opts, catalog, output, response, ids);
}

// A completion object can have a flat token array in addition to scalars.
// Bound nesting before entering the shared recursive JSON reader.
fn ih_flat_object(body: const u8[:]) -> bool {
    var opened: bool = false;
    var array_open: bool = false;
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
            if !opened || array_open { return false; }
            array_open = true;
        } else if byte == ']' {
            if !array_open { return false; }
            array_open = false;
        }
    }
    return opened && !array_open;
}

fn ih_generate_ids(fd: i64, p: const PNode, prompt: i64, opts: const IhCompletion &,
                   output: u8[:], response: u8[:], ids: i64[:]) {
    let count = p.jn[prompt];
    var tokens: i64[>..] = [];
    tokens.resize(count, 0);
    let first = p.jf[prompt];
    for i in count {
        let token = p.jchild[first + i];
        if p.jk[token] != PJ_K_INT && p.jk[token] != PJ_K_UINT {
            ih_response(fd, 400, "{\"error\":\"invalid prompt token\"}");
            return;
        }
        tokens[i] = p.jv[token];
    }
    ih_generate(fd, tokens[..], opts, "", output, response, ids);
}

fn ih_option(key: const u8[:], kind: i64, integer: i64, number: f64, opts: IhCompletion &) -> bool {
    if rq_eq(key, "prompt") { return true; }
    if kind == PJ_K_INT || kind == PJ_K_UINT {
        if rq_eq(key, "n_predict") { opts.limit = integer; return true; }
        if rq_eq(key, "seed") { opts.sampler.seed = integer; return true; }
        if rq_eq(key, "top_k") { opts.sampler.top_k = integer; return true; }
        if rq_eq(key, "min_keep") { opts.sampler.min_keep = integer; return true; }
        if rq_eq(key, "repeat_last_n") { opts.sampler.repeat_last_n = integer; return true; }
        if rq_eq(key, "n_probs") { opts.top = integer; return true; }
    }
    if kind == PJ_K_BOOL {
        if rq_eq(key, "ignore_eos") { opts.sampler.ignore_eos = integer != 0; return true; }
        if rq_eq(key, "return_tokens") { opts.return_tokens = integer != 0; return true; }
        if rq_eq(key, "stream") { opts.stream = integer != 0; return true; }
        if rq_eq(key, "cache_prompt") { return integer == 0; }
    }
    if kind == PJ_K_INT || kind == PJ_K_UINT || kind == 4 {
        let val = if kind == 4 { number } else { integer as f64 };
        if !(val >= 0.0) || !(val <= 3.4028234663852886e38) { return false; }
        if rq_eq(key, "temperature") { opts.sampler.temperature = val as f32; return true; }
        if rq_eq(key, "repeat_penalty") { opts.sampler.repeat_penalty = val as f32; return true; }
        if val <= 1.0 {
            if rq_eq(key, "top_p") { opts.sampler.top_p = val as f32; return true; }
            if rq_eq(key, "min_p") { opts.sampler.min_p = val as f32; return true; }
            if rq_eq(key, "typical_p") { opts.sampler.typical_p = val as f32; return true; }
        }
    }
    return false;
}

fn ih_chat_depth_ok(body: const u8[:]) -> bool {
    var depth: i64 = 0;
    var quoted: bool = false;
    var escaped: bool = false;
    for i in body.len {
        let byte = body[i];
        if quoted {
            if escaped { escaped = false; }
            else if byte == 92 { escaped = true; }
            else if byte == 34 { quoted = false; }
        } else if byte == 34 {
            quoted = true;
        } else if byte == 123 || byte == 91 {
            depth += 1;
            if depth > 32 { return false; }
        } else if byte == 125 || byte == 93 {
            depth -= 1;
            if depth < 0 { return false; }
        }
    }
    return depth == 0 && !quoted;
}

fn ih_chat_json(p: const PNode, numbers: const f64[:], node: i64, out: u8[:], at: i64) -> i64 {
    return dump_node(node, 0, -1, p.jk, p.jv, numbers, p.js, p.jl, p.jf, p.jn,
                     p.jkey, p.jkl, p.jchild, p.sbuf, out, at, 1);
}

fn ih_apex_history_parameters(p: const PNode, numbers: const f64[:], node: i64, out: u8[:], at: i64) -> i64 {
    if p.jk[node] != PJ_K_OBJECT { return -1; }
    var n = at;
    for i in p.jn[node] {
        let edge = p.jf[node] + i;
        let name = p.sbuf[p.jkey[edge] .. p.jkey[edge] + p.jkl[edge]];
        if !ih_tool_identifier(name) { return -1; }
        let child = p.jchild[edge];
        n = cp_put(out, n, "<parameter=");
        n = cp_put(out, n, name);
        n = cp_put(out, n, ">\n");
        if p.jk[child] == PJ_K_STRING {
            n = cp_put(out, n, p.sbuf[p.js[child] .. p.js[child] + p.jl[child]]);
        } else {
            n = ih_chat_json(p, numbers, child, out, n);
        }
        n = cp_put(out, n, "\n</parameter>\n");
    }
    return n;
}

fn ih_apex_history_args(raw: const u8[:], out: u8[:], at: i64) -> i64 {
    if !ih_chat_depth_ok(raw) { return -1; }
    let n = raw.len + 1;
    var table: i64[>..] = [];
    table.resize(14*n, 0);
    var doubles: f64[>..] = [];
    doubles.resize(n, 0.0);
    var strings: u8[>..] = [];
    strings.resize(n, 0);
    var pst: i64[16] = [0; 16];
    let root = json_parse_abi(raw, table[0..n], table[n..2*n], doubles[..], table[2*n..3*n],
        table[3*n..4*n], table[4*n..5*n], table[5*n..6*n], table[6*n..7*n],
        table[7*n..8*n], table[8*n..9*n], table[9*n..10*n], table[10*n..11*n],
        strings[..], table[11*n..12*n], table[12*n..13*n], table[13*n..14*n], pst);
    if root < 0 { return -1; }
    let p = PNode { jk: table[0..n], jv: table[n..2*n], js: table[2*n..3*n], jl: table[3*n..4*n],
        jf: table[4*n..5*n], jn: table[5*n..6*n], jkey: table[11*n..12*n],
        jkl: table[12*n..13*n], jchild: table[13*n..14*n], sbuf: strings[..] };
    return ih_apex_history_parameters(p, doubles[..], root, out, at);
}

fn ih_apex_history_calls(p: const PNode, numbers: const f64[:], calls: i64, out: u8[:], at: i64) -> i64 {
    if p.jk[calls] != PJ_K_ARRAY { return -1; }
    var n = at;
    for i in p.jn[calls] {
        let call = p.jchild[p.jf[calls] + i];
        let function = pget(p, call, "function");
        if function < 0 || p.jk[function] != PJ_K_OBJECT { return -1; }
        let off, len = pget_str(p, function, "name");
        if off < 0 || !ih_tool_identifier(p.sbuf[off .. off + len]) { return -1; }
        let args = pget(p, function, "arguments");
        if args < 0 { return -1; }
        n = cp_put(out, n, "\n<tool_call>\n<function=");
        n = cp_put(out, n, p.sbuf[off .. off + len]);
        n = cp_put(out, n, ">\n");
        n = if p.jk[args] == PJ_K_STRING {
            ih_apex_history_args(p.sbuf[p.js[args] .. p.js[args] + p.jl[args]], out, n)
        } else { ih_apex_history_parameters(p, numbers, args, out, n) };
        if n < 0 || n > out.len { return -1; }
        n = cp_put(out, n, "</function>\n</tool_call>");
    }
    return n;
}

fn ih_generate_chat(fd: i64, p: const PNode, numbers: const f64[:], root: i64, opts: const IhCompletion &,
                    output: u8[:], response: u8[:], ids: i64[:]) {
    let messages = pget(p, root, "messages");
    if messages < 0 || p.jk[messages] != PJ_K_ARRAY || p.jn[messages] <= 0 {
        ih_response(fd, 400, "{\"error\":\"messages must be a nonempty array\"}");
        return;
    }
    var prompt: u8[>..] = [];
    prompt.resize(response.len, 0);
    var at: i64 = 0;
    var catalog: u8[>..] = [];
    catalog.resize(response.len, 0);
    var catalog_len: i64 = 0;
    let tools = pget(p, root, "tools");
    let choice_off, choice_len = pget_str(p, root, "tool_choice");
    let no_tools = choice_off >= 0 && rq_eq(p.sbuf[choice_off .. choice_off + choice_len], "none");
    var tools_header = false;
    if !no_tools && tools >= 0 && p.jk[tools] == PJ_K_ARRAY && p.jn[tools] > 0 {
        tools_header = true;
        at = cp_put(prompt[..], at, "<|im_start|>system\n# Tools\n\nYou have access to the following functions:\n\n<tools>");
        for tool in p.jn[tools] {
            let def = p.jchild[p.jf[tools] + tool];
            let fn_node = pget(p, def, "function");
            if fn_node < 0 {
                ih_response(fd, 400, "{\"error\":\"tools must contain function definitions\"}");
                return;
            }
            let name, len = pget_str(p, fn_node, "name");
            if name < 0 || !ih_tool_identifier(p.sbuf[name .. name + len]) {
                ih_response(fd, 400, "{\"error\":\"invalid tool name\"}");
                return;
            }
            catalog_len = cp_put(catalog[..], catalog_len, p.sbuf[name .. name + len]);
            catalog_len = cp_put(catalog[..], catalog_len, "\n");
            at = cp_put(prompt[..], at, "\n");
            at = ih_chat_json(p, numbers, def, prompt[..], at);
        }
        at = cp_put(prompt[..], at, "\n</tools>\n\nIf you choose to call a function ONLY reply in the following format with NO suffix:\n\n<tool_call>\n<function=example_function_name>\n<parameter=example_parameter>\nvalue\n</parameter>\n</function>\n</tool_call>\n\nUse only function names listed in <tools>. Include all required parameters. Reason before the call, never after. If a tool result reports an error, use it to correct your next call. If there is no function call available, answer normally.");
    }
    var tool_group = false;
    for i in p.jn[messages] {
        let message = p.jchild[p.jf[messages] + i];
        if p.jk[message] != PJ_K_OBJECT {
            ih_response(fd, 400, "{\"error\":\"messages must contain objects\"}");
            return;
        }
        let role, role_len = pget_str(p, message, "role");
        let content = pget(p, message, "content");
        if role < 0 {
            ih_response(fd, 400, "{\"error\":\"each message requires a role and text content\"}");
            return;
        }
        let name = p.sbuf[role .. role + role_len];
        let tool_role = rq_eq(name, "tool");
        let assistant = rq_eq(name, "assistant");
        let system = rq_eq(name, "system");
        if !system && !assistant && !tool_role && !rq_eq(name, "user") {
            ih_response(fd, 400, "{\"error\":\"unsupported message role\"}");
            return;
        }
        if !tool_role && tool_group {
            at = cp_put(prompt[..], at, "<|im_end|>\n");
            tool_group = false;
        }
        if tool_role {
            if !tool_group { at = cp_put(prompt[..], at, "<|im_start|>user"); }
            at = cp_put(prompt[..], at, "\n<tool_response>\n");
            tool_group = true;
        } else if i == 0 && system && tools_header {
            at = cp_put(prompt[..], at, "\n\n");
        } else {
            if i == 0 && tools_header { at = cp_put(prompt[..], at, "<|im_end|>\n"); }
            at = cp_put(prompt[..], at, "<|im_start|>");
            at = cp_put(prompt[..], at, name);
            at = cp_put(prompt[..], at, "\n");
        }
        if content < 0 || p.jk[content] == 0 {
            if !assistant {
                ih_response(fd, 400, "{\"error\":\"message requires content\"}");
                return;
            }
        } else if p.jk[content] == PJ_K_STRING {
            let off = p.js[content];
            at = cp_put(prompt[..], at, p.sbuf[off .. off + p.jl[content]]);
        } else if p.jk[content] == PJ_K_ARRAY {
            for part in p.jn[content] {
                let child = p.jchild[p.jf[content] + part];
                if p.jk[child] != PJ_K_OBJECT {
                    ih_response(fd, 400, "{\"error\":\"content arrays support only text parts\"}");
                    return;
                }
                let kind, kind_len = pget_str(p, child, "type");
                let text, len = pget_str(p, child, "text");
                if kind < 0 || text < 0 || !rq_eq(p.sbuf[kind .. kind + kind_len], "text") {
                    ih_response(fd, 400, "{\"error\":\"content arrays support only text parts\"}");
                    return;
                }
                at = cp_put(prompt[..], at, p.sbuf[text .. text + len]);
                if at < 0 || at > prompt.len {
                    ih_response(fd, 413, "{\"error\":\"chat prompt too large\"}");
                    return;
                }
            }
        } else {
            ih_response(fd, 400, "{\"error\":\"content must be a string or an array of text parts\"}");
            return;
        }
        if assistant {
            let calls = pget(p, message, "tool_calls");
            if calls >= 0 && p.jk[calls] != 0 {
                at = ih_apex_history_calls(p, numbers, calls, prompt[..], at);
                if at < 0 {
                    ih_response(fd, 400, "{\"error\":\"invalid assistant tool_calls history\"}");
                    return;
                }
            }
        }
        at = cp_put(prompt[..], at, if tool_role { "\n</tool_response>" } else { "<|im_end|>\n" });
        if at < 0 || at > prompt.len {
            ih_response(fd, 413, "{\"error\":\"chat prompt too large\"}");
            return;
        }
    }
    if tool_group { at = cp_put(prompt[..], at, "<|im_end|>\n"); }
    at = cp_put(prompt[..], at, "<|im_start|>assistant\n<think>");
    if at < 0 || at > prompt.len {
        ih_response(fd, 413, "{\"error\":\"chat prompt too large\"}");
        return;
    }
    if catalog_len > catalog.len {
        ih_response(fd, 413, "{\"error\":\"tool catalog too large\"}");
        return;
    }
    ih_generate_text(fd, prompt[0 .. at], opts, catalog[0 .. catalog_len], output, response, ids);
}

fn ih_generate_json(fd: i64, raw: const u8[:], output: u8[:], response: u8[:], ids: i64[:], route: i64) {
    let bom = raw.len >= 3 && raw[0] == 239 && raw[1] == 187 && raw[2] == 191;
    let body = raw[(if bom { 3 } else { 0 }) .. raw.len];
    let chat = route == 13;
    let oai_text = route == 12;
    if !ih_chat_depth_ok(body) {
        ih_response(fd, 400, "{\"error\":\"invalid JSON nesting\"}");
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
    if !chat && pget(p, root, "messages") >= 0 {
        ih_response(fd, 400, "{\"error\":\"This endpoint expects prompt; send messages to /v1/chat/completions\"}");
        return;
    }
    if !chat && !oai_text && !ih_flat_object(body) {
        ih_response(fd, 400, "{\"error\":\"completion expects a prompt and scalar options\"}");
        return;
    }
    let prompt = pget(p, root, "prompt");
    var opts = ih_completion_defaults();
    if chat { opts.chat = true; opts.limit = 8192; opts.call_id = (q35_generation_now() * 1000000.0) as i64; }
    opts.oai_text = oai_text;
    let first = p.jf[root];
    for i in p.jn[root] {
        let edge = first + i;
        let key = p.sbuf[p.jkey[edge] .. p.jkey[edge]+p.jkl[edge]];
        let child = p.jchild[edge];
        if chat || oai_text {
            if (chat && rq_eq(key, "messages")) || rq_eq(key, "model") || rq_eq(key, "stream_options") { continue; }
            if rq_eq(key, "stream") && p.jk[child] == PJ_K_BOOL {
                opts.stream = p.jv[child] != 0;
                continue;
            }
            if chat && p.jk[child] == PJ_K_BOOL {
                if rq_eq(key, "timings_per_token") { opts.live_timings = p.jv[child] != 0; continue; }
                if rq_eq(key, "return_progress") { opts.prompt_progress = p.jv[child] != 0; continue; }
            }
            if rq_eq(key, "max_tokens") || rq_eq(key, "max_completion_tokens") {
                if p.jk[child] != PJ_K_INT && p.jk[child] != PJ_K_UINT {
                    ih_response(fd, 400, "{\"error\":\"invalid max_tokens\"}");
                    return;
                }
                opts.limit = p.jv[child];
                continue;
            }
            if rq_eq(key, "tools") { continue; }
            if rq_eq(key, "tool_choice") { continue; }
            if rq_eq(key, "response_format") { ih_log_ignored(3); continue; }
        }
        if !ih_option(key, p.jk[child], p.jv[child], doubles[child], opts) && !chat && !oai_text {
            ih_response(fd, 400, "{\"error\":\"unsupported completion option\"}");
            return;
        }
    }
    if chat {
        ih_generate_chat(fd, p, doubles[..], root, opts, output, response, ids);
        return;
    }
    if prompt >= 0 && p.jk[prompt] == PJ_K_ARRAY {
        ih_generate_ids(fd, p, prompt, opts, output, response, ids);
        return;
    }
    let off, len = pget_str(p, root, "prompt");
    if off < 0 || len <= 0 {
        ih_response(fd, 400, "{\"error\":\"expected a prompt string or a token array\"}");
        return;
    }
    ih_generate_text(fd, p.sbuf[off .. off + len], opts, "", output, response, ids);
}

fn ih_request(fd: i64, request: u8[:], output: u8[:], response: u8[:], ids: i64[:]) {
    var meta: i64[6] = [0; 6];
    ih_log_request(0, 0);
    let status = ih_read(fd, request, meta);
    if status == 404 {
        ih_response(fd, status, "{\"error\":\"route not found\"}");
    } else if status != 0 {
        ih_response(fd, status, "{\"error\":\"invalid request framing\"}");
    } else if meta[2] == 1 {
        ih_response(fd, 200, "{\"status\":\"ok\"}");
    } else if meta[2] == 36 || meta[2] == 39 {
        // Standalone has no resumable sessions or server-side tool catalog.
        ih_response(fd, 200, "[]");
    } else if meta[2] == 101 {
        if !ih_windows_asset(fd, request[meta[4] .. meta[4] + meta[5]]) {
            ih_response(fd, 404, "{\"error\":\"file not found\"}");
        } else { ih_log_response(200, ""); }
    } else if meta[2] == 5 {
        ih_response(fd, 200, "{\"object\":\"list\",\"data\":[{\"id\":\"frankenmoe\",\"object\":\"model\",\"created\":0,\"owned_by\":\"local\"}]}");
    } else if meta[2] == 3 {
        var stats: i64[24] = [0; 24];
        ih_stats_abi(stats);
        if stats[6] < 0 {
            ih_response(fd, 500, "{\"error\":\"model residency unavailable\"}");
            return;
        }
        var n = cp_put(response, 0, "{\"n_vocab\":");
        n = cp_uint(response, n, stats[0]);
        n = cp_put(response, n, ",\"n_special\":");
        n = cp_uint(response, n, stats[1]);
        n = cp_put(response, n, ",\"n_layer\":");
        n = cp_uint(response, n, stats[4]);
        n = cp_put(response, n, ",\"n_ctx\":");
        n = cp_uint(response, n, stats[5]);
        n = cp_put(response, n, ",\"model_bytes\":");
        n = cp_uint(response, n, stats[6]);
        n = cp_put(response, n, ",\"model_resident_bytes\":");
        n = cp_uint(response, n, stats[7]);
        n = cp_put(response, n, ",\"model_pages\":");
        n = cp_uint(response, n, stats[8]);
        n = cp_put(response, n, ",\"model_resident_pages\":");
        n = cp_uint(response, n, stats[9]);
        n = cp_put(response, n, ",\"cuda_dense_weight_bytes\":");
        n = cp_uint(response, n, stats[10]);
        n = cp_put(response, n, ",\"cuda_expert_cache_bytes\":");
        n = cp_uint(response, n, stats[11]);
        n = cp_put(response, n, ",\"cuda_expert_cache_limit_bytes\":");
        n = cp_uint(response, n, stats[21]);
        n = cp_put(response, n, ",\"cache_type_k\":\"q4_0\",\"cache_type_v\":\"q4_0\"");
        n = cp_put(response, n, ",\"cuda_model_state_bytes\":");
        n = cp_uint(response, n, stats[12]);
        n = cp_put(response, n, ",\"n_batch\":");
        n = cp_uint(response, n, stats[13]);
        n = cp_put(response, n, ",\"n_ubatch\":");
        n = cp_uint(response, n, stats[14]);
        n = cp_put(response, n, ",\"n_threads_batch\":");
        n = cp_uint(response, n, stats[22]);
        n = cp_put(response, n, ",\"n_threads\":");
        n = cp_uint(response, n, stats[23]);
        n = cp_put(response, n, ",\"cuda_expert_hits\":");
        n = cp_uint(response, n, stats[15]);
        n = cp_put(response, n, ",\"cuda_expert_fills\":");
        n = cp_uint(response, n, stats[16]);
        n = cp_put(response, n, ",\"cuda_expert_evictions\":");
        n = cp_uint(response, n, stats[17]);
        n = cp_put(response, n, ",\"cuda_expert_upload_bytes\":");
        n = cp_uint(response, n, stats[18]);
        n = cp_put(response, n, ",\"cuda_pinned_expert_bytes\":");
        n = cp_uint(response, n, stats[19]);
        n = cp_put(response, n, ",\"cuda_pinned_expert_ranges\":");
        n = cp_uint(response, n, stats[20]);
        n = cp_put(response, n, "}");
        if n <= response.len { ih_response(fd, 200, response[0 .. n]); }
    } else {
        let body = request[meta[0] .. meta[0]+meta[1]];
        if meta[3] != 0 {
            ih_generate_json(fd, body, output, response, ids, meta[2]);
        } else {
            let opts = ih_completion_defaults();
            ih_generate_text(fd, body, opts, "", output, response, ids);
        }
    }
}

export fn ih_serve(port: i64, limit: i64, request: u8[:], output: u8[:], response: u8[:], ids: i64[:]) -> i64 {
    if port <= 0 || port > 65535 || limit < 0 { return -1; }
    var opts: i64[1] = [0];
    let listener = ht_open_impl(port, opts);
    if listener < 0 { return listener; }
    ih_listening(port);
    ih_log_ready(port);
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
