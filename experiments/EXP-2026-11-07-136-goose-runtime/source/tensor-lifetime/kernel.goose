struct NodeState {
    children: i32, views: i32, buffer_id: i32, pad0: u32, chunk_id: i32, pad1: u32, offset: u64, allocated: bool, pad2: u8[7]
}

fn goose_parent_done(state: NodeState &, is_view: bool) -> i32 {
    state.children -= 1;

    if state.children == 0 && state.views == 0 {
        if is_view {
            return 2;
        }

        if state.allocated {
            return 1;
        }
    }
    return 0;
}

fn goose_view_done(state: NodeState &) -> bool {
    state.views -= 1;
    return state.views == 0 && state.children == 0 && state.allocated;
}

fn main() {
    var parent: NodeState = default < NodeState > ();
    var owner: NodeState = default < NodeState > ();
    parent.children = 1;
    owner.views = 1;
    owner.allocated = true;
    assert(goose_parent_done(& parent, true) == 2);
    assert(goose_view_done(& owner));
    assert(parent.children == 0 && owner.views == 0);
}

