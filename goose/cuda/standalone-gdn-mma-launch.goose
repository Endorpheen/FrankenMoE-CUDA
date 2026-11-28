extern "fg174s_dget" fn gdn_mma_word(packet: i64, table: i64, index: i64) -> u64;
extern "fg174s_df" fn gdn_mma_scale(packet: i64, index: i64) -> f32;
fn gdn_mma_signed(packet: i64, index: i64) -> i64 { return gdn_mma_word(packet, 0, index) as i64; }
export fn gdn_mma_launch(packet: i64, shared: u64, task: i32, thread: i32, narrow: bool) {
    if narrow {
        gdn_v32(gdn_mma_word(packet, 0, 0), gdn_mma_word(packet, 0, 1), gdn_mma_word(packet, 0, 2),
        gdn_mma_word(packet, 0, 3), gdn_mma_word(packet, 0, 4), gdn_mma_word(packet, 0, 5),
        gdn_mma_word(packet, 0, 6), gdn_mma_word(packet, 0, 7), shared,
        gdn_mma_word(packet, 3, 1) as i32, gdn_mma_word(packet, 0, 52) as i32, gdn_mma_word(packet, 0, 55) as i32,
        gdn_mma_signed(packet, 34), gdn_mma_signed(packet, 9), gdn_mma_signed(packet, 10), gdn_mma_signed(packet, 11),
        gdn_mma_signed(packet, 17), gdn_mma_signed(packet, 18), gdn_mma_signed(packet, 19),
        gdn_mma_signed(packet, 21), gdn_mma_signed(packet, 22), gdn_mma_signed(packet, 23),
        thread % 32, thread / 32, task, gdn_mma_scale(packet, 0));
    } else {
        gdn_v128(gdn_mma_word(packet, 0, 0), gdn_mma_word(packet, 0, 1), gdn_mma_word(packet, 0, 2),
        gdn_mma_word(packet, 0, 3), gdn_mma_word(packet, 0, 4), gdn_mma_word(packet, 0, 5),
        gdn_mma_word(packet, 0, 6), gdn_mma_word(packet, 0, 7), shared,
        gdn_mma_word(packet, 3, 1) as i32, gdn_mma_word(packet, 0, 52) as i32, gdn_mma_word(packet, 0, 55) as i32,
        gdn_mma_signed(packet, 34), gdn_mma_signed(packet, 9), gdn_mma_signed(packet, 10), gdn_mma_signed(packet, 11),
        gdn_mma_signed(packet, 17), gdn_mma_signed(packet, 18), gdn_mma_signed(packet, 19),
        gdn_mma_signed(packet, 21), gdn_mma_signed(packet, 22), gdn_mma_signed(packet, 23),
        thread % 32, thread / 32, task, gdn_mma_scale(packet, 0));
    }
}
