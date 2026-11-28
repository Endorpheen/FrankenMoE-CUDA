extern "gs_os_arg_count" fn entry_arg_count() -> i64;
extern "gs_os_arg" fn entry_arg(index: i64, output: u8[>..]&);
extern "gs_os_write_stderr" fn entry_error(text: const u8[:]);
extern "memcpy" fn entry_copy(destination: c_ptr<void>, source: c_ptr<const void>, bytes: u64) -> c_ptr<void>;
extern "exit" fn entry_exit(status: i32);
extern fn fg_socket_init(argc: i32, argv: u64);
extern fn fg_common_memory_init() -> i32;
extern fn fg_graph_bindings_init(argc: i32, argv: u64);
extern fn goose_cuda_init(argc: i32, argv: u64);
extern fn gooseinfer_cuda_entry_init(argc: i32, argv: u64);
extern fn gooseinfer_cuda_entry(argc: i32, argv: u64) -> i32;
extern fn gooseinfer_vulkan_entry(argc: i32, argv: u64) -> i32;

fn entry_address(pointer: c_ptr<const void>) -> u64 {
    var bits: u64 = 0;
    entry_copy(unsafe_c_address<c_ptr<void>>(bits), unsafe_c_address<c_ptr<const void>>(pointer), 8);
    return bits;
}

fn main() {
    let argc = entry_arg_count();
    var arguments: u8[>..] = [];
    var offsets: i64[>..] = [];
    for i in argc {
        offsets.push(arguments.len);
        entry_arg(i, arguments);
        arguments.push(0);
    }

    var skip: i64 = 0;
    var vulkan = false;
    if argc > 1 {
        let start = offsets[1];
        let end = if argc > 2 { offsets[2] - 1 } else { arguments.len - 1 };
        if end - start >= 10 && arguments[start .. start + 10] == "--backend=" {
            let name = arguments[start + 10 .. end];
            vulkan = name == "vulkan";
            if !vulkan && name != "cuda" {
                entry_error(str("Unsupported backend '", name, "'. Choose cuda or vulkan.\n"));
                entry_exit(2);
                return;
            }
            skip = 1;
        }
    }

    var argv: u64[>..] = [];
    for i in argc {
        if i == 1 && skip != 0 { continue; }
        argv.push(entry_address(unsafe_c_address<c_ptr<const void>>(arguments[offsets[i]])));
    }
    argv.push(0);
    let address = entry_address(unsafe_c_address<c_ptr<const void>>(argv[0]));
    let count = (argc - skip) as! i32;
    if vulkan {
        entry_exit(gooseinfer_vulkan_entry(count, address));
        return;
    }
    fg_graph_bindings_init(count, address);
    if fg_common_memory_init() != 0 {
        entry_error("Could not initialize the GGML runtime lock.\n");
        entry_exit(1);
        return;
    }
    fg_socket_init(count, address);
    goose_cuda_init(count, address);
    gooseinfer_cuda_entry_init(count, address);
    let status = gooseinfer_cuda_entry(count, address);
    entry_exit(status);
}
