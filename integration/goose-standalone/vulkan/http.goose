extern "fg_vk_http_tokenize" fn vkh_tokenize(context: u64, text: const u8[:], ids: i64[:], special: bool) -> i64;
extern "fg_vk_http_generate" fn vkh_generate(context: u64, prompt: const i64[:], limit: i64,
    ignore_eog: bool, ids: i64[:], text: u8[:], info: i64[:]) -> i64;
extern "fg_vk_http_timeout" fn vkh_timeout(fd: i64) -> i64;
extern "fg_vk_http_ready" fn vkh_ready(port: i64);

fn ih_log_request(method: i64, route: i64) {}
fn ih_log_response(status: i64, body: const u8[:]) {}

const VKH_MODEL: u8[:] = "qwen2.5-0.5b-instruct";

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

fn vkh_decode_body(context: u64, fd: i64, body: const u8[:], route: i64, cap: i64,
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
    let field: const u8[:] = if route == 25 { "content" } else { "prompt" };
    var input: i64 = -1;
    var limit: i64 = 16;
    var special = false;
    var ignore_eog = false;
    var return_tokens = false;
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
        else if route != 25 && (key == "stream" || key == "cache_prompt") && kind == PJ_K_BOOL && value == 0 {}
        else if route != 25 && key == "ignore_eos" && kind == PJ_K_BOOL { ignore_eog = value != 0; }
        else if route != 25 && key == "return_tokens" && kind == PJ_K_BOOL { return_tokens = value != 0; }
        else if route != 25 && key == "model" && kind == PJ_K_STRING {
            let at = tree[2 * size + node];
            if strings[at .. at + tree[3 * size + node]] != VKH_MODEL {
                vkh_error(fd, 400, "unknown model");
                return;
            }
        } else {
            vkh_error(fd, 400, "unsupported request option");
            return;
        }
    }
    if input < 0 {
        vkh_error(fd, 400, "missing prompt or content");
        return;
    }
    var count: i64 = -1;
    if tree[input] == PJ_K_STRING {
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
            if tree[item] != PJ_K_INT || id < 0 || id >= 151936 {
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
        var result: i64[2] = [0, 0];
        let bytes = vkh_generate(context, prompt_ids[0 .. count], limit, ignore_eog, generated, text, result);
        if bytes < 0 || bytes > text.len || result[0] < 0 || result[0] > limit {
            vkh_error(fd, 500, "Vulkan inference failed");
            return;
        }
        let stop: const u8[:] = if result[1] != 0 { "stop" } else { "length" };
        if route == 12 {
            n = cp_put(response, n, "{\"object\":\"text_completion\",\"model\":\"qwen2.5-0.5b-instruct\",\"choices\":[{\"index\":0,\"text\":");
        } else {
            n = cp_put(response, n, "{\"content\":");
        }
        n = vkh_json_text(response, n, text[0 .. bytes]);
        n = cp_put(response, n, ",\"finish_reason\":");
        n = vkh_json_text(response, n, stop);
        if return_tokens {
            n = cp_put(response, n, ",\"tokens\":");
            n = vkh_json_ids(response, n, generated[0 .. result[0]]);
        }
        if route == 12 { n = cp_put(response, n, "}]"); }
        n = cp_put(response, n, ",\"usage\":{\"prompt_tokens\":");
        n = cp_uint(response, n, count);
        n = cp_put(response, n, ",\"completion_tokens\":");
        n = cp_uint(response, n, result[0]);
        n = cp_put(response, n, ",\"total_tokens\":");
        n = cp_uint(response, n, count + result[0]);
        n = cp_put(response, n, "}}");
    }
    if n > response.len { vkh_error(fd, 500, "Vulkan response too large"); }
    else { ih_response(fd, 200, response[0 .. n]); }
}

export fn fg_vk_http_serve(context: u64, port: i64, limit: i64, cap: i64) -> i64 {
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
                ih_response(fd, 200, "{\"object\":\"list\",\"data\":[{\"id\":\"qwen2.5-0.5b-instruct\",\"object\":\"model\",\"owned_by\":\"local\"}]}");
            } else if meta[2] == 11 || meta[2] == 12 || meta[2] == 25 {
                if meta[1] <= 0 || meta[3] == 0 { vkh_error(fd, 400, "expected application/json body"); }
                else {
                    vkh_decode_body(context, fd, request[meta[0] .. meta[0] + meta[1]], meta[2], cap,
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
