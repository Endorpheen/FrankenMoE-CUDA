const REPLAY_OP_LIMIT: i64 = 128;
const REPLAY_ARG_LIMIT: i64 = 32;
const REPLAY_BYTE_LIMIT: i64 = 65536;
const REPLAY_KERNEL: i64 = 0;
const REPLAY_HOST_COPY: i64 = 1;
const REPLAY_DEVICE_COPY: i64 = 2;
const REPLAY_WAIT: i64 = 3;
const REPLAY_WAIT_PLANES: i64 = 4;

// Copy fields are destination, source and size. Kernel fields are layout and
// argument start. Wait operations carry no comparison fields.
const REPLAY_KEY_WORDS: i64 = 4;

export fn fg_cuda_replay_same_region(previous: const u64[:], current: const u64[:]) -> bool {
    if previous.len != current.len || current.len % REPLAY_KEY_WORDS != 0 { return false; }
    let count = current.len / REPLAY_KEY_WORDS;
    if count > REPLAY_OP_LIMIT { return false; }
    for index in count {
        let at = index * REPLAY_KEY_WORDS;
        let kind = current[at] as! i64;
        if current[at] != previous[at] { return false; }
        if kind == REPLAY_WAIT || kind == REPLAY_WAIT_PLANES { continue; }
        if kind != REPLAY_KERNEL && kind != REPLAY_HOST_COPY && kind != REPLAY_DEVICE_COPY {
            return false;
        }
        if previous[at + 1] != current[at + 1] || previous[at + 2] != current[at + 2] {
            return false;
        }
        if kind != REPLAY_KERNEL && previous[at + 3] != current[at + 3] { return false; }
    }
    return true;
}

export fn fg_cuda_replay_packet_start(used: i64, operations: i64, bytes: i64,
        argument_offsets: const i64[:], argument_sizes: const i64[:],
        arguments: const u64[:]) -> i64 {
    if used < 0 || used > REPLAY_BYTE_LIMIT || operations < 0 || operations >= REPLAY_OP_LIMIT ||
        bytes < 0 || argument_offsets.len != argument_sizes.len ||
        argument_sizes.len != arguments.len || arguments.len > REPLAY_ARG_LIMIT { return -1; }

    let start = ((used + 15) / 16) * 16;
    if bytes > REPLAY_BYTE_LIMIT - start { return -1; }
    for index in arguments.len {
        let offset = argument_offsets[index];
        let size = argument_sizes[index];
        if arguments[index] == 0 || offset < 0 || offset > bytes ||
            size < 0 || size > bytes - offset { return -1; }
    }
    return start;
}

extern "fg143_copy_bytes" fn replay_copy_bytes(dst: u64, src: u64, count: i64);

// Layout is validated before any write, so a failed packet can fall back
// to immediate submission without changing the recording buffer.
export fn fg_cuda_replay_capture_packet(storage: u8[:], storage_address: u64,
        used: i64, operations: i64, bytes: i64, argument_offsets: const i64[:],
        argument_sizes: const i64[:], arguments: const u64[:]) -> i64 {
    let start = fg_cuda_replay_packet_start(used, operations, bytes,
        argument_offsets, argument_sizes, arguments);
    if start < 0 || start + bytes > storage.len || storage_address == 0 ||
        storage_address > 0xffffffffffffffff - ((start + bytes) as u64) { return -1; }
    for offset in used .. start + bytes { storage[offset] = 0; }
    for index in arguments.len {
        let count = argument_sizes[index];
        if count > 0 {
            let destination = storage_address + ((start + argument_offsets[index]) as u64);
            replay_copy_bytes(destination, arguments[index], count);
        }
    }
    return start;
}

export fn fg_cuda_replay_node_type(kind: i64) -> i32 {
    if kind == REPLAY_KERNEL { return 0; }
    if kind == REPLAY_HOST_COPY || kind == REPLAY_DEVICE_COPY { return 1; }
    if kind == REPLAY_WAIT || kind == REPLAY_WAIT_PLANES { return 6; }
    return -1;
}

