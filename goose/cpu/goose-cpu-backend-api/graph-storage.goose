extern "fg143_storage_count" fn storage_count(graph: u64) -> i64;
extern "fg143_storage_tensor" fn storage_tensor(graph: u64, index: i64) -> u64;
extern "fg143_storage_contains" fn storage_contains(graph: u64, tensor: u64) -> i64;
extern "ggml_nbytes" fn storage_bytes(tensor: u64) -> u64;
extern "fg156_tget" fn storage_get(tensor: u64, field: i64, axis: i64) -> u64;
extern "fg156_tset" fn storage_set(tensor: u64, field: i64, axis: i64, value: u64);
extern "fg143_storage_alignment" fn storage_alignment() -> u64;
extern "fg143_storage_alloc" fn storage_alloc(bytes: u64) -> u64;

const STORAGE_VIEW: i64 = 2;
const STORAGE_VIEW_OFFSET: i64 = 3;
const STORAGE_DATA: i64 = 4;
const STORAGE_MAX: u64 = 18446744073709551615;

fn storage_member(graph: u64, tensor: u64, count: i64) -> bool {
    let indexed = storage_contains(graph, tensor);
    if indexed >= 0 { return indexed != 0; }
    // A GGML subgraph borrows the original hash, including tensors outside
    // the selected range. Check the actual members in this case.
    for i in count {
        if storage_tensor(graph, i) == tensor { return true; }
    }
    return false;
}

fn storage_view_valid(graph: u64, tensor: u64, parent: u64, count: i64) -> bool {
    let parent_bytes = storage_bytes(parent);
    let at = storage_get(tensor, STORAGE_VIEW_OFFSET, 0);
    if at > parent_bytes || storage_bytes(tensor) > parent_bytes - at { return false; }
    if storage_get(tensor, STORAGE_DATA, 0) != 0 { return true; }
    let base = storage_get(parent, STORAGE_DATA, 0);
    if base != 0 { return base <= STORAGE_MAX - at; }
    // GGML flattens views to their base when constructing the tensor.
    return storage_get(parent, STORAGE_VIEW, 0) == 0 && storage_member(graph, parent, count);
}

export fn fg143_storage_allocate(graph: u64) -> u64 {
    if graph == 0 { return 0; }
    let count = storage_count(graph);
    if count < 0 { return 0; }
    let align = storage_alignment();
    let mask = align - 1;
    let aligned = STORAGE_MAX - mask;
    var needed: u64 = 0;
    for i in count {
        let t = storage_tensor(graph, i);
        if t == 0 { return 0; }
        let parent = storage_get(t, STORAGE_VIEW, 0);
        if parent != 0 {
            if !storage_view_valid(graph, t, parent, count) { return 0; }
        } else if storage_get(t, STORAGE_DATA, 0) == 0 {
            let bytes = storage_bytes(t);
            if bytes > aligned { return 0; }
            let padded = (bytes + mask) & aligned;
            if needed > STORAGE_MAX - padded { return 0; }
            needed += padded;
        }
    }
    let storage = storage_alloc(if needed == 0 { align } else { needed });
    if storage == 0 { return 0; }
    var cursor: u64 = 0;
    for i in count {
        let t = storage_tensor(graph, i);
        if storage_get(t, STORAGE_VIEW, 0) == 0 && storage_get(t, STORAGE_DATA, 0) == 0 {
            let bytes = storage_bytes(t);
            storage_set(t, STORAGE_DATA, 0, storage + cursor);
            cursor += (bytes + mask) & aligned;
        }
    }
    for i in count {
        let t = storage_tensor(graph, i);
        let parent = storage_get(t, STORAGE_VIEW, 0);
        if parent != 0 && storage_get(t, STORAGE_DATA, 0) == 0 {
            let base = storage_get(parent, STORAGE_DATA, 0);
            storage_set(t, STORAGE_DATA, 0, base + storage_get(t, STORAGE_VIEW_OFFSET, 0));
        }
    }
    return storage;
}
