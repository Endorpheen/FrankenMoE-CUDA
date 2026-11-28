var i2_resident_plan: i32 = -1;

export fn fg_i2_host_resident_plan() -> bool {
    if i2_resident_plan < 0 {
        i2_resident_plan = if i2d_env_is("I2_RESIDENT_PLAN", 49) { 1 } else { 0 };
    }
    return i2_resident_plan != 0;
}

export fn fg_i2_host_resident_control() -> bool {
    return fg_i2_host_resident_plan() && !i2d_env_is("I2_RESIDENT_CONTROL", 48);
}

export fn fg_i2_resident_moe() -> bool {
    return fg_i2_host_resident_plan() && !i2d_env_is("I2_RESIDENT_MOE", 48);
}

export fn fg_i2_resident_attention_projection() -> bool {
    return !i2d_env_is("I2_ATTENTION_RESIDENT", 48);
}

export fn fg_i2_device_attention_input() -> bool {
    return fg_i2_resident_attention_projection() && !i2d_env_is("I2_ATTENTION_DEVICE_INPUT", 48);
}

export fn fg_i2_serial_graph_enabled() -> bool {
    return fg_i2_device_attention_input() && fg_i2_host_resident_plan() && !i2d_env_is("I2_ATTENTION_GRAPH", 48);
}

fn i2_joined_bridge_enabled() -> bool {
    return i2d_env_is("I2_JOIN_ATTENTION_MOE", 49);
}

fn fg_i2_backend_draft_features() -> bool {
    return fg_i2_serial_graph_enabled() && fg_i2_host_cached_history() && fg_i2_resident_moe() && i2_joined_bridge_enabled();
}