// Each command stores the kind followed by its CUDA arguments. The caller
// keeps the pointer array and the kernel argument payload alive for capture.
const REPLAY_COMMAND_WORDS: i64 = 10;

export fn fg_cuda_replay_record(commands: const u64[:], stream: u64, uploads_ready: u64,
        planes_ready: u64, buffering: i64[:]) -> i32 {
    if buffering.len != 1 || commands.len % REPLAY_COMMAND_WORDS != 0 { return -1; }
    let count = commands.len / REPLAY_COMMAND_WORDS;
    if count > REPLAY_OP_LIMIT { return -1; }

    var error = fg_cuda_driver_graph_begin(stream, 1);
    if error != 0 { return error; }
    buffering[0] = 0;
    for index in count {
        let at = index * REPLAY_COMMAND_WORDS;
        let kind = commands[at] as! i64;
        if kind == REPLAY_KERNEL {
            error = fg_cuda_driver_launch(commands[at + 1],
                commands[at + 2] as u32, commands[at + 3] as u32, commands[at + 4] as u32,
                commands[at + 5] as u32, commands[at + 6] as u32, commands[at + 7] as u32,
                commands[at + 8] as u32, stream, commands[at + 9], 0);
        } else if kind == REPLAY_HOST_COPY {
            error = fg_cuda_driver_upload_async(commands[at + 1], commands[at + 2],
                commands[at + 3], stream);
        } else if kind == REPLAY_DEVICE_COPY {
            error = fg_cuda_driver_copy_device_async(commands[at + 1], commands[at + 2],
                commands[at + 3], stream);
        } else if kind == REPLAY_WAIT_PLANES || kind == REPLAY_WAIT {
            let event = if kind == REPLAY_WAIT_PLANES { planes_ready } else { uploads_ready };
            error = fg_cuda_driver_stream_wait_event(stream, event, 1);
        } else {
            return -1;
        }
        if error != 0 { return error; }
    }
    return 0;
}

// state[0] holds the cached executable. state[1] is 0 for a new executable,
// 1 for an in-place update, and 2 when ending capture failed.
export fn fg_cuda_replay_finish_capture(stream: u64, cache: bool, state: u64[:]) -> i32 {
    if state.len != 2 { return -1; }
    state[1] = 0;

    var graph_out: u64[1] = [0];
    let ended = fg_cuda_driver_graph_end_typed(unsafe_c_ptr<c_ptr<void>>(stream), unsafe_c_ptr<c_ptr<c_ptr<void>>>(unsafe_c_address<c_ptr<void>>(graph_out)));
    let graph = graph_out[0];
    if ended != 0 { state[1] = 2; return ended; }
    var executable: u64[1] = [0];
    if cache { executable[0] = state[0]; }
    if executable[0] != 0 {
        // CUgraphExecUpdateResultInfo contains two pointers after its 32-bit result.
        var result: u64[3] = [0, 0, 0];
        if fg_cuda_driver_graph_update_typed(unsafe_c_ptr<c_ptr<void>>(executable[0]), unsafe_c_ptr<c_ptr<void>>(graph), unsafe_c_address<c_ptr<void>>(result)) == 0 &&
            (result[0] & 0xffffffff) == 0 {
            state[1] = 1;
        } else {
            fg_cuda_driver_graph_exec_destroy(executable[0]);
            executable[0] = 0;
        }
    }
    var status: i32 = 0;
    if executable[0] == 0 {
        status = fg_cuda_driver_graph_instantiate_typed(unsafe_c_ptr<c_ptr<c_ptr<void>>>(unsafe_c_address<c_ptr<void>>(executable)), unsafe_c_ptr<c_ptr<void>>(graph), 0);
    }
    if status == 0 { status = fg_cuda_driver_graph_launch(executable[0], stream); }
    if cache {
        state[0] = executable[0];
    } else if executable[0] != 0 {
        fg_cuda_driver_graph_exec_destroy(executable[0]);
    }
    fg_cuda_driver_graph_destroy(graph);
    return status;
}

