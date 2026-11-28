export fn fg143_graph_observe(address: u64) -> bool { return false; }
export fn fg_q35_checkpoint_enabled() -> bool { return false; }
export fn fg_q35_checkpoint_compute(plan: u64, frames: const i64[:], layers: const i64[:],
        prefix: i64, tokens: i64) -> i32 {
    return fg_standalone_graph_plan_compute(plan);
}
