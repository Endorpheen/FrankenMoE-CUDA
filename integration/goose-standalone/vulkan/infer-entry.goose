extern "gs_os_arg_count" fn vk_cli_arg_count() -> i64;
extern "gs_os_arg" fn vk_cli_arg(index: i64, out: u8[>..]&);
extern "exit" fn vk_cli_exit(status: i32);
extern fn putchar(byte: i32) -> i32;
extern fn fg_vk_queue_init(argc: i32, argv: u64);
extern fn fg_vk_bank_init(argc: i32, argv: u64);
extern fn fg_socket_init(argc: i32, argv: u64);
extern fn goose_init(argc: i32, argv: u64);
extern fn fg_vk_cli_crt_write(address: u64, size: u64, count: u64, file: u64) -> u64;
extern fn fg_vk_cli_crt_ferror(file: u64) -> i32;

const VK_CLI_QWEN_DEVICE_NAME: u64 = 60;
const VK_CLI_QWEN_CONTEXT: u64 = 11392;

struct VkCliOptions {
    device: u32, predict: u32, port: u32, requests: u32, context: u32,
    cpu: bool, special: bool, tokenize_only: bool,
    explicit_kind: bool, kind: u32, dump: u64, prompt_text: u64, prompt_file: u64, start: i64
}

fn vk_cli_usage() -> i32 {
    vk_http_error("Usage: gooseinfer --backend=vulkan MODEL [--device-index N] [--allow-cpu] [--n-predict N] [--logits FILE]\n       -- TOKEN ... | --prompt TEXT | --prompt-file UTF8_FILE [--special] [--tokenize-only]\n       --bonsai -- TOKEN ...\n       --apex -- TOKEN ...\n       --inkling -- TOKEN ...\n       --serve PORT [--ctx-size N] [--max-requests N]\n");
    return 2;
}

fn vk_cli_number(text: const u8[:], limit: u64) -> i64 {
    if text.len == 0 { return -1; }
    var value: u64 = 0;
    for i in text.len {
        if text[i] < 48 || text[i] > 57 { return -1; }
        value = value * 10 + ((text[i] - 48) as! u64);
        if value >= limit { return -1; }
    }
    return value as! i64;
}

fn vk_cli_parse(args: (const u8[:])[:], argv: const u64[:], options: VkCliOptions&) -> bool {
    var at: i64 = 2;
    while at < args.len && args[at] != "--" {
        let option = args[at];
        if option == "--allow-cpu" { options.cpu = true; }
        else if option == "--special" { options.special = true; }
        else if option == "--tokenize-only" { options.tokenize_only = true; }
        else if !options.explicit_kind && (option == "--bonsai" || option == "--apex" || option == "--inkling") {
            options.kind = if option == "--bonsai" { 1 } else { if option == "--apex" { 2 } else { 3 } };
            options.explicit_kind = true;
        } else {
            if at + 1 >= args.len { return false; }
            at += 1;
            if option == "--prompt" { options.prompt_text = argv[at]; }
            else if option == "--prompt-file" { options.prompt_file = argv[at]; }
            else if option == "--logits" { options.dump = argv[at]; }
            else if option == "--device-index" || option == "--max-requests" {
                let value = vk_cli_number(args[at], 0xffffffff);
                if value < 0 || (value == 0 && option == "--max-requests") { return false; }
                if option == "--device-index" { options.device = value as! u32; }
                else { options.requests = value as! u32; }
            } else if option == "--n-predict" || option == "--ctx-size" || option == "--serve" {
                let limit: u64 = if option == "--serve" { 65536 } else { 32769 };
                let value = vk_cli_number(args[at], limit);
                if value <= 0 { return false; }
                if option == "--n-predict" { options.predict = value as! u32; }
                else if option == "--ctx-size" { options.context = value as! u32; }
                else { options.port = value as! u32; }
            } else { return false; }
        }
        at += 1;
    }
    options.start = at;
    return true;
}

