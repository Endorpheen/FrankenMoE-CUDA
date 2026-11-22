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
extern "fg_http_windows_timing" fn ih_log_timing(prompt: i64, generated: i64, prefill: f64, decode: f64);

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
    if route != 1 && route != 3 && route != 5 && route != 11 && route != 12 && route != 13 && route != 101 { return 404; }
    if (route == 11 || route == 12 || route == 13) && length <= 0 { return 400; }
    if (route == 12 || route == 13) && json == 0 { return 400; }
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
    limit: i64, top: i64, return_tokens: bool, chat: bool, oai_text: bool, stream: bool, sampler: Q35Sample
}

fn ih_completion_defaults() -> IhCompletion {
    let s = q35_sample_defaults();
    return IhCompletion { limit: 1, top: 0, return_tokens: false, chat: false, oai_text: false, stream: false, sampler: s };
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

fn ih_generate(fd: i64, prompt: const i64[:], opts: const IhCompletion &,
               output: u8[:], response: u8[:], ids: i64[:]) {
    var stats: i64[6] = [0; 6];
    ih_stats_abi(stats);
    let limit = opts.limit;
    if prompt.len <= 0 || prompt.len > stats[5] || limit <= 0 || limit > ids.len ||
       limit - 1 > stats[5] - prompt.len || opts.top < 0 || !q35_sample_valid(opts.sampler) {
        ih_response(fd, 400, "{\"error\":\"invalid prompt length or completion options\"}");
        return;
    }
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
    let count = q35_sample_generate(prompt, stats[0], stats[5], eog[..], limit, opts.sampler, ids,
        timing, top, prob_ids[..], logp[..], pipeline);
    var reports_ready = true;
    if pipeline {
        let wait_start = q35_generation_now();
        reports_ready = prob_pipeline_finish(count, prob_ids[..], logp[..]);
        if count > 1 { timing[1] += q35_generation_now() - wait_start; }
    }
    if count <= 0 || count > limit || !reports_ready {
        ih_response(fd, 500, "{\"error\":\"generation failed\"}");
        return;
    }
    ih_log_timing(prompt.len, count, timing[0], timing[1]);
    let stopped_eos = bt_is_eog_impl(eog[..], eog.len, ids[count - 1]) != 0;
    let visible = if stopped_eos { count - 1 } else { count };
    let bytes = ih_detokenize(ids[0 .. visible], output);
    if bytes < 0 || bytes > output.len {
        ih_response(fd, 500, "{\"error\":\"detokenization failed\"}");
        return;
    }
    if opts.chat {
        let reason = if stopped_eos { "stop" } else { "length" };
        var n = cp_put(response, 0, "{\"id\":\"chatcmpl-local\",\"created\":0,\"model\":\"frankenmoe\",\"object\":");
        n = cp_put(response, n, if opts.stream { "\"chat.completion.chunk\"" } else { "\"chat.completion\"" });
        n = cp_put(response, n, if opts.stream { ",\"choices\":[{\"index\":0,\"delta\":" }
                                else { ",\"choices\":[{\"index\":0,\"message\":" });
        n = cp_put(response, n, "{\"role\":\"assistant\",\"content\":");
        n = esc_string(response, n, output, 0, bytes, 1);
        n = cp_put(response, n, "},\"finish_reason\":\"");
        n = cp_put(response, n, reason);
        n = cp_put(response, n, "\"}],\"usage\":{\"prompt_tokens\":");
        n = cp_uint(response, n, prompt.len);
        n = cp_put(response, n, ",\"completion_tokens\":");
        n = cp_uint(response, n, count);
        n = cp_put(response, n, ",\"total_tokens\":");
        n = cp_uint(response, n, prompt.len + count);
        n = cp_put(response, n, "}}");
        if n < 0 || n > response.len {
            ih_response(fd, 500, "{\"error\":\"response too large\"}");
            return;
        }
        if opts.stream {
            ih_log_response(200, "");
            ih_windows_sse(fd, response[0 .. n]);
        }
        else { ih_response(fd, 200, response[0 .. n]); }
        return;
    }
    if opts.oai_text {
        var n = cp_put(response, 0, "{\"id\":\"cmpl-local\",\"object\":\"text_completion\",\"created\":0,\"model\":\"frankenmoe\",\"choices\":[{\"index\":0,\"text\":");
        n = esc_string(response, n, output, 0, bytes, 1);
        n = cp_put(response, n, if stopped_eos { ",\"finish_reason\":\"stop\"}]}" } else { ",\"finish_reason\":\"length\"}]}" });
        if n < 0 || n > response.len {
            ih_response(fd, 500, "{\"error\":\"response too large\"}");
            return;
        }
        if opts.stream {
            ih_log_response(200, "");
            ih_windows_sse(fd, response[0 .. n]);
        } else {
            ih_response(fd, 200, response[0 .. n]);
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

fn ih_generate_text(fd: i64, prompt: const u8[:], opts: const IhCompletion &,
                    output: u8[:], response: u8[:], ids: i64[:]) {
    var tokens: i64[>..] = [];
    tokens.resize(prompt.len, 0);
    let count = ih_tokenize(prompt, tokens[..]);
    if count <= 0 || count > tokens.len {
        ih_response(fd, 400, "{\"error\":\"invalid prompt\"}");
        return;
    }
    ih_generate(fd, tokens[0 .. count], opts, output, response, ids);
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
    ih_generate(fd, tokens[..], opts, output, response, ids);
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

fn ih_generate_chat(fd: i64, p: const PNode, root: i64, opts: const IhCompletion &,
                    output: u8[:], response: u8[:], ids: i64[:]) {
    let messages = pget(p, root, "messages");
    if messages < 0 || p.jk[messages] != PJ_K_ARRAY || p.jn[messages] <= 0 {
        ih_response(fd, 400, "{\"error\":\"messages must be a nonempty array\"}");
        return;
    }
    var prompt: u8[>..] = [];
    prompt.resize(response.len, 0);
    var at: i64 = 0;
    for i in p.jn[messages] {
        let message = p.jchild[p.jf[messages] + i];
        if p.jk[message] != PJ_K_OBJECT {
            ih_response(fd, 400, "{\"error\":\"messages must contain objects\"}");
            return;
        }
        let role, role_len = pget_str(p, message, "role");
        let content = pget(p, message, "content");
        if role < 0 || content < 0 {
            ih_response(fd, 400, "{\"error\":\"each message requires a role and text content\"}");
            return;
        }
        let name = p.sbuf[role .. role + role_len];
        if !rq_eq(name, "system") && !rq_eq(name, "assistant") && !rq_eq(name, "user") {
            ih_response(fd, 400, "{\"error\":\"unsupported message role\"}");
            return;
        }
        at = cp_put(prompt[..], at, "<|im_start|>");
        at = cp_put(prompt[..], at, name);
        at = cp_put(prompt[..], at, "\n");
        if p.jk[content] == PJ_K_STRING {
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
        at = cp_put(prompt[..], at, "<|im_end|>\n");
        if at < 0 || at > prompt.len {
            ih_response(fd, 413, "{\"error\":\"chat prompt too large\"}");
            return;
        }
    }
    at = cp_put(prompt[..], at, "<|im_start|>assistant\n<think>");
    if at < 0 || at > prompt.len {
        ih_response(fd, 413, "{\"error\":\"chat prompt too large\"}");
        return;
    }
    ih_generate_text(fd, prompt[0 .. at], opts, output, response, ids);
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
    if chat { opts.chat = true; opts.limit = 256; }
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
            if rq_eq(key, "max_tokens") || rq_eq(key, "max_completion_tokens") {
                if p.jk[child] != PJ_K_INT && p.jk[child] != PJ_K_UINT {
                    ih_response(fd, 400, "{\"error\":\"invalid max_tokens\"}");
                    return;
                }
                opts.limit = p.jv[child];
                continue;
            }
            if rq_eq(key, "tools") { ih_log_ignored(1); continue; }
            if rq_eq(key, "tool_choice") { ih_log_ignored(2); continue; }
            if rq_eq(key, "response_format") { ih_log_ignored(3); continue; }
        }
        if !ih_option(key, p.jk[child], p.jv[child], doubles[child], opts) && !chat && !oai_text {
            ih_response(fd, 400, "{\"error\":\"unsupported completion option\"}");
            return;
        }
    }
    if chat {
        ih_generate_chat(fd, p, root, opts, output, response, ids);
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
    ih_generate_text(fd, p.sbuf[off .. off + len], opts, output, response, ids);
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
            ih_generate_text(fd, body, opts, output, response, ids);
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
