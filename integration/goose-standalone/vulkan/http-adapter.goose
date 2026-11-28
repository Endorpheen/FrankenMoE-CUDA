const VK_HTTP_MODEL: u64 = 0;
const VK_HTTP_TOKENIZER: u64 = 8;
const VK_HTTP_LOGITS: u64 = 16;
const VK_HTTP_KIND: u64 = 24;
const VK_HTTP_CAPACITY: u64 = 32;
const VK_HTTP_VOCABULARY: u64 = 40;
const VK_HTTP_PREVIOUS: u64 = 48;
const VK_HTTP_CONTROLS: u64 = 56;
const VK_HTTP_BYTES: u64 = 80;
const VK_HTTP_QWEN_BYTES: u64 = 11408;
const VK_HTTP_QWEN_POSITION: u64 = 11396;
const VK_HTTP_QWEN_RECORDING: u64 = 2768;
const VK_HTTP_FIRST_TOKEN: u64 = 0xffffffff;
const VK_HTTP_MARK_COUNT: i64 = 5;

extern fn fg_vk_inkling_open(path: u64, capacity: u32, index: u32, cpu: i32) -> u64;
extern fn fg_vk_inkling_close(model: u64);
extern fn fg_vk_inkling_step(model: u64, id: u32, logits: u64) -> i32;
extern fn fg_vk_inkling_reset(model: u64) -> i32;
extern fn fg_vk_http_serve(owner: u64, port: i64, requests: i64, capacity: i64, vocabulary: i64, kind: i64) -> i64;
extern "gs_os_write_stderr" fn vk_http_error(text: const u8[:]);
extern fn fg_vk_http_time(timer: u64) -> i64;

fn vk_http_set_u32(address: u64, value: u32) {
    vk_gguf_copy(unsafe_c_ptr<c_ptr<void>>(address), unsafe_c_address<c_ptr<const void>>(value), 4);
}

fn vk_http_u32(address: u64) -> u32 {
    var value: u32 = 0;
    vk_gguf_copy(unsafe_c_address<c_ptr<void>>(value), unsafe_c_ptr<c_ptr<const void>>(address), 4);
    return value;
}

fn vk_http_step(owner: u64, id: u32, logits: u64) -> bool {
    let model = vk_gguf_read_word(owner + VK_HTTP_MODEL);
    let kind = vk_gguf_read_word(owner + VK_HTTP_KIND);
    if kind == 3 { return fg_vk_inkling_step(model, id, logits) != 0; }
    if kind == 2 { return fg_vk_apex_step(model, id, logits) != 0; }
    if kind == 1 { return fg_vk_bonsai_step(model, id, logits) != 0; }
    return fg_vk_qwen2_step(model, id, logits) != 0;
}

fn vk_http_reset(owner: u64) -> bool {
    let model = vk_gguf_read_word(owner + VK_HTTP_MODEL);
    let kind = vk_gguf_read_word(owner + VK_HTTP_KIND);
    if kind == 3 { return fg_vk_inkling_reset(model) != 0; }
    if kind == 2 { return fg_vk_apex_reset(model) != 0; }
    if kind == 1 { return fg_vk_bonsai_reset(model) != 0; }
    vk_http_set_u32(model + VK_HTTP_QWEN_POSITION, 0);
    vk_http_set_u32(model + VK_HTTP_QWEN_RECORDING, 0);
    return true;
}

export fn fg_vk_http_tokenize(owner: u64, prompt: const u8[:], ids: i64[:], special: bool) -> i64 {
    if prompt.len <= 0 || prompt.len > 65536 || ids.len <= 0 || ids.len > 32768 { return -1; }
    let tokens = vk_tok_table(ids.len, 4).address;
    if tokens == 0 { return -1; }
    let buffer = vk_tok_address(unsafe_c_slice_data<c_ptr<const void>>(prompt));
    let count = fg_vk_tokenizer_encode(vk_gguf_read_word(owner + VK_HTTP_TOKENIZER), buffer,
        prompt.len as! u64, if special { 1 } else { 0 }, tokens, ids.len as! u32) as! i64;
    if count > 0 {
        for i in count { ids[i] = vk_http_u32(tokens + (i as! u64) * 4) as! i64; }
    }
    vk_tok_release(tokens);
    return count;
}