fn vk_cli_detect(path: u64) -> i64 {
    let mode: u8[3] = [114,98,0];
    let file = fg_vk_tok_fopen(path, vk_tok_address(unsafe_c_address<c_ptr<const void>>(mode[0])));
    if file == 0 { return -1; }
    let header = vk_tok_table(1, TK_HEADER_BYTES).address;
    var result: i64 = -1;
    if header != 0 {
        if fg_gguf_read_header(header, file) >= 0 {
            if vk_tok_meta_is(header, "general.architecture", "inkling2_loop") { result = 3; }
            else if vk_tok_meta_is(header, "general.architecture", "qwen35moe") { result = 2; }
            else if vk_tok_meta_is(header, "general.architecture", "qwen35") { result = 1; }
            else if vk_tok_meta_is(header, "general.architecture", "qwen2") { result = 0; }
        }
        vk_tok_release(vk_gguf_read_word(header));
        vk_tok_release(header);
    }
    fg_vk_tok_fclose(file);
    return result;
}

fn vk_cli_read_prompt(path: u64) -> VkGgufSlice {
    let empty = vk_tok_slice(0, 0);
    let mode: u8[3] = [114,98,0];
    let file = fg_vk_tok_fopen(path, vk_tok_address(unsafe_c_address<c_ptr<const void>>(mode[0])));
    if file == 0 { fg_vk_tok_perror(path); return empty; }
    let buffer = vk_tok_table(65537, 1).address;
    var size: u64 = 0;
    if buffer != 0 {
        size = fg_vk_crt_read(buffer, 1, 65537, file);
        if size > 65536 || fg_vk_cli_crt_ferror(file) != 0 { size = 0; }
        for i in (size as! i64) {
            if vk_gguf_read_byte(buffer + (i as! u64)) == 0 { size = 0; break; }
        }
    }
    fg_vk_tok_fclose(file);
    if size == 0 { vk_tok_release(buffer); return empty; }
    return vk_tok_slice(buffer, size as! i64);
}

fn vk_cli_print(text: const u8[:]) -> bool {
    for i in text.len { if putchar(text[i] as! i32) < 0 { return false; } }
    return true;
}

fn vk_cli_dump(path: u64, logits: u64, vocabulary: u32) -> bool {
    let mode: u8[3] = [119,98,0];
    let file = fg_vk_tok_fopen(path, vk_tok_address(unsafe_c_address<c_ptr<const void>>(mode[0])));
    if file == 0 { return false; }
    let written = fg_vk_cli_crt_write(logits, 4, vocabulary as! u64, file);
    return fg_vk_tok_fclose(file) == 0 && written == (vocabulary as! u64);
}

fn vk_cli_model(path: u64, opts: VkCliOptions, capacity: u32) -> u64 {
    let cpu: i32 = if opts.cpu { 1 } else { 0 };
    if opts.kind == 3 { return fg_vk_inkling_open(path, capacity, opts.device, cpu); }
    if opts.kind == 2 { return fg_vk_apex_open(path, capacity, opts.device, cpu); }
    if opts.kind == 1 { return fg_vk_bonsai_open(path, capacity, opts.device, cpu); }
    return vk_http_open(0, path, capacity, opts.device, cpu, 0);
}

