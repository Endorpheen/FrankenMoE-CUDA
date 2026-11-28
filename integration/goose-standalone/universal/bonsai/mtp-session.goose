const MTP_PENDING: u64 = 24;
const MTP_VERIFY_ROWS: u64 = 52;
const MTP_DRAFT_CHAIN: u64 = 296;

extern fn bonsai_draft_forward(model: u64, token: u32, pos: u32, hidden: u64, logits: u64, device: i32) -> i32;
extern fn bonsai_choose_mtp_rows(model: u64, rows: u32, ignore_eos: i32, result: u64) -> i32;
extern fn bonsai_save_conditioning(dst: u64, src: u64) -> i32;
extern fn bonsai_is_eog(model: u64, token: u32) -> i32;
extern fn bonsai_draft_catchup(model: u64, ids: u64, rows: u32, already_written: u32) -> i32;

export fn bonsai_ffn_prefix_launch(context: u64, hidden: i64, branch: i64,
    weight: i64, signs: i64, residual: i64, owner: i64) -> i64 {
    return 0;
}

export fn bonsai_session_mtp_prepare(session: u64, allowed: i32) {
    if session != 0 && batch_u32(session + SESSION_NEEDS_RESET) == 0 {
        let model = vocab_word(session);
        batch_set(model + SESSION_MTP_REQUEST, if batch_u32(model + SESSION_MTP_ENABLED) != 0 && allowed != 0 { 1 } else { 0 });
    }
}

fn mtp_verify_block(model: u64, anchor: u32, ignore_eos: i32, rows: u32) -> i32 {
    var tokens: u32[5] = [0; 5];
    var choices: u32[15] = [0; 15];
    tokens[0] = anchor;
    let ids = vocab_address(unsafe_c_address<c_ptr<const void>>(tokens[0]));
    let result = vocab_address(unsafe_c_address<c_ptr<const void>>(choices[0]));
    let start = batch_u32(model + BATCH_POSITION);
    let chain = vocab_word(model + MTP_DRAFT_CHAIN);
    var index: u32 = 0;
    while index + 1 < rows {
        let hidden = if index == 0 { vocab_word(model + SESSION_DRAFT_CARRY) } else { chain };
        if bonsai_draft_forward(model, tokens[index as! i64], start + index, hidden, 0, 1) != 0 ||
            bonsai_choose_mtp_rows(model, 1, ignore_eos, result) != 0 || choices[2] != 0 || choices[0] >= 248320 { return -1; }
        tokens[(index + 1) as! i64] = choices[0];
        if index + 2 < rows && bonsai_save_conditioning(chain, vocab_word(model + BATCH_B + 8)) != 0 { return -1; }
        index += 1;
    }

    batch_set(model + BATCH_IN_DECODE, 1);
    let failed = evaluate_batch_output(model, ids, rows, 0, 1) != 0 ||
        bonsai_choose_mtp_rows(model, rows, ignore_eos, result) != 0;
    batch_set(model + BATCH_IN_DECODE, 0);
    if failed { return -1; }

    var kept: u32 = 1;
    var stop: bool = false;
    while kept < rows {
        let offset = ((kept - 1) as! i64) * 3;
        let choice = choices[offset];
        if choices[offset + 2] != 0 || choice >= 248320 { return -1; }
        if choice != tokens[kept as! i64] { break; }
        kept += 1;
        if ignore_eos == 0 && bonsai_is_eog(model, choice) != 0 {
            stop = true;
            break;
        }
    }
    let offset = ((kept - 1) as! i64) * 3;
    let next = choices[offset];
    if !stop && (choices[offset + 2] != 0 || next >= 248320) { return -1; }
    if bonsai_commit_target(model, kept) != 0 || (!stop && bonsai_draft_catchup(model, ids, kept, 1) != 0) { return -1; }
    let pending = model + MTP_PENDING;
    for accepted in (kept - 1) as! i64 {
        batch_set(pending + (accepted as! u64) * 4, tokens[accepted + 1]);
    }
    batch_set(model + SESSION_MTP_NEXT, 0);
    if !stop { batch_set(pending + ((kept - 1) as! u64) * 4, next); }
    batch_set(model + SESSION_MTP_END, if stop { kept - 1 } else { kept });
    vocab_log(str("[bonsai-mtp] proposed=", (rows - 1) as! i64, " accepted=", (kept - 1) as! i64, "\n"));
    return 0;
}

fn mtp_take_pending(model: u64, index: u32) -> i64 {
    batch_set(model + SESSION_MTP_NEXT, index + 1);
    return batch_u32(model + MTP_PENDING + (index as! u64) * 4) as! i64;
}

export fn bonsai_session_mtp_next(session: u64, token: u32, ignore_eos: i32, remaining: i64) -> i64 {
    if session == 0 || batch_u32(session + SESSION_NEEDS_RESET) != 0 || token >= 248320 || remaining <= 0 { return -1; }
    let model = vocab_word(session);
    let next = batch_u32(model + SESSION_MTP_NEXT);
    if next < batch_u32(model + SESSION_MTP_END) {
        if next == 0 || token != batch_u32(model + MTP_PENDING + ((next - 1) as! u64) * 4) { return -1; }
        return mtp_take_pending(model, next);
    }
    let rows = batch_u32(model + MTP_VERIFY_ROWS);
    if batch_u32(model + SESSION_MTP_REQUEST) == 0 || remaining < (rows as! i64) ||
        rows > batch_u32(model + BATCH_CONTEXT) - batch_u32(model + BATCH_POSITION) {
        return bonsai_session_choose_next(session, token, ignore_eos);
    }
    if session_bind(model, "bind Bonsai MTP context") != 0 { return -1; }
    let status = mtp_verify_block(model, token, ignore_eos, rows);
    if fg_bonsai_session_finish(session, status) != 0 { return -1; }
    return mtp_take_pending(model, batch_u32(model + SESSION_MTP_NEXT));
}
