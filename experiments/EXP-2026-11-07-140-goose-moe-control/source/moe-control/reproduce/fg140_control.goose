struct Row {
    slot: i32, token: i32
}
struct Plan {
    context: i64, mode: i64, ith: i64, nth: i64, experts: i64, real_experts: i64, ids_per_token: i64, tokens: i64, need_quant: i64, block_size: i64, block_bytes: i64, ne10: i64, ne11: i64, ne12: i64, ne13: i64, nb10: i64, nb11: i64, nb12: i64, nb13: i64, output_rows: i64, row_size: i64, ids_stride0: i64, ids_stride1: i64, source_contiguous: i64, is_iq2s: i64, fused_available: i64, disable_chunking: i64, tiled_bytes: i64, cache_line: i64, wdata_mod: i64, wdata_bytes: i64, dst_bytes: i64, counts_offset: i64, rows_offset: i64, chunks_offset: i64, scratch_offset: i64, total_bytes: i64, apex_math: i64, weight_row_bytes: i64, nb01: i64, nb02: i64, dst_nb1: i64, dst_nb2: i64
}
extern "fg140_quantize" fn quantize(context: i64, source: i64, target: i64, values: i64);
extern "fg140_reset" fn reset(context: i64, expert: i64, value: i32);
extern "fg140_barrier" fn barrier(context: i64);
extern "fg140_ready" fn ready(context: i64, expert: i64);
extern "fg140_tiled" fn tiled(context: i64, expert: i64, rows: i64) -> bool;
extern "fg140_chunk" fn chunk(context: i64, expert: i64, first: i64, last: i64, cfirst: i64, clast: i64, fused: i64);
extern "fg140_matrix" fn matrix(context: i64, weight: i64, input: i64, output: i64, width: i64, rows: i64);
extern "fg140_claim" fn claim(context: i64, expert: i64) -> i32;
extern "fg140_iq_stats" fn stats(context: i64, index: i64);
extern "fg140_witness" fn witness(context: i64, kind: i64, value: i64);

fn minimum(a: i64, b: i64) -> i64 {
    return if a < b {
        a
    } else {
        b
    }
    ;
}

fn add_size(a: i64, b: i64) -> i64 {
    assert(a >= 0 && b >= 0 && a <= 0x7fffffffffffffff - b);
    return a + b;
}

fn mul_size(a: i64, b: i64) -> i64 {
    assert(a >= 0 && b >= 0);
    assert(a == 0 || b <= 0x7fffffffffffffff / a);
    return a * b;
}

fn align_at(p: const Plan &, offset: i64, align: i64) -> i64 {
    assert(align > 0 && (align & (align - 1)) == 0);
    let absolute = add_size(p.wdata_mod, offset);
    let padded = add_size(absolute, align - 1);
    return (padded & (-align)) - p.wdata_mod;
}

fn layout(p: Plan &) {
    assert(p.mode >= 0 && p.mode <= 2 && p.nth > 0 && p.ith >= 0 && p.ith < p.nth);
    assert(p.cache_line > 0 && (p.cache_line & (p.cache_line - 1)) == 0);
    let highest = if p.cache_line > 64 {
        p.cache_line
    } else {
        64
    }
    ;
    assert(p.wdata_mod >= 0 && p.wdata_mod < highest && p.wdata_bytes >= 0);
    assert(p.experts >= 0 && p.real_experts >= 0 && p.ids_per_token >= 0 && p.tokens >= 0);
    assert(p.ne10 > 0 && p.ne11 > 0 && p.ne12 >= 0 && p.ne13 >= 0 && p.output_rows > 0);
    assert(p.block_size > 0 && p.block_bytes > 0 && p.ne10 % p.block_size == 0 && p.row_size >= 0);
    let capacity = mul_size(p.ids_per_token, p.tokens);
    var cursor: i64 = 0;

    if p.need_quant != 0 {
        cursor = add_size(align_at(p, cursor, 8), mul_size(mul_size(mul_size(p.row_size, p.ne11), p.ne12), p.ne13));
    }
    p.counts_offset = align_at(p, cursor, 8);
    cursor = add_size(p.counts_offset, mul_size(p.experts, 8));
    p.rows_offset = align_at(p, cursor, 8);
    cursor = add_size(p.rows_offset, mul_size(mul_size(p.experts, capacity), 8));
    p.chunks_offset = align_at(p, cursor, p.cache_line);
    cursor = add_size(p.chunks_offset, mul_size(p.cache_line, p.experts));
    p.scratch_offset = cursor;

    if p.mode == 0 {
        p.scratch_offset = align_at(p, cursor, 64);
        cursor = add_size(p.scratch_offset, p.tiled_bytes);
    }
    p.total_bytes = cursor;
    assert(cursor <= p.wdata_bytes);
}