fn vk_cli_generate(owner: u64, ids: u64, prompt: u32, opts: VkCliOptions, text_mode: bool) -> bool {
    let logits = vk_gguf_read_word(owner + VK_HTTP_LOGITS);
    let model = vk_gguf_read_word(owner + VK_HTTP_MODEL);
    let tokenizer = vk_gguf_read_word(owner + VK_HTTP_TOKENIZER);
    let vocab = vk_gguf_read_word(owner + VK_HTTP_VOCABULARY) as! u32;
    for i in (prompt as! i64) {
        let out: u64 = if i + 1 == (prompt as! i64) { logits } else { 0 };
        if !vk_http_step(owner, vk_http_u32(ids + (i as! u64) * 4), out) { return false; }
    }
    if opts.dump != 0 && !vk_cli_dump(opts.dump, logits, vocab) { return false; }
    if text_mode && !vk_cli_binary_stdout() { return false; }
    for i in (opts.predict as! i64) {
        var token: u32 = 0;
        if fg_vk_choose(logits, vocab, 0, vk_tok_address(unsafe_c_address<c_ptr<const void>>(token))) == 0 { return false; }
        if text_mode {
            if fg_vk_tokenizer_is_eog(tokenizer, token) != 0 { break; }
            var output: u8[1024];
            let size = fg_vk_tokenizer_piece(tokenizer, token, vk_tok_address(unsafe_c_address<c_ptr<const void>>(output[0])), 1024);
            if size < 0 || size > 1024 || !vk_cli_print(output[0 .. size as! i64]) { return false; }
        } else {
            if !vk_cli_print(str(token, "\n")) { return false; }
            if opts.kind == 1 && fg_vk_bonsai_is_eog(model, token) != 0 { break; }
            if (opts.kind == 2 || opts.kind == 3) && fg_vk_tokenizer_is_eog(tokenizer, token) != 0 { break; }
        }
        if i + 1 < (opts.predict as! i64) && !vk_http_step(owner, token, logits) { return false; }
    }
    return !text_mode || putchar(10) >= 0;
}

fn vk_cli_infer(path: u64, opts: VkCliOptions, ids: u64, prompt: u32, tokenizer: u64, vocabulary: u32, text_mode: bool) -> i32 {
    var data: u64[10] = [0; 10];
    let owner = vk_tok_address(unsafe_c_address<c_ptr<const void>>(data[0]));
    let logits = vk_tok_table(vocabulary as! i64, 4).address;
    var model: u64 = 0;
    if logits != 0 { model = vk_cli_model(path, opts, prompt + opts.predict - 1); }
    vk_gguf_write_word(owner + VK_HTTP_MODEL, model);
    vk_gguf_write_word(owner + VK_HTTP_TOKENIZER, tokenizer);
    vk_gguf_write_word(owner + VK_HTTP_LOGITS, logits);
    vk_gguf_write_word(owner + VK_HTTP_KIND, opts.kind as! u64);
    vk_gguf_write_word(owner + VK_HTTP_VOCABULARY, vocabulary as! u64);
    if model != 0 && opts.kind == 0 {
        var name: u8[>..] = [];
        var i: u64 = 0;
        while i < 256 {
            let byte = vk_gguf_read_byte(model + VK_CLI_QWEN_DEVICE_NAME + i);
            if byte == 0 { break; }
            name.push(byte);
            i += 1;
        }
        vk_http_error(str("Vulkan Qwen2 on ", name, ", context=", vk_http_u32(model + VK_CLI_QWEN_CONTEXT), "\n"));
    }
    let ok = model != 0 && vk_cli_generate(owner, ids, prompt, opts, text_mode);
    vk_http_close(model, opts.kind);
    vk_tok_release(logits);
    if !ok { vk_http_error("Vulkan inference failed\n"); }
    return if ok { 0 } else { 1 };
}

