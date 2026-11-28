extern "memcpy" fn cli_copy(dst: c_ptr<void>, src: c_ptr<const void>, size: u64) -> c_ptr<void>;
extern "malloc" fn cli_malloc(size: u64) -> c_ptr<void>;
extern "realloc" fn cli_realloc(p: c_ptr<void>, size: u64) -> c_ptr<void>;
extern "free" fn cli_free(p: c_ptr<void>);
extern "putchar" fn cli_putchar(byte: i32) -> i32;
extern "gooseinfer_cli_flush" fn cli_flush(stream: c_ptr<void>) -> i32;
extern "gs_os_write_stderr" fn cli_error(message: const u8[:]);

extern fn hose_tokenize(prompt: u64, length: i64, out: u64, capacity: i64) -> i64;
extern fn hose_detokenize(ids: u64, length: i64, out: u64, capacity: i64) -> i64;
extern fn hose_generate(prompt: u64, length: i64, limit: i64, ids: u64, text: u64, capacity: i64, stats: u64) -> i64;
extern fn hose_stats(out: u64);
extern fn hose_context_capacity() -> i64;
extern fn hose_model_residency(out: u64) -> i64;
extern fn hose_prefill_profile(out: u64);
extern fn hose_expert_cache_limit() -> i64;
extern fn hose_kv_format() -> i64;
extern fn fg_cuda_dense_bytes() -> i64;
extern fn fg_cuda_expert_bytes() -> i64;
extern fn fg_cuda_model_state_bytes() -> i64;
extern fn fg_cuda_expert_stats(out: i64[:]);
extern fn fg_standalone_cpu_batch_threads() -> i64;
extern fn fg_standalone_cpu_decode_threads() -> i64;
extern fn fg_standalone_configure_cpu_batch(threads: i64) -> i64;
extern fn fg_standalone_configure_cpu_decode(threads: i64) -> i64;
extern fn fg_standalone_configure_cpu_affinity(mask: u64, strict: bool, prefill: bool) -> i64;
extern fn hose_configure_kv(value: bool, format: u64) -> i64;
extern fn hose_configure_expert_cache(mib: i64) -> i64;
extern fn hose_configure_prefill(batch: i64, ubatch: i64) -> i64;
extern fn fg_goose_host_ensure_init();
extern fn hose_host_load_context(model: u64, capacity: i64);
extern fn ih_serve(port: i64, limit: i64, request: u8[:], output: u8[:], response: u8[:], ids: i64[:]) -> i64;

var cli_request: u8[262144];
var cli_output: u8[65536];
var cli_response: u8[524288];
var cli_ids: i64[2048];

fn cli_address(pointer: c_ptr<const void>) -> u64 {
    var bits: u64 = 0;
    cli_copy(unsafe_c_address<c_ptr<void>>(bits), unsafe_c_address<c_ptr<const void>>(pointer), 8);
    return bits;
}

fn cli_mut_address(pointer: c_ptr<void>) -> u64 {
    var bits: u64 = 0;
    cli_copy(unsafe_c_address<c_ptr<void>>(bits), unsafe_c_address<c_ptr<const void>>(pointer), 8);
    return bits;
}

fn cli_word(address: u64) -> u64 {
    var bits: u64 = 0;
    cli_copy(unsafe_c_address<c_ptr<void>>(bits), unsafe_c_ptr<c_ptr<const void>>(address), 8);
    return bits;
}

fn cli_byte(address: u64) -> u8 {
    var byte: u8 = 0;
    cli_copy(unsafe_c_address<c_ptr<void>>(byte), unsafe_c_ptr<c_ptr<const void>>(address), 1);
    return byte;
}

fn cli_arg(argv: u64, index: i64) -> u64 {
    return cli_word(argv + (index as! u64) * 8);
}

fn cli_text(address: u64) -> u8[>..] {
    var text: u8[>..] = [];
    if address == 0 { return text; }
    var i: u64 = 0;
    while cli_byte(address + i) != 0 {
        text.push(cli_byte(address + i));
        i += 1;
    }
    return text;
}

export fn fg_http_tokenize(prompt: const u8[:], ids: i64[:]) -> i64 {
    return hose_tokenize(cli_address(unsafe_c_slice_data<c_ptr<const void>>(prompt)), prompt.len,
        cli_address(unsafe_c_slice_data<c_ptr<const void>>(ids)), ids.len);
}

export fn fg_http_detokenize(ids: const i64[:], text: u8[:]) -> i64 {
    return hose_detokenize(cli_address(unsafe_c_slice_data<c_ptr<const void>>(ids)), ids.len,
        cli_address(unsafe_c_slice_data<c_ptr<const void>>(text)), text.len);
}

