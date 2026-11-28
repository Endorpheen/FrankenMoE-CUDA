extern fn apex_mailbox_sync();
extern fn apex_mailbox_publish(ids: u64, mailbox: u64, sequence: u32);

export fn apex_route_mailbox(probabilities: u64, ids: u64, weights: u64, scale: f32,
    mailbox: u64, sequence: u32, lane: i32) {
    goose_compact_route(probabilities, ids, weights, scale, lane);
    apex_mailbox_sync();
    if lane == 0 { apex_mailbox_publish(ids, mailbox, sequence); }
}
