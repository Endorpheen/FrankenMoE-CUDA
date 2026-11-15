// P10-12 S3: the expert streamer's shard resolution.
//
// Native source: runtime/core/src/moe/gguf_offsets.cpp at base commit
// aa9109b5, functions split_count_from_path and make_shard_path (the
// anonymous-namespace half of fill_all_offsets). The rest of the file is the
// gguf API walk (gguf_init_from_file, gguf_get_tensor_offset and friends),
// which is the container library and stays native; this module is the naming
// decision that decides WHICH files the walk opens.
//
// A model may arrive as `model-00001-of-00003.gguf` plus siblings. The rules,
// exactly as the native code applies them:
//   - the path must be at least 20 bytes: 19 of shard tail plus a separator;
//   - the last 19 bytes must be `DDDDD-of-DDDDD.gguf` with Arabic digits;
//   - the byte before the tail must be '-';
//   - a count of 1 is not a split set (a `-00001-of-00001` file is a single
//     file, not an error);
//   - any shard other than the first is refused (-1): the caller must pass the
//     first shard, because downstream indices line up with the loader's;
//   - the prefix is everything before the separating dash, and siblings are
//     rebuilt as `<prefix>-%05d-of-%05d.gguf`.
//
// The prefix is sn_. `stoi` on five digits is plain decimal accumulation here,
// and `%05d` is zero padding to at least five characters.

const SN_TAIL: i64 = 19;

fn sn_five_digits(p: const u8[:], at: i64) -> bool {
    var i: i64 = at;
    while i < at + 5 {
        if p[i] < 48 || p[i] > 57 {
            return false;
        }
        i = i + 1;
    }
    return true;
}

fn sn_stoi5(p: const u8[:], at: i64) -> i64 {
    var v: i64 = 0;
    var i: i64 = at;
    while i < at + 5 {
        v = v * 10 + (p[i] - 48);
        i = i + 1;
    }
    return v;
}

// The prefix length of a split-set path: everything before the dash that
// introduces the shard number. -1 when the name is not `<prefix>-DDDDD-of-DDDDD.gguf`.
fn sn_plen(path: const u8[:]) -> i64 {
    if path.len < SN_TAIL + 1 {
        return 0 - 1;
    }
    let tail: i64 = path.len - SN_TAIL;
    if path[tail - 1] != 45 {
        return 0 - 1;
    }
    if !sn_five_digits(path, tail) {
        return 0 - 1;
    }
    if path[tail + 5..tail + 9] != "-of-" {
        return 0 - 1;
    }
    if !sn_five_digits(path, tail + 9) {
        return 0 - 1;
    }
    if path[tail + 14..tail + 19] != ".gguf" {
        return 0 - 1;
    }
    return tail - 1;
}

// The exported form of the prefix scan.
export fn sn_prefix_len(path: const u8[:]) -> i64 {
    return sn_plen(path);
}

// split_count_from_path's return value: the shard count, 0 for a single file
// (no tail, or a `-00001-of-00001` name), -1 for "this is shard N, pass the
// first one". `prefix_out` receives the prefix bytes when the count is > 1.
export fn sn_split_count(path: const u8[:], prefix_out: u8[:]) -> i64 {
    let plen: i64 = sn_plen(path);
    if plen < 0 {
        return 0;
    }
    let tail: i64 = path.len - SN_TAIL;
    let no: i64 = sn_stoi5(path, tail);
    let count: i64 = sn_stoi5(path, tail + 9);
    if count <= 1 {
        return 0;
    }
    if no != 1 {
        return 0 - 1;
    }
    var i: i64 = 0;
    while i < plen && i < prefix_out.len {
        prefix_out[i] = path[i];
        i = i + 1;
    }
    return count;
}

// make_shard_path: `<prefix>-%05d-of-%05d.gguf`. Returns the bytes written.
export fn sn_shard_path(prefix: const u8[:], no: i64, count: i64, out: u8[:]) -> i64 {
    var at: i64 = 0;
    var i: i64 = 0;
    while i < prefix.len && at < out.len {
        out[at] = prefix[i];
        at = at + 1;
        i = i + 1;
    }
    at = sn_putc(out, at, 45); // '-'
    at = sn_put05(out, at, no);
    at = sn_putc(out, at, 45); // '-'
    at = sn_putc(out, at, 111); // 'o'
    at = sn_putc(out, at, 102); // 'f'
    at = sn_putc(out, at, 45); // '-'
    at = sn_put05(out, at, count);
    at = sn_putc(out, at, 46); // '.'
    at = sn_putc(out, at, 103); // 'g'
    at = sn_putc(out, at, 103); // 'g'
    at = sn_putc(out, at, 117); // 'u'
    at = sn_putc(out, at, 102); // 'f'
    return at;
}

fn sn_putc(out: u8[:], at: i64, c: u8) -> i64 {
    if at < out.len {
        out[at] = c;
        return at + 1;
    }
    return at;
}

// "%05d": at least five digits, zero padded. Negative values keep the sign and
// then pad to five digits in total, which is what printf does for %05d.
fn sn_put05(out: u8[:], at0: i64, v0: i64) -> i64 {
    var at: i64 = at0;
    var v: i64 = v0;
    if v < 0 {
        at = sn_putc(out, at, 45);
        v = 0 - v;
    }
    var digits: i64 = 1;
    var t: i64 = v;
    while t >= 10 {
        t = t / 10;
        digits = digits + 1;
    }
    var zeros: i64 = 5 - digits;
    while zeros > 0 {
        at = sn_putc(out, at, 48);
        zeros = zeros - 1;
    }
    var buf: i64[12] = [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0];
    var n: i64 = 0;
    if v == 0 {
        buf[0] = 0;
        n = 1;
    }
    while v > 0 {
        buf[n] = v % 10;
        v = v / 10;
        n = n + 1;
    }
    var i: i64 = n;
    while i > 0 {
        i = i - 1;
        at = sn_putc(out, at, (48 + buf[i]) as u8);
    }
    return at;
}
