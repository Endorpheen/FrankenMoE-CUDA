struct NodeState {
    children: i32, views: i32, buffer_id: i32, pad0: u32,
    chunk_id: i32, pad1: u32, offset: u64, allocated: bool, pad2: u8[7]
}
extern "fg_graph_hash" fn hash_state(allocator: u64, tensor: u64) -> NodeState&;
extern "fg_graph_source" fn source_of(tensor: u64, index: i64) -> u64;
extern "fg_graph_owner" fn owner_of(tensor: u64) -> u64;
extern "fg_graph_is_view" fn is_view(tensor: u64) -> bool;
extern "fg_graph_flags" fn flags_of(tensor: u64) -> u32;
extern "fg_graph_op" fn op_of(tensor: u64) -> i32;
extern "fg_graph_allocate" fn allocate_tensor(allocator: u64, tensor: u64, buffer_id: i32);
extern "fg_graph_release" fn release_tensor(allocator: u64, tensor: u64);
extern "fg_graph_verify" fn verify_transition(state: NodeState&, old_children: i32, old_views: i32, old_allocated: bool, kind: i32, action: i32, view_kind: bool);
fn choose_buffer(ids: const i32[:], index: i64) -> i32 {
    if ids.len == 0 { return 0; }
    return ids[index];
}
fn add_reference(allocator: u64, tensor: u64, view_kind: bool, verify: bool) {
    let state: NodeState& = hash_state(allocator, tensor);
    let children = state.children;
    let views = state.views;
    let allocated = state.allocated;
    if view_kind { state.views += 1; } else { state.children += 1; }
    if verify { verify_transition(state, children, views, allocated, (if view_kind { 0 } else { 1 }) as i32, 0, view_kind); }
}
fn finish_parent(allocator: u64, tensor: u64, verify: bool) -> i32 {
    let state: NodeState& = hash_state(allocator, tensor);
    let children = state.children;
    let views = state.views;
    let allocated = state.allocated;
    let view_kind = is_view(tensor);
    state.children -= 1;
    var action: i32 = 0;
    if state.children == 0 && state.views == 0 {
        if view_kind { action = 2; } else if state.allocated { action = 1; }
    }
    if verify { verify_transition(state, children, views, allocated, 2, action, view_kind); }
    return action;
}
fn finish_view(allocator: u64, tensor: u64, verify: bool) -> bool {
    let state: NodeState& = hash_state(allocator, tensor);
    let children = state.children;
    let views = state.views;
    let allocated = state.allocated;
    state.views -= 1;
    let release = state.views == 0 && state.children == 0 && state.allocated;
    if verify { verify_transition(state, children, views, allocated, 3, (if release { 1 } else { 0 }) as i32, true); }
    return release;
}
fn goose_graph_plan(allocator: u64, nodes: const u64[:], leafs: const u64[:], node_buffers: const i32[:], leaf_buffers: const i32[:], max_sources: i64, input_flag: u32, op_none: i32, verify: bool) {
    assert(max_sources >= 0);
    assert(node_buffers.len == 0 || node_buffers.len >= nodes.len);
    assert(leaf_buffers.len == 0 || leaf_buffers.len >= leafs.len);
    for i in leafs.len { allocate_tensor(allocator, leafs[i], choose_buffer(leaf_buffers, i)); }
    for i in nodes.len {
        let tensor = nodes[i];
        let buffer_id = choose_buffer(node_buffers, i);
        if is_view(tensor) && op_of(tensor) != op_none { add_reference(allocator, owner_of(tensor), true, verify); }
        if (flags_of(tensor) & input_flag) != 0 { allocate_tensor(allocator, tensor, buffer_id); }
        for j in max_sources {
            let parent = source_of(tensor, j);
            if parent != 0 {
                add_reference(allocator, parent, false, verify);
                if (flags_of(parent) & input_flag) != 0 { allocate_tensor(allocator, parent, buffer_id); }
            }
        }
    }
    for i in nodes.len {
        let tensor = nodes[i];
        let buffer_id = choose_buffer(node_buffers, i);
        for j in max_sources {
            let parent = source_of(tensor, j);
            if parent != 0 { allocate_tensor(allocator, parent, buffer_id); }
        }
        allocate_tensor(allocator, tensor, buffer_id);
        for j in max_sources {
            let parent = source_of(tensor, j);
            if parent != 0 {
                let action = finish_parent(allocator, parent, verify);
                if action == 2 {
                    let owner = owner_of(parent);
                    if finish_view(allocator, owner, verify) { release_tensor(allocator, owner); }
                } else if action == 1 { release_tensor(allocator, parent); }
            }
        }
    }
}
fn main() {
    let handles: u64[1] = default<u64[1]>();
    let buffers: i32[1] = default<i32[1]>();
    goose_graph_plan(0, handles[..0], handles[..0], buffers[..0], buffers[..0], 0, 1, 0, false);
}
