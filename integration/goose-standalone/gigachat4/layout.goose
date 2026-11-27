fn giga_int(data: const u8[:], state: i64[:], kv: i64[:], slots: i64[:], key: const u8[:]) -> i64 {
    var value: i64[1] = [0; 1];
    let index = gr_find_kv(data, state, kv, slots, key);
    if gr_meta_int(data, state, kv, index, value) != 0 { return -1; }
    return value[0];
}

export fn giga_read_layout(data: const u8[:], state: i64[:], kv: i64[:],
                           slots: i64[:], info: i64[:], kinds: i64[:]) -> i64 {
    if info.len < 12 { return -1; }
    var span: i64[4] = [0; 4];
    let arch = gr_find_kv(data, state, kv, slots, "general.architecture");
    if gr_val_span(state, kv, arch, span) != 0 || span[0] != 8 || span[2] != 9 ||
       data[span[1] .. span[1] + span[2]] != "gigachat4" { return -1; }

    let layers = giga_int(data, state, kv, slots, "gigachat4.block_count");
    let nextn = giga_int(data, state, kv, slots, "gigachat4.nextn_predict_layers");
    let width = giga_int(data, state, kv, slots, "gigachat4.embedding_length");
    let experts = giga_int(data, state, kv, slots, "gigachat4.expert_count");
    let active = giga_int(data, state, kv, slots, "gigachat4.expert_used_count");
    let inner = giga_int(data, state, kv, slots, "gigachat4.ssm.inner_size");
    let heads = giga_int(data, state, kv, slots, "gigachat4.ssm.time_step_rank");
    let groups = giga_int(data, state, kv, slots, "gigachat4.ssm.group_count");
    let dim = giga_int(data, state, kv, slots, "gigachat4.ssm.state_size");
    let kernel = giga_int(data, state, kv, slots, "gigachat4.ssm.conv_kernel");
    let top = giga_int(data, state, kv, slots, "gigachat4.attention.indexer.top_k");
    if layers != 33 || nextn != 1 || kinds.len < layers || width != 2560 ||
       experts != 192 || active != 8 || inner != 4096 || heads != 32 ||
       groups != 16 || dim != 128 || kernel != 4 || top != 2048 { return -2; }

    let trunk = layers - nextn;
    let recurrent = gr_find_kv(data, state, kv, slots, "gigachat4.attention.recurrent_layers");
    let indexer = gr_find_kv(data, state, kv, slots, "gigachat4.attention.indexer.types");
    if gr_arr_type(state, kv, recurrent) != 7 || gr_arr_n(state, kv, recurrent) != layers ||
       gr_arr_type(state, kv, indexer) != 7 || gr_arr_n(state, kv, indexer) != trunk { return -3; }
    var value: i64[1] = [0; 1];
    var n_gdn: i64 = 0;
    var n_dsa: i64 = 0;
    for layer in trunk {
        if gr_array_int(data, state, kv, recurrent, layer, value) != 0 { return -3; }
        let is_gdn = value[0];
        if gr_array_int(data, state, kv, indexer, layer, value) != 0 || is_gdn + value[0] != 1 { return -3; }
        kinds[layer] = if is_gdn == 1 { 0 } else { 1 };
        n_gdn += is_gdn;
        n_dsa += value[0];
    }
    if n_gdn != 24 || n_dsa != 8 ||
       gr_array_int(data, state, kv, recurrent, trunk, value) != 0 || value[0] != 0 { return -3; }
    kinds[trunk] = 2;
    info[0] = trunk;
    info[1] = nextn;
    info[2] = n_gdn;
    info[3] = n_dsa;
    info[4] = width;
    info[5] = experts;
    info[6] = active;
    info[7] = top;
    info[8] = inner;
    info[9] = heads;
    info[10] = dim;
    info[11] = (kernel - 1) * (inner + 2 * groups * dim);
    return 0;
}
