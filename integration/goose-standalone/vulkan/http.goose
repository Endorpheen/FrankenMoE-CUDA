extern "fg_vk_http_tokenize" fn vkh_tokenize(context: u64, text: const u8[:], ids: i64[:], special: bool) -> i64;
extern "fg_vk_http_generate" fn vkh_generate(context: u64, prompt: const i64[:], limit: i64,
    ignore_eog: bool, ids: i64[:], text: u8[:], reasoning: u8[:], info: i64[:]) -> i64;
extern "fg_vk_http_inkling_control" fn vkh_inkling_control(context: u64, id: i64) -> i64;
extern "fg_vk_http_prefill" fn vkh_prefill(context: u64, prompt: const i64[:], limit: i64) -> i64;
extern "fg_vk_http_next" fn vkh_next(context: u64, ignore_eog: bool, text: u8[:], id: i64[:]) -> i64;
extern "fg_vk_http_now" fn vkh_now() -> i64;
extern "fg_vk_http_timeout" fn vkh_timeout(fd: i64) -> i64;
extern "fg_vk_http_ready" fn vkh_ready(port: i64);

fn ih_log_request(method: i64, route: i64) {}
fn ih_log_response(status: i64, body: const u8[:]) {}

fn vkh_name(model_kind: i64) -> const u8[:] {
    if model_kind == 3 { return "inkling-2-mini"; }
    return if model_kind == 2 { "apex-35b" } else { if model_kind == 1 { "bonsai-2.5-27b" } else { "qwen2.5-0.5b-instruct" } };
}

fn vkh_error(fd: i64, status: i64, message: const u8[:]) {
    var out: u8[512] = [0; 512];
    var n = cp_put(out, 0, "{\"error\":{\"message\":\"");
    n = cp_put(out, n, message);
    n = cp_put(out, n, "\"}}");
    ih_response(fd, status, out[0 .. n]);
}

fn vkh_json_text(out: u8[:], start: i64, text: const u8[:]) -> i64 {
    var n = cp_chr(out, start, 34);
    var i: i64 = 0;
    var advance: i64[1] = [1];
    let hex: const u8[:] = "0123456789abcdef";
    while i < text.len {
        let c = text[i];
        if c == 34 || c == 92 {
            n = cp_chr(out, n, 92);
            n = cp_chr(out, n, c);
            i += 1;
        } else if c < 32 {
            n = cp_put(out, n, "\\u00");
            n = cp_chr(out, n, hex[(c >> 4) as i64]);
            n = cp_chr(out, n, hex[(c & 15) as i64]);
            i += 1;
        } else if c < 128 {
            n = cp_chr(out, n, c);
            i += 1;
        } else {
            let code = uc_cpt_from(text, i, advance);
            if code < 0 { n = cp_put(out, n, "\\ufffd"); }
            else { n = cp_put(out, n, text[i .. i + advance[0]]); }
            i += advance[0];
        }
    }
    return cp_chr(out, n, 34);
}

fn vkh_json_ids(out: u8[:], start: i64, ids: const i64[:]) -> i64 {
    var n = cp_chr(out, start, 91);
    for i in ids.len {
        if i != 0 { n = cp_chr(out, n, 44); }
        n = cp_uint(out, n, ids[i]);
    }
    return cp_chr(out, n, 93);
}

fn vkh_chat_frame(context: u64, ids: i64[:], at: i64, ctrl: const i64[:],
    role: const u8[:], message: const u8[:], closed: bool, scratch: u8[:]) -> i64 {
    let tail: i64 = if closed { 2 } else { 0 };
    if at < 0 || ids.len - at <= 1 + tail || role.len + 1 + message.len > scratch.len { return -1; }
    var n = cp_put(scratch, 0, role);
    n = cp_chr(scratch, n, 10);
    n = cp_put(scratch, n, message);
    ids[at] = ctrl[0];
    let count = vkh_tokenize(context, scratch[0 .. n], ids[at + 1 .. ids.len - tail], false);
    if count <= 0 { return -1; }
    let end = at + 1 + count;
    if closed {
        ids[end] = ctrl[1];
        ids[end + 1] = ctrl[2];
    }
    return end + tail;
}

fn vkh_trim(text: const u8[:]) -> const u8[:] {
    var advance: i64[1] = [1];
    var start: i64 = 0;
    var end: i64 = 0;
    var at: i64 = 0;
    while at < text.len {
        let cpt = uc_cpt_from(text, at, advance);
        if cpt < 0 || (uf_flags(cpt) & 256) == 0 { end = at + advance[0]; }
        else if end == 0 { start = at + advance[0]; }
        at += advance[0];
    }
    return if end > start { text[start .. end] } else { "" };
}

