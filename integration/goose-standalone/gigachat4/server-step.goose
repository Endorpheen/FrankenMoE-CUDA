extern "fg143_storage_alloc" fn giga_server_alloc(bytes: u64) -> u64;

export fn giga_run_server_step(tokens_address: u64, tokens: const i32[:], rows: i64,
        logits: u64, logit_capacity: i64, draft_enabled: bool) -> i64 {
    let count = tokens.len;
    if tokens_address == 0 || count < 1 || count > 4096 || rows < -1 || rows > 1 ||
       (rows == 0 && logit_capacity != 0) { return -1; }
    let emit = if rows < 0 { count } else { rows };
    let values = emit * 128256;
    if emit > 0 && (logits == 0 || logit_capacity < values) { return -1; }
    for i in count {
        if tokens[i] < 0 || tokens[i] >= 128256 { return -1; }
    }

    let aligned = if draft_enabled { giga_server_alloc((count as u64) * 2560 * 4) } else { 0 };
    if draft_enabled && aligned == 0 { return -1; }
    var status = giga_server_execute(tokens_address, count, emit, logits, 0, aligned);
    if aligned != 0 {
        if status == 0 {
            status = giga_server_execute(tokens_address, count, 0, 0, aligned, 0);
        }
        giga_host_free(aligned);
    }
    if status != 0 {
        giga_server_reset();
        return status;
    }
    return values;
}
