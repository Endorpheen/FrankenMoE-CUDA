export fn gdn_launch(packet: i64, head: i32, sequence: i32, segment: i32, lane: i32, decode: bool) {
    if decode { gdn_decode(packet, head, sequence, segment, lane % 32, lane / 32); }
    else {
        let effective = gdn_packet_word(packet, 3, 11) as i32;
        gdn(packet, head, sequence, segment, lane % effective, lane / effective);
    }
}