fn vkh_bonsai_assistant(context: u64, prompt: i64[:], at: i64, ctrl: const i64[:],
    text: const u8[:], closed: bool) -> i64 {
    let tail: i64 = if closed { 2 } else { 0 };
    if at < 0 || prompt.len - at <= 1 + tail { return -1; }
    prompt[at] = ctrl[0];
    let begin = at + 1;
    let bound = prompt.len - tail;
    let prefix = vkh_tokenize(context, "assistant\n<think>\n\n</think>\n\n", prompt[begin .. bound], true);
    if prefix <= 0 { return -1; }
    var end = begin + prefix;
    if text.len > 0 {
        let count = vkh_tokenize(context, text, prompt[end .. bound], false);
        if count <= 0 { return -1; }
        end += count;
    }
    if closed {
        prompt[end] = ctrl[1];
        prompt[end + 1] = ctrl[2];
    }
    return end + tail;
}

fn vkh_inkling_append(context: u64, ids: i64[:], at: i64, text: const u8[:], controls: bool) -> i64 {
    if at < 0 || at > ids.len { return -1; }
    if text.len == 0 { return at; }
    let size = vkh_tokenize(context, text, ids[at .. ids.len], controls);
    return if size > 0 { at + size } else { -1 };
}

fn vkh_inkling_effort(context: u64, ids: i64[:], at: i64) -> i64 {
    return vkh_inkling_append(context, ids, at,
        "<|message_system|><|content_text|>Thinking effort level: 0<|end_message|>", true);
}

fn vkh_inkling_message(context: u64, ids: i64[:], at: i64, role: const u8[:], text: const u8[:]) -> i64 {
    let prefix = if role == "user" { "<|message_user|><|content_text|>" }
        else if role == "assistant" { "<|message_model|><|content_text|>" }
        else { "<|message_system|><|content_text|>" };
    let tail = if role == "assistant" { "<|end_message|><|content_model_end_sampling|>" }
        else { "<|end_message|>" };
    let start = vkh_inkling_append(context, ids, at, prefix, true);
    let end = vkh_inkling_append(context, ids, start, text, false);
    return vkh_inkling_append(context, ids, end, tail, true);
}

fn vkh_chat_prompt(context: u64, array: i64, tree: const i64[:], size: i64,
    strings: const u8[:], prompt: i64[:], scratch: u8[:], model_kind: i64) -> i64 {
    if tree[array] != PJ_K_ARRAY || tree[5 * size + array] <= 0 { return -1; }
    let inkling = model_kind == 3;
    let bonsai = model_kind == 1 || model_kind == 2;
    var control: i64[3] = [0; 3];
    if !inkling && (vkh_tokenize(context, "<|im_start|>", control[0 .. 1], true) != 1 ||
       vkh_tokenize(context, "<|im_end|>", control[1 .. 2], true) != 1 ||
       vkh_tokenize(context, "\n", control[2 .. 3], false) != 1) { return -1; }
    let first = tree[4 * size + array];
    var at: i64 = 0;
    var user = false;
    var effort = false;
    for i in tree[5 * size + array] {
        let message = tree[13 * size + first + i];
        if tree[message] != PJ_K_OBJECT || tree[5 * size + message] != 2 { return -1; }
        var role: const u8[:] = "";
        var text: const u8[:] = "";
        let start = tree[4 * size + message];
        for j in 2 {
            let edge = start + j;
            let ko = tree[11 * size + edge];
            let key = strings[ko .. ko + tree[12 * size + edge]];
            let value = tree[13 * size + edge];
            if tree[value] != PJ_K_STRING { return -1; }
            let off = tree[2 * size + value];
            let bytes = strings[off .. off + tree[3 * size + value]];
            if key == "role" { role = bytes; }
            else if key == "content" { text = bytes; }
            else { return -1; }
        }
        if role != "system" && role != "user" && role != "assistant" { return -1; }
        if inkling {
            if !effort && role != "system" {
                at = vkh_inkling_effort(context, prompt, at);
                effort = true;
            }
            at = vkh_inkling_message(context, prompt, at, role, text);
            if at < 0 { return -1; }
            continue;
        }
        if !bonsai && i == 0 && role != "system" {
            at = vkh_chat_frame(context, prompt, at, control, "system",
                "You are Qwen, created by Alibaba Cloud. You are a helpful assistant.", true, scratch);
            if at < 0 { return -1; }
        }
        if bonsai {
            text = vkh_trim(text);
            if role == "system" && i != 0 { return -1; }
            if role == "system" && text.len == 0 { continue; }
            if role == "user" { user = true; }
        }
        at = if bonsai && role == "assistant" {
            vkh_bonsai_assistant(context, prompt, at, control, text, true)
        } else {
            vkh_chat_frame(context, prompt, at, control, role, text, true, scratch)
        };
        if at < 0 { return -1; }
    }
    if inkling { return if effort { at } else { vkh_inkling_effort(context, prompt, at) }; }
    if bonsai {
        if !user { return -1; }
        return vkh_bonsai_assistant(context, prompt, at, control, "", false);
    }
    return vkh_chat_frame(context, prompt, at, control, "assistant", "", false, scratch);
}

