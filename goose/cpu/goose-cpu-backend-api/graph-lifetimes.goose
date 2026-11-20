extern "fg143_storage_hash_size" fn storage_hash_size(graph: u64) -> i64;
extern "fg143_storage_slot" fn storage_slot(graph: u64, tensor: u64) -> i64;
extern "fg143_storage_ram_available" fn storage_ram_available() -> u64;
extern "fg143_storage_ram_error" fn storage_ram_error(required: u64, available: u64);
extern "fg143_storage_report" fn storage_report(bytes: u64, blocks: i64, reused: i64);

fn storage_keep(graph: u64, tensor: u64, until: i64, lifetimes: i64[:]) -> bool {
    if tensor == 0 { return true; }
    let parent = storage_get(tensor, STORAGE_VIEW, 0);
    let owner = if parent == 0 { tensor } else { parent };
    if storage_get(owner, STORAGE_DATA, 0) != 0 { return true; }
    let slot = storage_slot(graph, owner);
    if slot < 0 || slot >= lifetimes.len || lifetimes[slot] < 0 { return false; }
    lifetimes[slot] = high(lifetimes[slot], until);
    return true;
}

// A buffer can be reused only after the last node that reads any of its
// views. Outputs and inputs that meet in the same node use disjoint blocks.
export fn fg143_storage_allocate_output(graph: u64, output: u64, deadlines: i64[:]) -> u64 {
    if graph == 0 { return 0; }
    let count = storage_count(graph);
    let slots = storage_hash_size(graph);
    let leaves = count - graph_nodes(graph);
    if count < 0 || leaves < 0 || slots <= 0 || deadlines.len != count - leaves { return 0; }
    let align = storage_alignment();
    let mask = align - 1;
    let aligned = STORAGE_MAX - mask;
    var lifetimes: i64[>..] = [];
    var placements: u64[>..] = [];
    lifetimes.resize(slots, -1);
    placements.resize(slots, 0);
    for i in count {
        let t = storage_tensor(graph, i);
        if t == 0 { return 0; }
        let parent = storage_get(t, STORAGE_VIEW, 0);
        if parent != 0 {
            if !storage_view_valid(graph, t, parent, count) { return 0; }
        } else if storage_get(t, STORAGE_DATA, 0) == 0 {
            let slot = storage_slot(graph, t);
            if slot < 0 || slot >= slots { return 0; }
            lifetimes[slot] = i;
        }
    }
    var sources: u64[10] = [0; 10];
    for i in count {
        let t = storage_tensor(graph, i);
        if !storage_keep(graph, t, i, lifetimes[..]) { return 0; }
        if graph_prop(t, 2) == OP_NONE { continue; }
        graph_inputs(t, sources[..]);
        for j in 10 {
            if !storage_keep(graph, sources[j], i, lifetimes[..]) { return 0; }
        }
    }
    if !storage_keep(graph, output, count, lifetimes[..]) { return 0; }

    var block_size: u64[>..] = [];
    var block_offset: u64[>..] = [];
    var block_until: i64[>..] = [];
    block_size.resize(count, 0);
    block_offset.resize(count, 0);
    block_until.resize(count, 0);
    var blocks: i64 = 0;
    var reused: i64 = 0;
    var needed: u64 = 0;
    for i in count {
        let t = storage_tensor(graph, i);
        if storage_get(t, STORAGE_VIEW, 0) != 0 || storage_get(t, STORAGE_DATA, 0) != 0 { continue; }
        let bytes = storage_bytes(t);
        if bytes > aligned { return 0; }
        let padded = (bytes + mask) & aligned;
        var fit: i64 = -1;
        for j in blocks {
            if block_until[j] < i && block_size[j] >= padded &&
               (fit < 0 || block_size[j] < block_size[fit]) {
                fit = j;
                if block_size[j] == padded { break; }
            }
        }
        if fit < 0 {
            if needed > STORAGE_MAX - padded { return 0; }
            fit = blocks;
            blocks += 1;
            block_size[fit] = padded;
            block_offset[fit] = needed;
            needed += padded;
        } else { reused += 1; }
        let slot = storage_slot(graph, t);
        block_until[fit] = lifetimes[slot];
        placements[slot] = block_offset[fit];
    }
    let available = storage_ram_available();
    if available == 0 || needed > available {
        storage_ram_error(needed, available);
        return 0;
    }
    let storage = storage_alloc(if needed == 0 { align } else { needed });
    if storage == 0 {
        storage_ram_error(needed, available);
        return 0;
    }
    storage_report(needed, blocks, reused);
    for i in count {
        let t = storage_tensor(graph, i);
        if storage_get(t, STORAGE_VIEW, 0) == 0 && storage_get(t, STORAGE_DATA, 0) == 0 {
            let slot = storage_slot(graph, t);
            storage_set(t, STORAGE_DATA, 0, storage + placements[slot]);
            if i >= leaves { deadlines[i - leaves] = lifetimes[slot] - leaves; }
        }
    }
    for i in count {
        let t = storage_tensor(graph, i);
        let parent = storage_get(t, STORAGE_VIEW, 0);
        if parent != 0 && storage_get(t, STORAGE_DATA, 0) == 0 {
            storage_set(t, STORAGE_DATA, 0,
                storage_get(parent, STORAGE_DATA, 0) + storage_get(t, STORAGE_VIEW_OFFSET, 0));
        }
    }
    return storage;
}
