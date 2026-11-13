extern fn bridge_launch(kind: i32);

export fn goose_launch_loop(kind: i32, count: i64) {
    for i in count {
        bridge_launch(kind);
    }
}