fn vkh_identity(out: u8[:], start: i64, route: i64, serial: i64, created: i64, streaming: bool, model: const u8[:]) -> i64 {
    var n = cp_put(out, start, "{\"id\":");
    n = cp_put(out, n, if route == 13 { "\"chatcmpl-vulkan-" } else { "\"cmpl-vulkan-" });
    n = cp_uint(out, n, serial);
    n = cp_put(out, n, "\",\"object\":");
    let kind: const u8[:] = if route == 13 {
        if streaming { "chat.completion.chunk" } else { "chat.completion" }
    } else { "text_completion" };
    n = vkh_json_text(out, n, kind);
    n = cp_put(out, n, ",\"created\":");
    n = cp_uint(out, n, created);
    n = cp_put(out, n, ",\"model\":");
    return vkh_json_text(out, n, model);
}

fn vkh_usage(out: u8[:], start: i64, prompt: i64, generated: i64) -> i64 {
    var n = cp_put(out, start, ",\"usage\":{\"prompt_tokens\":");
    n = cp_uint(out, n, prompt);
    n = cp_put(out, n, ",\"completion_tokens\":");
    n = cp_uint(out, n, generated);
    n = cp_put(out, n, ",\"total_tokens\":");
    n = cp_uint(out, n, prompt + generated);
    return cp_chr(out, n, 125);
}

fn vkh_stream_chunk(fd: i64, out: u8[:], route: i64, serial: i64, created: i64,
    text: const u8[:], role: bool, reasoning: bool, finish: const u8[:], model: const u8[:]) -> bool {
    var n = cp_put(out, 0, "data: ");
    n = vkh_identity(out, n, route, serial, created, true, model);
    n = cp_put(out, n, ",\"choices\":[{\"index\":0,");
    if route == 13 {
        n = cp_put(out, n, "\"delta\":{");
        if role {
            n = cp_put(out, n, "\"role\":\"assistant\"");
        } else if text.len > 0 {
            n = cp_put(out, n, if reasoning { "\"reasoning_content\":" } else { "\"content\":" });
            n = vkh_json_text(out, n, text);
        }
        n = cp_chr(out, n, 125);
    } else {
        n = cp_put(out, n, "\"text\":");
        n = vkh_json_text(out, n, text);
    }
    n = cp_put(out, n, ",\"finish_reason\":");
    n = if finish.len == 0 { cp_put(out, n, "null") } else { vkh_json_text(out, n, finish) };
    n = cp_put(out, n, "}]}\n\n");
    return n <= out.len && ht_write_all_impl(fd, out[0 .. n]) == 0;
}

// A token may end after only the first bytes of a UTF-8 codepoint.
fn vkh_complete_utf8(text: const u8[:]) -> i64 {
    if text.len == 0 { return 0; }
    var at = text.len - 1;
    while at > 0 && text.len - at < 4 && (text[at] & 192) == 128 { at -= 1; }
    let lead = text[at];
    let width: i64 = if lead >= 194 && lead <= 223 { 2 }
        else if lead >= 224 && lead <= 239 { 3 }
        else if lead >= 240 && lead <= 244 { 4 } else { 1 };
    return if text.len - at < width { at } else { text.len };
}

