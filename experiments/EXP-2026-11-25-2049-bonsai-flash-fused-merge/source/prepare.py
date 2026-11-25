"""Generate a Bonsai Flash path with in-CTA ordered merging."""

from pathlib import Path
import sys


def replace(text, old, new):
    if text.count(old) != 1:
        raise ValueError(f"Expected one anchor: {old!r}")
    return text.replace(old, new)


def kernel(source):
    start = source.index("__kernel void bonsai_flash_kv_lean2(")
    end = source.index("\n}\n", start) + 3
    code = source[start:end]
    code = replace(code, "bonsai_flash_kv_lean2", "bonsai_flash_kv_fused_ordered")
    code = replace(code, "__global float *partial", "__global float *output")
    code = replace(code, """    const uint tile_base = flash_grid_y() * 2;
    if (tile_base * FLASH_KEYS >= first_visible + pair_last/6) return;""", "")
    code = replace(code, """    sync_threads();

    for (uint j = 0; j < 2; ++j) {
        const uint tile = tile_base + j, key_start = tile * FLASH_KEYS;
        if (tile >= tile_count || key_start >= first_visible + pair_last/6) break;
        const int live = key_start < first_visible + last_row/6;
        __global float *out = partial + ((ulong)group*tile_count + tile) * FLASH_ROWS * FLASH_PARTIAL;""",
        """    if (thread < FLASH_ROWS) {
        query_half[thread][128] = as_uint(neg_inf);
        query_half[thread][129] = 0;
    }
    sync_threads();

    float4 values_acc[8];
    #pragma unroll
    for (uint r = 0; r < 8; ++r) values_acc[r] = (float4)(0.0f);
    const uint visible_end = flash_min(key_count, first_visible + pair_last/6);
    for (uint tile = 0; tile < tile_count; ++tile) {
        const uint key_start = tile * FLASH_KEYS;
        if (key_start >= visible_end) break;""")
    code = replace(code, "    const uint last_row = flash_min(first_row+FLASH_ROWS, queries*6)-1;\n", "")
    code = replace(code, """            if (lane == 0 && live) {
                out[r*FLASH_PARTIAL] = mx;
                out[r*FLASH_PARTIAL+1] = sum;
            }""", """            if (lane == 0) {
                if (count != 0) {
                    const float prior = as_float(query_half[r][128]), seen = flash_max(prior,mx);
                    const float keep = flash_exp(prior-seen);
                    const float add = flash_exp(mx-seen);
                    query_half[r][128] = as_uint(seen);
                    query_half[r][129] = as_uint(as_float(query_half[r][129])*keep + sum*add);
                    query_half[r][130] = as_uint(keep);
                    query_half[r][131] = as_uint(add);
                }
            }""")
    start = code.index("        #pragma unroll\n        for (uint result = 0; result < (live")
    return code[:start] + """        const uint token0 = (first_row+lane_row)/6, token1 = (first_row+lane_row+8)/6;
        const int valid0 = token0 < queries && first_visible+token0 > key_start;
        const int valid1 = token1 < queries && first_visible+token1 > key_start;
        #pragma unroll
        for (uint result = 0; result < 8; ++result) {
            if (valid0) {
                values_acc[result].xy = values_acc[result].xy*as_float(query_half[lane_row][130]) + acc[result].xy*as_float(query_half[lane_row][131]);
            }
            if (valid1) {
                values_acc[result].zw = values_acc[result].zw*as_float(query_half[lane_row+8][130]) + acc[result].zw*as_float(query_half[lane_row+8][131]);
            }
        }
        sync_threads();
    }

    if (thread < FLASH_ROWS) {
        float inv;
        __asm__("rcp.approx.ftz.f32 %0, %1;" : "=f"(inv) : "f"(as_float(query_half[thread][129])));
        query_half[thread][129] = as_uint(inv);
    }
    sync_threads();
    const uint row0 = first_row+lane_row, row1 = row0+8;
    const ulong offset0 = ((ulong)(row0/6)*24 + kv_head*6 + row0%6)*256;
    const ulong offset1 = ((ulong)(row1/6)*24 + kv_head*6 + row1%6)*256;
    #pragma unroll
    for (uint result = 0; result < 8; ++result) {
        const uint col = 32*result + 8*warp + 2*lane_pair;
        if (row0/6 < queries)
            *(__global float2 *)(output+offset0+col) = values_acc[result].xy*as_float(query_half[lane_row][129]);
        if (row1/6 < queries)
            *(__global float2 *)(output+offset1+col) = values_acc[result].zw*as_float(query_half[lane_row+8][129]);
    }
}
"""


def host_variant(host):
    host = replace(host, "MMQ_CACHE128, KERNEL_COUNT", "MMQ_CACHE128, FLASH_FUSED_ORDERED, KERNEL_COUNT")
    host = replace(host, "flash_panel, mmq_cached;", "flash_panel, mmq_cached, flash_fused_ordered;")
    host = replace(host, '"bonsai_pq2_cached_64x128_k64",',
                   '"bonsai_pq2_cached_64x128_k64", "bonsai_flash_kv_fused_ordered",')
    host = replace(host, "    if (m->flash_panel) {\n        m->flash_panel =",
                   '''    const char *fused = getenv("BONSAI_FLASH_FUSED_ORDERED");
    m->flash_fused_ordered = (!fused || strcmp(fused, "0") != 0) &&
        bonsai_cuda.fn_cuFuncSetAttribute(m->kernel[FLASH_FUSED_ORDERED],
            CU_FUNC_ATTRIBUTE_MAX_DYNAMIC_SHARED_SIZE_BYTES, 50176) == CUDA_SUCCESS;
    if (m->flash_panel) {
        m->flash_panel =''')
    return replace(host, "    for (unsigned start = 0; start < groups; start += wave) {",
                   '''    if (panel && ordered && m->flash_fused_ordered && count >= 512) {
        return check(bonsai_cuda.fn_cuLaunchKernel(m->kernel[FLASH_FUSED_ORDERED],
            groups/2, 1, 1, 256, 1, 1, 50176, NULL,
            (void *[]){&q,&m->stage_k,&m->stage_v,&dst,&count,&keys,&first,&tiles,&(unsigned){0},&chunks},
            NULL), "Bonsai ordered fused Flash");
    }
    for (unsigned start = 0; start < groups; start += wave) {''')


if __name__ == "__main__":
    image, backend, out = map(Path, sys.argv[1:])
    source = image.read_text()
    (out / "pipeline.cl").write_text(source + "\n" + kernel(source))
    (out / "backend.c").write_text(host_variant(backend.read_text()))