export fn fg_http_stats(stats: i64[:]) {
    let n = stats.len;
    if n >= 5 { hose_stats(cli_address(unsafe_c_address<c_ptr<const void>>(stats[0]))); }
    if n >= 6 { stats[5] = hose_context_capacity(); }
    if n >= 10 && hose_model_residency(cli_address(unsafe_c_address<c_ptr<const void>>(stats[6]))) != 0 { stats[6] = -1; }
    if n >= 11 { stats[10] = fg_cuda_dense_bytes(); }
    if n >= 12 { stats[11] = fg_cuda_expert_bytes(); }
    if n >= 13 { stats[12] = fg_cuda_model_state_bytes(); }
    if n >= 15 { hose_prefill_profile(cli_address(unsafe_c_address<c_ptr<const void>>(stats[13]))); }
    if n >= 21 { fg_cuda_expert_stats(stats[15 .. 21]); }
    if n >= 22 { stats[21] = hose_expert_cache_limit(); }
    if n >= 23 { stats[22] = fg_standalone_cpu_batch_threads(); }
    if n >= 24 { stats[23] = fg_standalone_cpu_decode_threads(); }
    if n >= 25 { stats[24] = hose_kv_format(); }
}

export fn fg_http_listening(port: i64) {
    cli_error(str("listening on 127.0.0.1:", port, "\n"));
    cli_flush(null);
}

fn cli_positive(text: const u8[:]) -> i64 {
    var at: i64 = 0;
    while at < text.len && (text[at] == 32 || (text[at] >= 9 && text[at] <= 13)) { at += 1; }
    if at < text.len && text[at] == 43 { at += 1; }
    var value: i64 = 0;
    while at < text.len {
        if text[at] < 48 || text[at] > 57 { return -1; }
        let digit = (text[at] as! i64) - 48;
        if value > (0x7fffffffffffffff - digit) / 10 { return -1; }
        value = value * 10 + digit;
        at += 1;
    }
    return if value > 0 { value } else { -1 };
}

fn cli_serve(model: u64, port: i64, limit: i64, capacity: i64) -> i32 {
    if port <= 0 || port > 65535 || limit < 0 {
        cli_error("invalid port or connection limit\n");
        return 2;
    }
    if !cli_network_ready() { return 1; }
    fg_goose_host_ensure_init();
    hose_host_load_context(model, capacity);
    let status = ih_serve(port, limit, cli_request, cli_output, cli_response, cli_ids);
    if status >= 0 { return 0; }
    cli_error(str("HTTP listener failed (", status, ")\n"));
    return 1;
}

fn cli_print_tokens(ids: u64, count: i64, initial_text: c_ptr<void>) -> i32 {
    var capacity: i64 = 256;
    var text = initial_text;
    var ok = true;
    for i in count {
        let token = ids + (i as! u64) * 8;
        var length = hose_detokenize(token, 1, cli_mut_address(text), capacity);
        while length < 0 {
            if capacity > 0x3fffffffffffffff { break; }
            let grown = cli_realloc(text, (capacity as! u64) * 2);
            if grown == null { break; }
            text = grown;
            capacity *= 2;
            length = hose_detokenize(token, 1, cli_mut_address(text), capacity);
        }
        if length < 0 { ok = false; break; }
        let address = cli_mut_address(text);
        for j in length {
            if cli_putchar(cli_byte(address + (j as! u64)) as! i32) < 0 { ok = false; break; }
        }
        if !ok { break; }
    }
    cli_free(text);
    if !ok || cli_putchar(10) < 0 || cli_flush(null) != 0 { return 1; }
    return 0;
}

fn cli_generate(model: u64, prompt: u64, context: i64, limit: i64) -> i32 {
    let ids = cli_malloc((limit as! u64) * 8);
    let text = cli_malloc(256);
    if ids == null || text == null {
        cli_free(text);
        cli_free(ids);
        return 1;
    }
    fg_goose_host_ensure_init();
    hose_host_load_context(model, context);
    let token_address = cli_mut_address(ids);
    var prompt_len: i64 = 0;
    while cli_byte(prompt + (prompt_len as! u64)) != 0 { prompt_len += 1; }
    let count = hose_generate(prompt, prompt_len, limit, token_address, 0, 0, 0);
    var result: i32 = 1;
    if count > 0 { result = cli_print_tokens(token_address, count, text); }
    else {
        cli_free(text);
        cli_error("inference failed\n");
    }
    cli_free(ids);
    return result;
}