fn vkh_stream(context: u64, fd: i64, route: i64, serial: i64, created: i64,
    prompt: const i64[:], limit: i64, ignore_eog: bool, report_usage: bool, inkling: bool, out: u8[:], model: const u8[:]) {
    if vkh_prefill(context, prompt, limit) != 0 {
        vkh_error(fd, 500, "Vulkan prefill failed");
        return;
    }
    let header = "HTTP/1.1 200 OK\r\nContent-Type: text/event-stream; charset=utf-8\r\nCache-Control: no-cache\r\nConnection: close\r\n\r\n";
    if ht_write_all_impl(fd, header) != 0 { return; }
    if route == 13 && !vkh_stream_chunk(fd, out, route, serial, created, "", true, false, "", model) { return; }
    var piece: u8[1028] = [0; 1028];
    var id: i64[1] = [0];
    var pending: i64 = 0;
    var count: i64 = 0;
    var thinking = false;
    var finish: const u8[:] = "length";
    while count < limit {
        let got = vkh_next(context, ignore_eog, piece[pending .. piece.len], id);
        if got == -2 { finish = "stop"; break; }
        if got < 0 || got > piece.len - pending {
            ht_write_all_impl(fd, "data: {\"error\":{\"message\":\"Vulkan decode failed\"}}\n\n");
            ht_write_all_impl(fd, "data: [DONE]\n\n");
            return;
        }
        count += 1;
        let control = if inkling { vkh_inkling_control(context, id[0]) } else { 0 };
        if control != 0 {
            if pending > 0 && !vkh_stream_chunk(fd, out, route, serial, created, piece[0 .. pending],
                false, thinking, "", model) { return; }
            pending = 0;
            if control != 1 { thinking = control == 3; }
            continue;
        }
        pending += got;
        let ready = vkh_complete_utf8(piece[0 .. pending]);
        if ready > 0 {
            if !vkh_stream_chunk(fd, out, route, serial, created, piece[0 .. ready], false, thinking, "", model) { return; }
            for i in pending - ready { piece[i] = piece[ready + i]; }
            pending -= ready;
        }
    }
    if pending > 0 && !vkh_stream_chunk(fd, out, route, serial, created, piece[0 .. pending],
        false, thinking, "", model) { return; }
    if !vkh_stream_chunk(fd, out, route, serial, created, "", false, false, finish, model) { return; }
    if report_usage {
        var n = cp_put(out, 0, "data: ");
        n = vkh_identity(out, n, route, serial, created, true, model);
        n = cp_put(out, n, ",\"choices\":[]");
        n = vkh_usage(out, n, prompt.len, count);
        n = cp_put(out, n, "}\n\n");
        if n > out.len || ht_write_all_impl(fd, out[0 .. n]) != 0 { return; }
    }
    ht_write_all_impl(fd, "data: [DONE]\n\n");
}