fn apex_chunk(p: const Plan &, expert: i64, first: i64, last: i64, cfirst: i64, clast: i64, rows: const Row[:]) {
    let capacity = p.ids_per_token * p.tokens;
    var block1 = cfirst;

    while block1 < clast {
        var block0 = first;

        while block0 < last {
            var col = block1;

            while col < minimum(block1 + 16, clast) {
                let route = rows[expert * capacity + col];
                let slot = route.slot as i64;
                let token = route.token as i64;
                let input_slot = slot % p.ne11;
                let input = if p.source_contiguous != 0 || p.need_quant != 0 {
                    (input_slot + token * p.ne11) * p.row_size
                } else {
                    input_slot * p.nb11 + token * p.nb12
                }
                ;
                let output = slot * p.dst_nb1 + token * p.dst_nb2 + block0 * 4;
                let weight = expert * p.nb02 + block0 * p.nb01;
                let count = minimum(block0 + 16, last) - block0;

                if p.nb01 == p.weight_row_bytes {
                    matrix(p.context, weight, input, output, p.ne10, count);
                } else {
                    for k in count {
                        matrix(p.context, weight + k * p.nb01, input, output + k * 4, p.ne10, 1);
                    }
                }
                col += 1;
            }
            block0 += 16;
        }
        block1 += 16;
    }
}

