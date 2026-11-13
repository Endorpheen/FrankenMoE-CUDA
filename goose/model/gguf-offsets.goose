// GGUF offset indexing in Goose, from ggml/src/gguf.cpp (the data-section seek,
// the per-tensor offset validation and the size accumulation) and
// ggml/include/ggml.h (GGML_PAD).
//
// Owned decisions:
//   - alignment must be a non-zero power of two;
//   - the data section starts at pad(end_of_structure, alignment) relative to
//     the file start;
//   - a tensor's byte size is type_size * (nelements / block_size) and must
//     divide evenly;
//   - descriptor offsets are cumulative padded sizes, checked tensor by tensor;
//   - a tensor's absolute data offset is data_start + descriptor offset.
//
// The type size/block-size table and the file reads stay native.
extern "fg457_ld_i64" fn ld_i64(p: u64, o: u64) -> i64;
extern "fg457_st_i64" fn st_i64(p: u64, o: u64, v: i64);

fn put(out: u64, i: i64, v: i64) {
    st_i64(out, (i * (8 as i64)) as u64, v);
}

export fn align_up(x: i64, n: i64) -> i64 {
    return (x + n - 1) & ( (0 as i64) - n);
}

export fn align_ok(a: i64) -> i64 {
    if a <= 0 as i64 {
        return 0 as i64;
    }

    if (a & (a - 1)) != 0 as i64 {
        return 0 as i64;
    }
    return 1 as i64;
}
// 0 when nelements divides by the block size, -3 otherwise.

export fn nbytes(tsize: i64, blk: i64, nelements: i64, out: u64) -> i64 {
    var nb: i64 = 0 as i64;

    if blk <= 0 as i64 {
        put(out, 0 as i64, -(3 as i64));
        return -(3 as i64);
    }

    if nelements < 0 as i64 {
        put(out, 0 as i64, -(3 as i64));
        return -(3 as i64);
    }

    if (nelements % blk) != 0 as i64 {
        put(out, 0 as i64, -(3 as i64));
        return -(3 as i64);
    }
    nb = tsize * (nelements / blk);
    put(out, 0 as i64, nb);
    return nb;
}
// rel/nb are the descriptor offsets and byte sizes in file order; out receives
// the absolute offsets and, at out[rel.len], the total padded data size.
// Returns 0 on success, -1 for a bad alignment, -2 for a non-cumulative offset.

export fn layout(rel: i64[:], nb: i64[:], align: i64, data_start: i64, out: i64[:]) -> i64 {
    if align_ok(align) == 0 as i64 {
        return -(1 as i64);
    }

    if rel.len != nb.len {
        return -(2 as i64);
    }
    var size: i64 = 0 as i64;
    var i: i64 = 0 as i64;

    while i < rel.len {
        if rel[i] != size {
            return -(2 as i64);
        }
        out[i] = data_start + rel[i];
        size = size + align_up(nb[i], align);
        i = i + 1 as i64;
    }

    if rel.len < out.len {
        out[rel.len] = size;
    }
    return 0 as i64;
}