fn vkh_decode_body(context: u64, fd: i64, body: const u8[:], route: i64, cap: i64, serial: i64, vocabulary: i64, model_kind: i64,
    prompt_ids: i64[:], generated: i64[:], text: u8[:], response: u8[:]) {
    if !ih_chat_depth_ok(body) {
        vkh_error(fd, 400, "invalid JSON nesting");
        return;
    }
    let size = body.len + 1;
    var tree: i64[>..] = [];
    tree.resize(size * 14, 0);
    var numbers: f64[>..] = [];
    numbers.resize(size, 0.0);
    var strings: u8[>..] = [];
    strings.resize(size, 0);
    var state: i64[16] = [0; 16];
    let root = json_parse_abi(body, tree[0..size], tree[size..2*size], numbers[..], tree[2*size..3*size],
        tree[3*size..4*size], tree[4*size..5*size], tree[5*size..6*size], tree[6*size..7*size],
        tree[7*size..8*size], tree[8*size..9*size], tree[9*size..10*size], tree[10*size..11*size],
        strings[..], tree[11*size..12*size], tree[12*size..13*size], tree[13*size..14*size], state);
    if root < 0 || tree[root] != PJ_K_OBJECT {
        vkh_error(fd, 400, "expected a JSON object");
        return;
    }
    let model = vkh_name(model_kind);
    let bonsai = model_kind == 1 || model_kind == 2;
    let inkling = model_kind == 3;
    let field: const u8[:] = if route == 25 { "content" } else if route == 13 { "messages" } else { "prompt" };
    var input: i64 = -1;
    var limit: i64 = 16;
    var special = false;
    var ignore_eog = false;
    var return_tokens = false;
    var stream = false;
    var report_usage = false;
    let first = tree[4 * size + root];
    for i in tree[5 * size + root] {
        let edge = first + i;
        let key_at = tree[11 * size + edge];
        let key = strings[key_at .. key_at + tree[12 * size + edge]];
        let node = tree[13 * size + edge];
        let kind = tree[node];
        let value = tree[size + node];
        if key == field { input = node; }
        else if route == 25 && key == "parse_special" && kind == PJ_K_BOOL { special = value != 0; }
        else if route == 25 && key == "add_special" && kind == PJ_K_BOOL && value == 0 {}
        else if route != 25 && (key == "n_predict" || key == "max_tokens") &&
            kind == PJ_K_INT && value > 0 { limit = value; }
        else if route != 25 && key == "temperature" &&
            ((kind == PJ_K_INT && value == 0) || (kind == PJ_K_FLOAT && numbers[node] == 0.0)) {}
        else if route != 25 && key == "cache_prompt" && kind == PJ_K_BOOL && value == 0 {}
        else if (route == 12 || route == 13) && key == "stream" && kind == PJ_K_BOOL { stream = value != 0; }
        else if route == 11 && key == "stream" && kind == PJ_K_BOOL && value == 0 {}
        else if (route == 12 || route == 13) && key == "stream_options" &&
            kind == PJ_K_OBJECT && tree[5 * size + node] == 1 {
            let child_edge = tree[4 * size + node];
            let child_at = tree[11 * size + child_edge];
            let child = tree[13 * size + child_edge];
            if strings[child_at .. child_at + tree[12 * size + child_edge]] != "include_usage" ||
                tree[child] != PJ_K_BOOL {
                vkh_error(fd, 400, "unsupported stream option");
                return;
            }
            report_usage = tree[size + child] != 0;
        }
        else if (bonsai || inkling) && route == 13 && key == "enable_thinking" && kind == PJ_K_BOOL && value == 0 {}
        else if inkling && route == 13 && key == "reasoning_effort" {
            let at = tree[2 * size + node];
            if !((kind == PJ_K_STRING && strings[at .. at + tree[3 * size + node]] == "none") ||
                 (kind == PJ_K_INT && value == 0) || (kind == PJ_K_FLOAT && numbers[node] == 0.0)) {
                vkh_error(fd, 400, "Inkling text chat requires reasoning_effort none");
                return;
            }
        }
        else if route != 25 && key == "ignore_eos" && kind == PJ_K_BOOL { ignore_eog = value != 0; }
        else if route != 25 && key == "return_tokens" && kind == PJ_K_BOOL { return_tokens = value != 0; }
        else if route != 25 && key == "model" && kind == PJ_K_STRING {
            let at = tree[2 * size + node];
            if strings[at .. at + tree[3 * size + node]] != model {
                vkh_error(fd, 400, "unknown model");
                return;
            }
        } else {
            vkh_error(fd, 400, "unsupported request option");
            return;
        }
    }
    if (report_usage && !stream) || (return_tokens && stream) {
        vkh_error(fd, 400, "incompatible stream options");
        return;
    }
    if input < 0 {
        vkh_error(fd, 400, "missing prompt or content");
        return;
    }
    var count: i64 = -1;
    if route == 13 {
        count = vkh_chat_prompt(context, input, tree[..], size, strings[..], prompt_ids, text, model_kind);
    } else if tree[input] == PJ_K_STRING {
        let at = tree[2 * size + input];
        count = vkh_tokenize(context, strings[at .. at + tree[3 * size + input]], prompt_ids, special);
    } else if route != 25 && tree[input] == PJ_K_ARRAY {
        count = tree[5 * size + input];
        if count <= 0 || count > prompt_ids.len {
            vkh_error(fd, 400, "invalid prompt ID count");
            return;
        }
        let edge = tree[4 * size + input];
        for i in count {
            let item = tree[13 * size + edge + i];
            let id = tree[size + item];
            if tree[item] != PJ_K_INT || id < 0 || id >= vocabulary {
                vkh_error(fd, 400, "invalid prompt token ID");
                return;
            }
            prompt_ids[i] = id;
        }
    }
    if count <= 0 || count > prompt_ids.len {
        vkh_error(fd, 400, "invalid or empty prompt");
        return;
    }
    var n: i64 = 0;
    if route == 25 {
        n = cp_put(response, n, "{\"tokens\":");
        n = vkh_json_ids(response, n, prompt_ids[0 .. count]);
        n = cp_chr(response, n, 125);
    } else {
        if count > cap || limit > generated.len || limit - 1 > cap - count {
            vkh_error(fd, 400, "prompt and output exceed model context");
            return;
        }
        let created = vkh_now();
        let inkling_chat = inkling && route == 13;
        if stream {
            vkh_stream(context, fd, route, serial, created, prompt_ids[0 .. count], limit,
                ignore_eog, report_usage, inkling_chat, response, model);
            return;
        }
        var result: i64[3] = [0, 0, 0];
        var reasoning: u8[>..] = [];
        if inkling_chat { reasoning.resize(text.len, 0); }
        let bytes = vkh_generate(context, prompt_ids[0 .. count], limit, ignore_eog, generated, text, reasoning[..], result);
        if bytes < 0 || bytes > text.len || result[0] < 0 || result[0] > limit {
            vkh_error(fd, 500, "Vulkan inference failed");
            return;
        }
        let stop: const u8[:] = if result[1] != 0 { "stop" } else { "length" };
        if route == 12 || route == 13 {
            n = vkh_identity(response, n, route, serial, created, false, model);
            n = cp_put(response, n, ",\"choices\":[{\"index\":0,");
        } else {
            n = cp_chr(response, n, 123);
        }
        if route == 13 {
            n = cp_put(response, n, "\"message\":{\"role\":\"assistant\",\"content\":");
        } else {
            n = cp_put(response, n, if route == 12 { "\"text\":" } else { "\"content\":" });
        }
        n = vkh_json_text(response, n, text[0 .. bytes]);
        if route == 13 {
            if result[2] > 0 {
                n = cp_put(response, n, ",\"reasoning_content\":");
                n = vkh_json_text(response, n, reasoning[0 .. result[2]]);
            }
            n = cp_chr(response, n, 125);
        }
        n = cp_put(response, n, ",\"finish_reason\":");
        n = vkh_json_text(response, n, stop);
        if return_tokens {
            n = cp_put(response, n, ",\"tokens\":");
            n = vkh_json_ids(response, n, generated[0 .. result[0]]);
        }
        if route == 12 || route == 13 { n = cp_put(response, n, "}]"); }
        n = vkh_usage(response, n, count, result[0]);
        n = cp_chr(response, n, 125);
    }
    if n > response.len { vkh_error(fd, 500, "Vulkan response too large"); }
    else { ih_response(fd, 200, response[0 .. n]); }
}

