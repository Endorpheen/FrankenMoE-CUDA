fn recurrent_attention(
    p: const StatePlan &,
    a0: const u8[:],
    a1: const u8[:],
    a2: const u8[:],
    a3: const u8[:],
    a4: const u8[:],
    a5: const u8[:],
    a6: const u8[:],
    dst: u8[:]) -> bool {
    let v7 = p.op == OP_RWKV_WKV7;
    let linear = p.op == OP_GATED_LINEAR_ATTN;

    if !v7 && !linear && p.op != OP_RWKV_WKV6 {
        return false;
    }
    let state_index = if v7 {
        6
    } else {
        if linear {
            4
        } else {
            5
        }
    }
    ;
    let width = p.source[1].ne[0];
    let heads = p.source[1].ne[1];
    let tokens = p.source[1].ne[2];
    let seqs = p.source[state_index].ne[1];
    assert(width > 0 && heads > 0 && tokens >= 0 && seqs > 0 && tokens % seqs == 0 && p.nth > 0 && p.ith >= 0 && p.ith < p.nth);
    assert(p.dst.kind == 0);
    validate(p.dst, dst.len);
    for input in state_index + 1 {
        let shape = p.source[input];
        assert(shape.kind == 0);
        var stride: i64 = 4;
        for axis in 4 {
            assert(shape.nb[axis] == stride);
            stride = mul_size(stride, shape.ne[axis]);
        }
        assert(shape.bytes >= stride);
    }
    validate(p.source[0], a0.len);
    validate(p.source[1], a1.len);
    validate(p.source[2], a2.len);
    validate(p.source[3], a3.len);
    validate(p.source[4], a4.len);

    if !linear {
        validate(p.source[5], a5.len);
    }

    if v7 {
        validate(p.source[6], a6.len);
        assert(width % 32 == 0);
    }
    let channels = mul_size(width, heads);
    let scores = mul_size(channels, tokens);
    let square = mul_size(width, width);
    let state_values = mul_size(mul_size(square, heads), seqs);
    assert(elements(p.source[state_index]) == state_values && mul_size(add_size(scores, state_values), 4) <= dst.len);
    for input in state_index {
        if !v7 && input == 3 && !linear {
            assert(elements(p.source[input]) >= channels);
        } else {
            assert(elements(p.source[input]) >= scores);
        }
    }
    let begin = heads * p.ith / p.nth;
    let finish = heads * (p.ith + 1) / p.nth;
    let simd_end = width / 8 * 8;
    for head in finish - begin {
        let h = begin + head;
        for token in tokens {
            let seq = token / (tokens / seqs);
            let output = token * channels + h * width;
            let state_base = scores + (seq * heads + h) * square;
            let initial = (seq * heads + h) * square;

            if token % (tokens / seqs) == 0 {
                for i in square {
                    let value = if v7 {
                        value_at(a6, initial + i)
                    } else {
                        if linear {
                            value_at(a4, initial + i)
                        } else {
                            value_at(a5, initial + i)
                        }
                    }
                    ;
                    set_value(dst, state_base + i, value);
                }
            }

            if v7 {
                for row in width {
                    let base = state_base + row * width;
                    let sa = vector_dot(p.source[4], a4, output * 4, 4, p.dst, dst, base * 4, 4, width);
                    let value = value_at(a3, output + row);
                    for col in width {
                        let at = output + col;
                        let kv = value * value_at(a2, at);
                        let decayed = fma32(value_at(dst, base + col), value_at(a1, at), kv);
                        set_value(dst, base + col, fma32(sa, value_at(a5, at), decayed));
                    }
                    set_value(
                        dst,
                        output + row,
                        vector_dot(p.dst, dst, base * 4, 4, p.source[0], a0, output * 4, 4, width));
                }
            } else {
                for col in width {
                    set_value(dst, output + col, 0 as f32);
                }
                for row in width {
                    let key = value_at(a0, output + row);
                    let query = value_at(a2, output + row) * (if linear {
                        p.fp[0]
                    } else {
                        1 as f32
                    }
                    );
                    let decay = if linear {
                        value_at(a3, output + row)
                    } else {
                        value_at(a4, output + row)
                    }
                    ;
                    let factor = if linear {
                        0 as f32
                    } else {
                        value_at(a3, h * width + row)
                    }
                    ;
                    for col in width {
                        let at = state_base + row * width + col;
                        let kv = value_at(a1, output + col) * key;
                        let previous = value_at(dst, at);
                        let next = if col < simd_end {
                            fma32(previous, decay, kv)
                        } else {
                            previous * decay + kv
                        }
                        ;
                        let weighted = if linear {
                            next
                        } else {
                            if col < simd_end {
                                fma32(kv, factor, previous)
                            } else {
                                kv * factor + previous
                            }
                        }
                        ;
                        let result = if col < simd_end {
                            fma32(weighted, query, value_at(dst, output + col))
                        } else {
                            value_at(dst, output + col) + weighted * query
                        }
                        ;
                        set_value(dst, at, next);
                        set_value(dst, output + col, result);
                    }
                }
            }
        }
    }
    witness(p.op, scores);
    return true;
}