// The caller owns the node storage. A graph may enter the replay cache only
// when its CUDA nodes are a linear chain matching the submitted commands.
fn cuda_replay_validate_chain(graph: u64, commands: const u64[:], nodes: u64[:]) -> bool {
    if graph == 0 || commands.len % REPLAY_COMMAND_WORDS != 0 { return false; }
    let count = commands.len / REPLAY_COMMAND_WORDS;
    if count == 0 || count > REPLAY_OP_LIMIT || count > nodes.len { return false; }

    let native_graph = unsafe_c_ptr<c_ptr<void>>(graph);
    var node_count: u64 = 0;
    let size = unsafe_c_address<c_ptr<u64>>(node_count);
    if fg_cuda_driver_graph_roots_typed(native_graph, null, size) != 0 || node_count != 1 { return false; }
    var node: c_ptr<void> = null;
    let output = unsafe_c_address<c_ptr<c_ptr<void>>>(node);
    if fg_cuda_driver_graph_roots_typed(native_graph, output, size) != 0 { return false; }

    for i in count {
        let expected = fg_cuda_replay_node_type(commands[i * REPLAY_COMMAND_WORDS] as! i64);
        var kind: i32 = -1;
        if expected < 0 || node == null ||
            fg_cuda_driver_graph_node_type_typed(node, unsafe_c_address<c_ptr<i32>>(kind)) != 0 ||
            kind != expected { return false; }
        nodes[i] = fg_cuda_replay_node_address(node);
        node_count = 0;
        if fg_cuda_driver_graph_dependents_typed(node, null, size) != 0 { return false; }
        let children: u64 = if i + 1 < count { 1 } else { 0 };
        if node_count != children { return false; }
        if children != 0 && fg_cuda_driver_graph_dependents_typed(node, output, size) != 0 { return false; }
    }
    return true;
}

extern "fg_cuda_host_pointer_bits" fn fg_cuda_replay_node_address(node: c_ptr<void>) -> u64;

struct CudaReplayKernelNode {
    function: u64,
    grid_x: u32, grid_y: u32, grid_z: u32,
    block_x: u32, block_y: u32, block_z: u32, shared_bytes: u32,
    parameters: u64, extra: u64,
    kernel: u64, context: u64
}

fn cuda_replay_set_kernel(executable: u64, node: u64, commands: const u64[:], at: i64) -> i32 {
    let parameters = CudaReplayKernelNode {
        function: commands[at + 1],
        grid_x: commands[at + 2] as u32, grid_y: commands[at + 3] as u32, grid_z: commands[at + 4] as u32,
        block_x: commands[at + 5] as u32, block_y: commands[at + 6] as u32, block_z: commands[at + 7] as u32,
        shared_bytes: commands[at + 8] as u32, parameters: commands[at + 9],
        extra: 0, kernel: 0, context: 0
    };
    return fg_cuda_driver_graph_set_kernel_typed(unsafe_c_ptr<c_ptr<void>>(executable),
        unsafe_c_ptr<c_ptr<void>>(node), unsafe_c_address<c_ptr<const void>>(parameters));
}

