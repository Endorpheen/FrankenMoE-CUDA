// P7-03: multi-shard file-name rule, in Goose.
//
// Native reference: src/llama.cpp, llama_split_prefix(). The expected postfix is
//   "-%05d-of-%05d.gguf" with (split_no + 1, split_count)
// where %05d pads to five digits but prints more when the number needs them. The rule then is:
//   - if the path is not longer than the postfix, no match;
//   - if the path ends with the postfix, the prefix length is path_len - postfix_len;
//   - otherwise no match.
//
// The module returns the prefix length, or -1 for "no match"; it does not copy, so the caller keeps its buffer.

extern "fg284p_fail" fn fail(c:i64);
extern "fg284p_ld_u8" fn ld_u8(base:u64, byte:i64)->i64;

fn ensure(v:bool,c:i64){if !v{fail(c);}}
fn at(a:u64, i:i64)->i64 {return ld_u8(a, i);}

fn dec_digits(v:i64)->i64 {
    var d:i64 = 1 as i64;
    var t:i64 = v;
    while t >= 10 as i64 {t = t / (10 as i64); d = d + 1 as i64;}
    d
}

// expected character at index i of the postfix, or -1 past its end; i runs over the whole postfix
fn postfix_at(i:i64, n:i64, count:i64)->i64 {
    let dn:i64 = if dec_digits(n)     > 5 as i64 {dec_digits(n)}     else {5 as i64};
    let dc:i64 = if dec_digits(count) > 5 as i64 {dec_digits(count)} else {5 as i64};
    let total:i64 = 1 as i64 + dn + 4 as i64 + dc + 5 as i64;  // "-" n "-of-" count ".gguf"
    if i < 0 as i64 || i >= total {return -1 as i64;}
    if i == 0 as i64 {return 45 as i64;}                       // '-'
    if i < 1 as i64 + dn {
        let k:i64 = i - 1 as i64;                              // digit index, high to low
        let place:i64 = dn - 1 as i64 - k;
        var p:i64 = 1 as i64;
        var q:i64 = 0 as i64;
        while q < place {p = p * (10 as i64); q = q + 1 as i64;}
        return 48 as i64 + ((n / p) % (10 as i64));
    }
    let off:i64 = 1 as i64 + dn;
    if i < off + 4 as i64 {
        let c:i64 = i - off;                                    // '-of-' = 45,111,102,45
        if c == 0 as i64 {return 45 as i64;}
        if c == 1 as i64 {return 111 as i64;}
        if c == 2 as i64 {return 102 as i64;}
        return 45 as i64;
    }
    let off2:i64 = off + 4 as i64;
    if i < off2 + dc {
        let k:i64 = i - off2;
        let place:i64 = dc - 1 as i64 - k;
        var p:i64 = 1 as i64;
        var q:i64 = 0 as i64;
        while q < place {p = p * (10 as i64); q = q + 1 as i64;}
        return 48 as i64 + ((count / p) % (10 as i64));
    }
    let off3:i64 = off2 + dc;                                   // ".gguf" = 46,103,103,117,102
    let c3:i64 = i - off3;
    if c3 == 0 as i64 {return 46 as i64;}
    if c3 == 1 as i64 {return 103 as i64;}
    if c3 == 2 as i64 {return 103 as i64;}
    if c3 == 3 as i64 {return 117 as i64;}
    return 102 as i64;
}

fn postfix_len(n:i64, count:i64)->i64 {
    let dn:i64 = if dec_digits(n)     > 5 as i64 {dec_digits(n)}     else {5 as i64};
    let dc:i64 = if dec_digits(count) > 5 as i64 {dec_digits(count)} else {5 as i64};
    1 as i64 + dn + 4 as i64 + dc + 5 as i64
}

// prefix length when the path ends with the shard postfix, else -1
fn split_prefix(path:u64, len:i64, split_no:i64, split_count:i64) -> i64 {
    ensure(len >= 0 as i64 && split_no >= 0 as i64 && split_count > 0 as i64, 10);
    let n:i64 = split_no + 1 as i64;
    let total:i64 = postfix_len(n, split_count);
    if len <= total {return -1 as i64;}
    let size_prefix:i64 = len - total;
    var i:i64 = 0 as i64;
    while i < total {
        if at(path, size_prefix + i) != postfix_at(i, n, split_count) {return -1 as i64;}
        i = i + 1 as i64;
    }
    size_prefix
}

fn main(){ split_prefix(0 as u64, 0 as i64, 0 as i64, 1 as i64); }