export fn gooseinfer_cuda_entry(raw_argc: i32, argv: u64) -> i32 {
    let argc = raw_argc as! i64;
    var at: i64 = 1;
    var context: i64 = 2048;
    var batch: i64 = 128;
    var ubatch: i64 = 0;
    var batch_threads: i64 = 1;
    var threads: i64 = 1;
    var mask: u64 = 0;
    var batch_mask: u64 = 0;
    var strict = false;
    var batch_strict = false;
    while at < argc {
        let option = cli_text(cli_arg(argv, at));
        let value_addr: u64 = if at + 1 < argc { cli_arg(argv, at + 1) } else { 0 };
        if option == "--cache-type-k" || option == "--cache-type-v" {
            if value_addr == 0 || hose_configure_kv(option == "--cache-type-v", value_addr) != 0 {
                cli_error(str(option, " requires q4_0 or f16\n"));
                return 2;
            }
        } else if option == "--expert-cache-mib" {
            if value_addr == 0 || hose_configure_expert_cache(cli_positive(cli_text(value_addr))) != 0 {
                cli_error("--expert-cache-mib requires a positive MiB value that fits in signed 64-bit bytes\n");
                return 2;
            }
        } else if option == "--cpu-mask" || option == "--cpu-mask-batch" {
            if value_addr == 0 {
                cli_error(str(option, " requires a hexadecimal CPU mask\n"));
                return 2;
            }
            if option == "--cpu-mask" { mask = value_addr; } else { batch_mask = value_addr; }
        } else if option == "--cpu-strict" || option == "--cpu-strict-batch" {
            let value = cli_text(value_addr);
            if value != "0" && value != "1" {
                cli_error(str(option, " requires 0 or 1\n"));
                return 2;
            }
            if option == "--cpu-strict" { strict = value == "1"; } else { batch_strict = value == "1"; }
        } else if option == "--ctx-size" || option == "--batch-size" || option == "--ubatch-size" ||
            option == "--threads-batch" || option == "--threads" {
            let number = cli_positive(cli_text(value_addr));
            if number <= 0 {
                cli_error(str(option, " requires a positive integer\n"));
                return 2;
            }
            if option == "--ctx-size" { context = number; }
            else if option == "--batch-size" { batch = number; }
            else if option == "--ubatch-size" { ubatch = number; }
            else if option == "--threads-batch" { batch_threads = number; }
            else { threads = number; }
        } else { break; }
        at += 2;
    }

    if context > 2147483647 || context % 32 != 0 {
        cli_error("--ctx-size must be a positive multiple of 32 within the model context length\n");
        return 2;
    }
    if ubatch == 0 { ubatch = if batch < 128 { batch } else { 128 }; }
    if hose_configure_prefill(batch, ubatch) != 0 {
        cli_error("require 1 <= --ubatch-size <= --batch-size <= 4096\n");
        return 2;
    }
    if batch_threads > 1 && fg_standalone_configure_cpu_batch(batch_threads) != 0 {
        cli_error("--threads-batch exceeds the available Goose worker capacity\n");
        return 2;
    }
    if threads > 1 && fg_standalone_configure_cpu_decode(threads) != 0 {
        cli_error("--threads exceeds the available Goose worker capacity\n");
        return 2;
    }
    if mask != 0 && fg_standalone_configure_cpu_affinity(mask, strict, false) != 0 {
        cli_error("--cpu-mask requires a nonzero hexadecimal mask of allowed CPUs\n");
        return 2;
    }
    if batch_mask != 0 && fg_standalone_configure_cpu_affinity(batch_mask, batch_strict, true) != 0 {
        cli_error("--cpu-mask-batch requires a nonzero hexadecimal mask of allowed CPUs\n");
        return 2;
    }

    let remaining = argc - at;
    let server = remaining >= 2 && cli_text(cli_arg(argv, at + 1)) == "--serve";
    if server && (remaining == 3 || remaining == 4) {
        let port = cli_positive(cli_text(cli_arg(argv, at + 2)));
        let limit: i64 = if remaining == 4 { cli_positive(cli_text(cli_arg(argv, at + 3))) } else { 0 };
        return cli_serve(cli_arg(argv, at), port, limit, context);
    }
    if server || (remaining != 2 && remaining != 3) {
        let program = cli_text(cli_arg(argv, 0));
        cli_error("options: --ctx-size N --batch-size N --ubatch-size N --cache-type-k q4_0 --cache-type-v q4_0 --expert-cache-mib N\n");
        cli_error("         --threads N --threads-batch N --cpu-mask HEX --cpu-mask-batch HEX --cpu-strict 0|1 --cpu-strict-batch 0|1\n");
        cli_error(str("usage: ", program, " [OPTIONS] MODEL.gguf PROMPT [MAX_TOKENS]\n"));
        cli_error(str("       ", program, " [OPTIONS] MODEL.gguf --serve PORT [MAX_CONNECTIONS]\n"));
        return 2;
    }
    let limit: i64 = if remaining == 3 { cli_positive(cli_text(cli_arg(argv, at + 2))) } else { 1 };
    if limit <= 0 || (limit as! u64) > 0x1fffffffffffffff {
        cli_error("invalid maximum token count\n");
        return 2;
    }
    return cli_generate(cli_arg(argv, at), cli_arg(argv, at + 1), context, limit);
}