// Each operation has a byte offset and span in the argument buffer. Shape
// fields can change without changing the replay key. A failed update lets the
// caller discard this executable and rebuild from the current recording.
export fn fg_cuda_replay_update(executable: u64, nodes: const u64[:],
        previous_keys: const u64[:], current_keys: const u64[:],
        previous: u64[:], current: const u64[:], offsets: const i64[:],
        old_arguments: u8[:], new_arguments: const u8[:]) -> i64 {
    if executable == 0 || previous.len != current.len || current.len % REPLAY_COMMAND_WORDS != 0 ||
        !fg_cuda_replay_same_region(previous_keys, current_keys) { return -1; }
    let count = current.len / REPLAY_COMMAND_WORDS;
    if count > REPLAY_OP_LIMIT || nodes.len < count || current_keys.len != count * REPLAY_KEY_WORDS ||
        offsets.len != 2 * count { return -1; }
    for i in count {
        let at = i * REPLAY_COMMAND_WORDS;
        if previous[at] != current[at] || current_keys[i * REPLAY_KEY_WORDS] != current[at] { return -1; }
        if current[at] != (REPLAY_KERNEL as! u64) { continue; }
        let start = offsets[2 * i];
        let bytes = offsets[2 * i + 1];
        if nodes[i] == 0 || start < 0 || bytes < 0 || start > old_arguments.len || start > new_arguments.len ||
            bytes > old_arguments.len - start || bytes > new_arguments.len - start { return -1; }
    }

    var changed: i64 = 0;
    for i in count {
        let at = i * REPLAY_COMMAND_WORDS;
        if current[at] != (REPLAY_KERNEL as! u64) { continue; }
        let start = offsets[2 * i];
        let bytes = offsets[2 * i + 1];
        var different = false;
        for field in 1 .. 9 {
            if previous[at + field] != current[at + field] { different = true; break; }
        }
        if !different {
            for j in start .. start + bytes {
                if old_arguments[j] != new_arguments[j] { different = true; break; }
            }
        }
        if !different { continue; }
        if cuda_replay_set_kernel(executable, nodes[i], current, at) != 0 { return -1; }
        for j in start .. start + bytes { old_arguments[j] = new_arguments[j]; }
        for field in REPLAY_COMMAND_WORDS { previous[at + field] = current[at + field]; }
        changed += 1;
    }
    return changed;
}

export fn fg_cuda_replay_clear_graph(handles: u64[:]) {
    if handles.len != 2 { return; }
    if handles[1] != 0 { fg_cuda_driver_graph_exec_destroy(handles[1]); }
    if handles[0] != 0 { fg_cuda_driver_graph_destroy(handles[0]); }
    handles[0] = 0;
    handles[1] = 0;
}

// handles retains a graph and its executable only if the graph is reusable.
// Launch failures keep newly allocated driver objects local to this call.
export fn fg_cuda_replay_rebuild(commands: const u64[:], stream: u64, uploads_ready: u64,
        planes_ready: u64, buffering: i64[:], cache: bool, nodes: u64[:], handles: u64[:]) -> i32 {
    if handles.len != 2 || handles[0] != 0 || handles[1] != 0 ||
        buffering.len != 1 || buffering[0] == 0 { return -1; }
    var graph: u64 = 0;
    var executable: u64 = 0;
    var status = fg_cuda_replay_record(commands, stream, uploads_ready, planes_ready, buffering);
    if buffering[0] == 0 {
        let ended = fg_cuda_driver_graph_end_typed(unsafe_c_ptr<c_ptr<void>>(stream),
            unsafe_c_ptr<c_ptr<c_ptr<void>>>(unsafe_c_address<c_ptr<void>>(graph)));
        if status == 0 { status = ended; }
    }
    if status == 0 {
        status = fg_cuda_driver_graph_instantiate_typed(
            unsafe_c_ptr<c_ptr<c_ptr<void>>>(unsafe_c_address<c_ptr<void>>(executable)),
            unsafe_c_ptr<c_ptr<void>>(graph), 0);
    }
    if status == 0 { status = fg_cuda_driver_graph_launch(executable, stream); }
    if status == 0 && cache && cuda_replay_validate_chain(graph, commands, nodes) {
        handles[0] = graph;
        handles[1] = executable;
    } else {
        if executable != 0 { fg_cuda_driver_graph_exec_destroy(executable); }
        if graph != 0 { fg_cuda_driver_graph_destroy(graph); }
    }
    return status;
}