fn vk_cli_execute(args: (const u8[:])[:], argv: const u64[:]) -> i32 {
    if args.len < 3 { return vk_cli_usage(); }
    var opt = VkCliOptions { device: 0xffffffff, predict: 1, port: 0, requests: 0, context: 0,
        cpu: false, special: false, tokenize_only: false, explicit_kind: false, kind: 0,
        dump: 0, prompt_text: 0, prompt_file: 0, start: 2 };
    if !vk_cli_parse(args, argv, opt) { return vk_cli_usage(); }
    let path = argv[1];
    goose_init(0, 0);
    if !opt.explicit_kind {
        let kind = vk_cli_detect(path);
        if kind < 0 { vk_http_error("Unable to detect a supported Vulkan model architecture\n"); return 1; }
        opt.kind = kind as! u32;
    }
    let text_mode = opt.prompt_text != 0 || opt.prompt_file != 0;
    if opt.port != 0 {
        if opt.start != args.len || text_mode || opt.dump != 0 || opt.special || opt.tokenize_only ||
            opt.predict != 1 || (opt.cpu && opt.device == 0xffffffff) { return vk_cli_usage(); }
        fg_socket_init(0, 0);
        let context: u32 = if opt.context == 0 { 512 } else { opt.context };
        return if fg_vk_http_run(path, context, opt.device, if opt.cpu { 1 } else { 0 },
            opt.port, opt.requests, opt.kind) != 0 { 0 } else { 1 };
    }
    if opt.requests != 0 || opt.context != 0 || (opt.prompt_text != 0 && opt.prompt_file != 0) ||
        (opt.cpu && opt.device == 0xffffffff) || (opt.tokenize_only && opt.dump != 0) { return vk_cli_usage(); }
    if text_mode {
        if opt.start != args.len { return vk_cli_usage(); }
    } else if opt.start + 1 >= args.len || opt.special || opt.tokenize_only { return vk_cli_usage(); }
    let capacity: u32 = if opt.tokenize_only { 32768 } else { 32769 - opt.predict };
    let start = opt.start + 1;
    var prompt: u32 = if text_mode { 0 } else { (args.len - start) as! u32 };
    if prompt > capacity { return vk_cli_usage(); }
    let vocab: u32 = if opt.kind == 3 { 200058 } else { if opt.kind == 0 { 151936 } else { 248320 } };
    let ids = vk_tok_table((if text_mode { capacity } else { prompt }) as! i64, 4).address;
    if ids == 0 { return 1; }
    var tokenizer: u64 = 0;
    var ok = true;
    if text_mode {
        let owned = if opt.prompt_file != 0 { vk_cli_read_prompt(opt.prompt_file) } else { vk_tok_slice(0, 0) };
        let text = if opt.prompt_text != 0 { vk_tok_slice(opt.prompt_text, fg_vk_crt_strlen(opt.prompt_text) as! i64) } else { owned };
        tokenizer = fg_vk_model_tokenizer_open(path, opt.kind);
        let count = fg_vk_tokenizer_encode(tokenizer, text.address, text.len as! u64,
            if opt.special { 1 } else { 0 }, ids, capacity);
        vk_tok_release(owned.address);
        ok = count > 0;
        if ok { prompt = count as! u32; }
        else { vk_http_error("Unable to tokenize the prompt within the context limit\n"); }
    } else {
        for i in (prompt as! i64) {
            let id = vk_cli_number(args[start + i], vocab as! u64);
            if id < 0 { ok = false; break; }
            vk_http_set_u32(ids + (i as! u64) * 4, id as! u32);
        }
        if ok && (opt.kind == 2 || opt.kind == 3) {
            tokenizer = fg_vk_model_tokenizer_open(path, opt.kind);
            ok = tokenizer != 0;
        }
    }
    var status: i32 = 1;
    if ok && opt.tokenize_only {
        for i in (prompt as! i64) { vk_cli_print(str(vk_http_u32(ids + (i as! u64) * 4), "\n")); }
        status = 0;
    } else if ok { status = vk_cli_infer(path, opt, ids, prompt, tokenizer, vocab, text_mode); }
    fg_vk_tokenizer_close(tokenizer);
    vk_tok_release(ids);
    return status;
}

fn main() {
    fg_vk_queue_init(0, 0);
    fg_vk_bank_init(0, 0);
    var storage: u8[>..] = [];
    var offsets: i64[>..] = [0];
    let argc = vk_cli_arg_count();
    for i in argc {
        vk_cli_arg(i, storage);
        storage.push(0);
        offsets.push(storage.len);
    }
    var args: (const u8[:])[>..] = [];
    var argv: u64[>..] = [];
    for i in argc {
        let at = offsets[i];
        args.push(storage[at .. offsets[i + 1] - 1]);
        argv.push(vk_tok_address(unsafe_c_address<c_ptr<const void>>(storage[at])));
    }
    vk_cli_exit(vk_cli_execute(args, argv));
}
