__kernel void bonsai_pack_query_panels(__global const float *input, __global uint *output,
    uint queries, uint groups_per_head) {
    const uint group = work_group(), kv_head = group / groups_per_head;
    const uint first_row = (group % groups_per_head) * 16;
    for (uint index = local_id(); index < 2048; index += 256) {
        const uint row = first_row + index / 128, token = row / 6;
        uint packed = 0;
        if (token < queries) {
            const size_t offset = ((size_t)token * 24 + kv_head * 6 + row % 6) * 256 + 2 * (index % 128);
            packed = bonsai_half_pair(input[offset], input[offset + 1]);
        }
        output[(size_t)group * 2048 + index] = packed;
    }
}