export fn fg_vk_http_prefill(owner: u64, prompt: const i64[:], limit: i64) -> i64 {
    let capacity = vk_gguf_read_word(owner + VK_HTTP_CAPACITY) as! i64;
    if prompt.len <= 0 || limit <= 0 || prompt.len > capacity || limit - 1 > capacity - prompt.len { return -1; }
    let vocabulary = vk_gguf_read_word(owner + VK_HTTP_VOCABULARY) as! i64;
    for i in prompt.len { if prompt[i] < 0 || prompt[i] >= vocabulary { return -1; } }
    if !vk_http_reset(owner) { return -1; }
    vk_gguf_write_word(owner + VK_HTTP_PREVIOUS, VK_HTTP_FIRST_TOKEN);
    let logits = vk_gguf_read_word(owner + VK_HTTP_LOGITS);
    for i in prompt.len {
        if !vk_http_step(owner, prompt[i] as! u32, if i + 1 == prompt.len { logits } else { 0 }) { return -1; }
    }
    return 0;
}

export fn fg_vk_http_next(owner: u64, ignore_eog: bool, text: u8[:], id: i64[:]) -> i64 {
    if id.len < 1 || text.len < 0 { return -1; }
    let previous = vk_gguf_read_word(owner + VK_HTTP_PREVIOUS);
    let logits = vk_gguf_read_word(owner + VK_HTTP_LOGITS);
    if previous != VK_HTTP_FIRST_TOKEN && !vk_http_step(owner, previous as! u32, logits) { return -1; }
    let tokenizer = vk_gguf_read_word(owner + VK_HTTP_TOKENIZER);
    var token: u32 = 0;
    if fg_vk_choose(logits, vk_gguf_read_word(owner + VK_HTTP_VOCABULARY) as! u32,
        if ignore_eog { tokenizer } else { 0 }, vk_tok_address(unsafe_c_address<c_ptr<const void>>(token))) == 0 { return -1; }
    if !ignore_eog && fg_vk_tokenizer_is_eog(tokenizer, token) != 0 { return -2; }
    let bytes = fg_vk_tokenizer_piece(tokenizer, token, vk_tok_address(unsafe_c_slice_data<c_ptr<const void>>(text)),
        text.len as! u64) as! i64;
    if bytes < 0 || bytes > text.len { return -1; }
    vk_gguf_write_word(owner + VK_HTTP_PREVIOUS, token as! u64);
    id[0] = token as! i64;
    return bytes;
}

export fn fg_vk_http_inkling_control(owner: u64, id: i64) -> i64 {
    for i in VK_HTTP_MARK_COUNT {
        if id == (vk_http_u32(owner + VK_HTTP_CONTROLS + (i as! u64) * 4) as! i64) { return i + 1; }
    }
    return 0;
}

fn vk_http_control(owner: u64, index: u64, name: const u8[:]) -> bool {
    let tokenizer = vk_gguf_read_word(owner + VK_HTTP_TOKENIZER);
    let text = vk_tok_address(unsafe_c_slice_data<c_ptr<const void>>(name));
    return fg_vk_tokenizer_encode(tokenizer, text, name.len as! u64, 1, owner + VK_HTTP_CONTROLS + index * 4, 1) == 1;
}

fn vk_http_prepare_controls(owner: u64) -> bool {
    return vk_http_control(owner, 0, "<|message_model|>") && vk_http_control(owner, 1, "<|content_text|>") &&
        vk_http_control(owner, 2, "<|content_thinking|>") && vk_http_control(owner, 3, "<|end_message|>") &&
        vk_http_control(owner, 4, "<|content_model_end_sampling|>");
}