fn execute(
    p: const Plan &,
    counts: i64[:],
    rows: Row[:],
    ids: const i32[:],
    mask: const i32[:],
    lut: const i32[:],
    dst: u8[:]) {
    assert(counts.len >= p.experts && rows.len >= mul_size(mul_size(p.experts, p.ids_per_token), p.tokens));
    assert(p.ids_stride0 >= 0 && p.ids_stride1 >= 0 && p.dst_bytes >= 0);

    if p.mode != 0 {
        assert(mask.len >= p.real_experts && dst.len >= p.dst_bytes);
    }

    if p.mode == 2 {
        assert(lut.len >= p.real_experts);
    }
    witness(p.context, 0, 1);

    if p.need_quant != 0 {
        let nw2 = p.row_size * p.ne11;
        let nw3 = nw2 * p.ne12;
        let first = (p.ith * p.ne10 / p.block_size) / p.nth;
        let last = ( (p.ith + 1) * p.ne10 / p.block_size) / p.nth;
        for sample in p.ne13 {
            for token in p.ne12 {
                for slot in p.ne11 {
                    let source = sample * p.nb13 + token * p.nb12 + slot * p.nb11 + first * p.block_size * p.nb10;
                    let target = sample * nw3 + token * nw2 + slot * p.row_size + first * p.block_bytes;
                    quantize(p.context, source, target, (last - first) * p.block_size);
                }
            }
        }
        witness(p.context, 1, (last - first) * p.ne11 * p.ne12 * p.ne13);
    }
    let capacity = p.ids_per_token * p.tokens;

    if p.ith == 0 {
        if p.mode != 0 {
            for i in p.dst_bytes {
                dst[i] = 0;
            }
        }
        for expert in p.experts {
            counts[expert] = 0;
        }
        for token in p.tokens {
            for slot in p.ids_per_token {
                let real = ids[token * p.ids_stride1 + slot * p.ids_stride0] as i64;
                assert(real >= 0 && real < p.real_experts);

                if p.mode == 0 || mask[real] != 0 {
                    let expert = if p.mode == 2 {
                        lut[real] as i64
                    } else {
                        real
                    }
                    ;
                    assert(expert >= 0 && expert < p.experts);
                    let index = expert * capacity + counts[expert];
                    rows[index] = Row {
                        slot as !i32, token as !i32
                    }
                    ;
                    counts[expert] += 1;
                }
            }
        }
        witness(p.context, 2, capacity);
        witness(p.context, 6, p.total_bytes);
    }
    var expert = p.ith;

    while expert < p.experts {
        reset(p.context, expert, p.nth as !i32);
        expert += p.nth;
    }
    barrier(p.context);
    expert = 0;

    while expert < p.experts {
        let n = counts[expert];

        if n > 0 {
            if p.mode == 0 && p.is_iq2s != 0 && p.ith == 0 {
                stats(p.context, 12);
                stats(p.context, if n >= 4 {
                    3
                } else {
                    n - 1
                }
                );
                stats(p.context, 8 + (if p.tokens >= 4 {
                    3
                } else {
                    p.tokens - 1
                }
                ));
            }

            if p.mode == 0 {
                ready(p.context, expert);
            }
            let fused = p.mode == 0 && p.fused_available != 0 && p.is_iq2s != 0 && p.tokens >= 2 && p.tokens <= 3 && n >= 2 && n <= 3 && p.disable_chunking == 0;

            if fused {
                stats(p.context, 14);

                if p.ith == 0 {
                    stats(p.context, 4 + n - 1);
                }
                let nc = (p.output_rows + 15) / 16;
                let dr = (p.output_rows + nc - 1) / nc;
                var current = p.ith;

                while current < nc {
                    let first = dr * current;
                    chunk(p.context, expert, first, minimum(first + dr, p.output_rows), 0, n, 1);
                    witness(p.context, 4, 1);

                    if p.nth >= nc {
                        break;
                    }
                    current = claim(p.context, expert) as i64;
                }
            } else {
                var delegated = false;

                if p.mode == 0 {
                    delegated = tiled(p.context, expert, n);

                    if delegated {
                        witness(p.context, 5, 1);
                    }
                }

                if !delegated {
                    var size: i64 = 16;

                    if p.output_rows == 1 || n == 1 {
                        size = 64;
                    }
                    var nc0 = (p.output_rows + size - 1) / size;
                    var nc1 = (n + size - 1) / size;

                    if nc0 * nc1 < p.nth * 4 || p.disable_chunking != 0 {
                        nc0 = if p.output_rows > n {
                            p.nth
                        } else {
                            1
                        }
                        ;
                        nc1 = if p.output_rows > n {
                            1
                        } else {
                            p.nth
                        }
                        ;
                    }
                    let dr0 = (p.output_rows + nc0 - 1) / nc0;
                    let dr1 = (n + nc1 - 1) / nc1;
                    var current = p.ith;

                    while current < nc0 * nc1 {
                        let first = dr0 * (current % nc0);
                        let cfirst = dr1 * (current / nc0);

                        if p.apex_math != 0 {
                            apex_chunk(
                                p,
                                expert,
                                first,
                                minimum(first + dr0, p.output_rows),
                                cfirst,
                                minimum(cfirst + dr1, n),
                                rows);
                        } else {
                            chunk(
                                p.context,
                                expert,
                                first,
                                minimum(first + dr0, p.output_rows),
                                cfirst,
                                minimum(cfirst + dr1, n),
                                0);
                        }
                        witness(p.context, 3, 1);

                        if p.nth >= nc0 * nc1 {
                            break;
                        }
                        current = claim(p.context, expert) as i64;
                    }
                }
            }
        }
        expert += 1;
    }
}

fn entry(
    p: Plan &,
    phase: i64,
    counts: i64[:],
    rows: Row[:],
    ids: const i32[:],
    mask: const i32[:],
    lut: const i32[:],
    dst: u8[:]) {
    if phase == 0 {
        layout(p);
    } else {
        assert(phase == 1);
        execute(p, counts, rows, ids, mask, lut, dst);
    }
}

fn main() {
    var p: Plan = default < Plan > ();
    p.nth = 1;
    p.cache_line = 64;
    p.block_size = 1;
    p.block_bytes = 4;
    p.ne10 = 1;
    p.ne11 = 1;
    p.output_rows = 1;
    p.wdata_bytes = 128;
    var counts: i64[1] = default < i64[1] > ();
    var rows: Row[1] = default < Row[1] > ();
    let ids: i32[1] = default < i32[1] > ();
    var dst: u8[1] = default < u8[1] > ();
    entry(p, 0, counts, rows, ids, ids, ids, dst);
    print(p.total_bytes);
}

