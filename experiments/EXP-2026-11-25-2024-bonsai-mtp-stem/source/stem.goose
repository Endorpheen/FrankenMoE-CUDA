// The draft stem consumes the unnormalized hidden state and the next token.
extern "bonsai_mtp_eh" fn mtp_eh(context: u64, weight: i64, input: i64, output: i64) -> i64;

export fn bonsai_mtp_stem(c: u64, tensor: const i64[:], b: i64[:], embedding: i64, token: i64) -> i64 {
    if tensor.len != 4 || b.len != 24 || embedding <= 0 || token < 0 || token >= 248320 {
        return -1;
    }
    if b_launch(c, 12, embedding, b[1], token, 0, 0, 0, 0, 0) != 0 ||
       b_rotate(c, b[1], b[20], b[2], 5120, 1, 0) != 0 ||
       b_norm(c, b[2], tensor[1], 0, b[7], 5120, 1, 0) != 0 ||
       b_norm(c, b[0], tensor[2], 0, b[7] + 20480, 5120, 1, 0) != 0 {
        return -2;
    }
    return mtp_eh(c, tensor[0], b[7], b[12]);
}