export fn fg_vk_http_generate(owner: u64, prompt: const i64[:], limit: i64,
    ignore_eog: bool, ids: i64[:], text: u8[:], reasoning: u8[:], info: i64[:]) -> i64 {
    if limit > ids.len || info.len < 3 || fg_vk_http_prefill(owner, prompt, limit) != 0 { return -1; }
    var at: i64 = 0;
    var thought: i64 = 0;
    var thinking = false;
    var piece: u8[1028];
    info[0] = 0;
    info[1] = 0;
    for step in limit {
        let n = fg_vk_http_next(owner, ignore_eog, piece, ids[step .. step + 1]);
        if n == -2 { info[1] = 1; break; }
        if n < 0 { return -1; }
        info[0] += 1;
        let control: i64 = if reasoning.len != 0 { fg_vk_http_inkling_control(owner, ids[step]) } else { 0 };
        if control != 0 {
            if control != 1 { thinking = control == 3; }
            continue;
        }
        if n > text.len - at - thought || (thinking && n > reasoning.len - thought) { return -1; }
        if thinking {
            for i in n { reasoning[thought + i] = piece[i]; }
            thought += n;
        } else {
            for i in n { text[at + i] = piece[i]; }
            at += n;
        }
    }
    info[2] = thought;
    return at;
}

export fn fg_vk_http_now() -> i64 { return fg_vk_http_time(0); }

export fn fg_vk_http_ready(port: i64) {
    vk_http_error(str("Vulkan HTTP listening on 127.0.0.1:", port, "\n"));
}

fn vk_http_open(owner: u64, path: u64, capacity: u32, index: u32, cpu: i32, kind: u32) -> u64 {
    if kind == 3 {
        if !vk_http_prepare_controls(owner) { return 0; }
        return fg_vk_inkling_open(path, capacity, index, cpu);
    }
    if kind == 2 { return fg_vk_apex_open(path, capacity, index, cpu); }
    if kind == 1 { return fg_vk_bonsai_open(path, capacity, index, cpu); }
    if kind != 0 { return 0; }
    let model = vk_tok_table(1, VK_HTTP_QWEN_BYTES).address;
    if model == 0 { return 0; }
    if fg_vk_qwen2_open(model, path, capacity, index, cpu) == 0 {
        fg_vk_qwen2_close(model);
        vk_tok_release(model);
        return 0;
    }
    return model;
}

fn vk_http_close(model: u64, kind: u32) {
    if model == 0 { return; }
    if kind == 3 { fg_vk_inkling_close(model); }
    else if kind == 2 { fg_vk_apex_close(model); }
    else if kind == 1 { fg_vk_bonsai_close(model); }
    else { fg_vk_qwen2_close(model); vk_tok_release(model); }
}

export fn fg_vk_http_run(path: u64, capacity: u32, index: u32, cpu: i32, port: u32, requests: u32, kind: u32) -> i32 {
    if !vk_http_network_ready() { return 0; }
    let owner = vk_tok_table(1, VK_HTTP_BYTES).address;
    var ok = false;
    if owner != 0 {
        let vocabulary: i64 = if kind == 3 { 200058 } else { if kind == 1 || kind == 2 { 248320 } else { 151936 } };
        let tokenizer = fg_vk_model_tokenizer_open(path, kind);
        let logits = vk_tok_table(vocabulary, 4).address;
        vk_gguf_write_word(owner + VK_HTTP_TOKENIZER, tokenizer);
        vk_gguf_write_word(owner + VK_HTTP_LOGITS, logits);
        vk_gguf_write_word(owner + VK_HTTP_KIND, kind as! u64);
        vk_gguf_write_word(owner + VK_HTTP_CAPACITY, capacity as! u64);
        vk_gguf_write_word(owner + VK_HTTP_VOCABULARY, vocabulary as! u64);
        if tokenizer != 0 && logits != 0 {
            let model = vk_http_open(owner, path, capacity, index, cpu, kind);
            if model != 0 {
                vk_gguf_write_word(owner + VK_HTTP_MODEL, model);
                ok = fg_vk_http_serve(owner, port as! i64, requests as! i64, capacity as! i64,
                    vocabulary, kind as! i64) >= 0;
                vk_http_close(model, kind);
            }
        }
        fg_vk_tokenizer_close(tokenizer);
        vk_tok_release(logits);
        vk_tok_release(owner);
    }
    vk_http_network_close();
    if !ok { vk_http_error("Vulkan HTTP stopped with an error\n"); }
    return if ok { 1 } else { 0 };
}
