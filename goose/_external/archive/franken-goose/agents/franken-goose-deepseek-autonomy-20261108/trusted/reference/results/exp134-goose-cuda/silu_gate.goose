extern fn expf(x: f32) -> f32;

export fn goose_silu_gate(gate: f32, up: f32) -> f32 {
    let one: f32 = 1.0;
    return (gate / (one + expf(-gate))) * up;
}
