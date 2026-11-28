type HostIqLookup = c_fnptr(u64, i64, i64) -> i64;


fn host_iq_lookup(slot: u64, label: const u8[:], group: i64, index: i64) -> i64 {
    let descriptor = fg_goose_host_iq_tables_pointer();
    if descriptor == 0 {
        host_quant_error(str("fg-goose: IQ tables (", label, ") requested before a provider was bound; ",
            "call fg_goose_host_bind_iq_tables() or link ggml-base\n"));
        host_quant_abort();
        return 0;
    }
    let context = builder_word((descriptor + 8) as! i64) as! u64;
    let target = builder_word((descriptor + slot) as! i64) as! u64;
    return unsafe_c_fnptr<HostIqLookup>(target)(context, group, index);
}

export fn qd_iq2_grid(group: i64, index: i64) -> i64 {
    return host_iq_lookup(16, "qd_iq2_grid", group, index);
}

export fn qd_iq3_grid(group: i64, index: i64) -> i64 {
    return host_iq_lookup(24, "qd_iq3_grid", group, index);
}

export fn qz2_iq2_map(group: i64, index: i64) -> i64 {
    return host_iq_lookup(32, "qz2_iq2_map", group, index);
}

export fn qz2_iq2_nb(group: i64, index: i64) -> i64 {
    return host_iq_lookup(40, "qz2_iq2_nb", group, index);
}

export fn qz2_iq2_qgrid(group: i64, index: i64) -> i64 {
    return host_iq_lookup(16, "qz2_iq2_qgrid", group, index);
}

export fn qz2_iq3_map(group: i64, index: i64) -> i64 {
    return host_iq_lookup(48, "qz2_iq3_map", group, index);
}

export fn qz2_iq3_nb(group: i64, index: i64) -> i64 {
    return host_iq_lookup(56, "qz2_iq3_nb", group, index);
}

export fn qz2_iq3_qgrid(group: i64, index: i64) -> i64 {
    return host_iq_lookup(24, "qz2_iq3_qgrid", group, index);
}

export fn qz_iq2_map(group: i64, index: i64) -> i64 {
    return host_iq_lookup(32, "qz_iq2_map", group, index);
}

export fn qz_iq2_nb(group: i64, index: i64) -> i64 {
    return host_iq_lookup(40, "qz_iq2_nb", group, index);
}

export fn qz_iq2_qgrid(group: i64, index: i64) -> i64 {
    return host_iq_lookup(16, "qz_iq2_qgrid", group, index);
}

export fn qz_iq3_map(group: i64, index: i64) -> i64 {
    return host_iq_lookup(48, "qz_iq3_map", group, index);
}

export fn qz_iq3_nb(group: i64, index: i64) -> i64 {
    return host_iq_lookup(56, "qz_iq3_nb", group, index);
}

export fn qz_iq3_qgrid(group: i64, index: i64) -> i64 {
    return host_iq_lookup(24, "qz_iq3_qgrid", group, index);
}

fn host_store_i64(base: u64, offset: u64, value: i64) {
    host_quant_copy(unsafe_c_ptr<c_ptr<void>>(base + offset), unsafe_c_address<c_ptr<const void>>(value), 8);
}

export fn fg457_ld_i64(base: u64, offset: u64) -> i64 {
    return builder_word((base + offset) as! i64);
}

export fn fg446_ld_u8(base: u64, offset: u64) -> u32 {
    return host_quant_load_u8(base + offset) as! u32;
}

export fn fg200g_ld_u32(base: u64, index: i64) -> i64 {
    let address = base + ((index as! u64) * 4);
    return ((builder_i32(address as! i64) as! u32) as! i64);
}

export fn fg446_st_i64(base: u64, offset: u64, value: i64) { host_store_i64(base, offset, value); }
export fn fg202t_st_i64(base: u64, offset: u64, value: i64) { host_store_i64(base, offset, value); }
export fn fg457_st_i64(base: u64, offset: u64, value: i64) { host_store_i64(base, offset, value); }
export fn fg458_st_i64(base: u64, offset: u64, value: i64) { host_store_i64(base, offset, value); }
export fn fg448_st_i64(base: u64, offset: u64, value: i64) { host_store_i64(base, offset, value); }