export fn fg_vk_http_serve(context: u64, port: i64, limit: i64, cap: i64, vocabulary: i64, model_kind: i64) -> i64 {
    if port <= 0 || port > 65535 || limit < 0 || cap <= 0 || cap > 32768 { return -1; }
    var opts: i64[1] = [0];
    let listener = ht_open_impl(port, opts);
    if listener < 0 { return listener; }
    var request: u8[65536] = [0; 65536];
    var prompt_ids: i64[32768] = [0; 32768];
    var generated: i64[32768] = [0; 32768];
    var text: u8[65536] = [0; 65536];
    var response: u8[>..] = [];
    response.resize(65536 * 6 + 32768 * 7 + 512, 0);
    var served: i64 = 0;
    vkh_ready(port);
    while limit == 0 || served < limit {
        var address: u8[16] = [0; 16];
        var length: i64[1] = [16];
        let fd = gx_accept(listener, address, length);
        if fd < 0 {
            gx_close(listener);
            return -1;
        }
        if vkh_timeout(fd) == 0 {
            var meta: i64[6] = [0; 6];
            let status = ih_read(fd, request, meta);
            if status != 0 { vkh_error(fd, status, "invalid HTTP request"); }
            else if meta[2] == 1 { ih_response(fd, 200, "{\"status\":\"ok\"}"); }
            else if meta[2] == 5 {
                var n = cp_put(response[..], 0, "{\"object\":\"list\",\"data\":[{\"id\":");
                n = vkh_json_text(response[..], n, vkh_name(model_kind));
                n = cp_put(response[..], n, ",\"object\":\"model\",\"owned_by\":\"local\"}]}");
                ih_response(fd, 200, response[0 .. n]);
            } else if meta[2] == 11 || meta[2] == 12 || meta[2] == 13 || meta[2] == 25 {
                if meta[1] <= 0 || meta[3] == 0 { vkh_error(fd, 400, "expected application/json body"); }
                else {
                    vkh_decode_body(context, fd, request[meta[0] .. meta[0] + meta[1]], meta[2], cap, served + 1, vocabulary, model_kind,
                        prompt_ids, generated, text, response[..]);
                }
            } else { vkh_error(fd, 404, "route is not supported by Vulkan"); }
        }
        gx_close(fd);
        served += 1;
    }
    gx_close(listener);
    return served;
}
