// The frozen MMA kernel consumes eight query heads per KV head. Keep the
// six real heads in their original order and fill the two unused slots.
__kernel void bonsai_pad_query(__global const float *query, __global float *padded) {
    const uint warp_head = work_group();
    const uint col = local_id();
    const uint group = warp_head / 8, head = warp_head % 8;
    padded[warp_head * 256 + col] = head < 6 ? query[(group * 6 + head) * 256 + col] : 0.0f;
}

__kernel void bonsai_unpad_attention(__global const float *padded, __global float *result) {
    const uint head = work_group();
    const uint col = local_id();
    const uint padded_head = (head / 6) * 8 + head % 6;
    result[head * 256 + col] = padded[padded_head * 256 + col];
}
