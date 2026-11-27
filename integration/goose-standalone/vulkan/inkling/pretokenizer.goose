// The pinned llama.cpp GPT4O path folds non-ASCII letters and marks
// into category bytes. Only ASCII a-z and A-Z distinguish the two runs.
fn pt_inkling_word(cpts: i64[:], n: i64, start: i64, lower_required: bool) -> i64 {
    var at = start;
    while at < n && (uf_flags(cpts[at]) & (PT_LETTER | PT_MARK)) != 0 &&
          (cpts[at] < 97 || cpts[at] > 122) {
        at += 1;
    }
    if lower_required {
        while at > start {
            if at < n && (uf_flags(cpts[at]) & (PT_LETTER | PT_MARK)) != 0 &&
               (cpts[at] < 65 || cpts[at] > 90) { break; }
            at -= 1;
        }
    } else if at == start {
        return start;
    }
    let lower_start = at;
    while at < n && (uf_flags(cpts[at]) & (PT_LETTER | PT_MARK)) != 0 &&
          (cpts[at] < 65 || cpts[at] > 90) {
        at += 1;
    }
    return if lower_required && at == lower_start { start } else { at };
}

fn pt_inkling_apostrophe(cpts: i64[:], n: i64, pos: i64) -> i64 {
    if pos + 1 >= n || cpts[pos] != 39 { return pos; }
    let a = cpts[pos + 1] | 32;
    if a == 115 || a == 116 || a == 109 || a == 100 { return pos + 2; }
    if pos + 2 >= n { return pos; }
    let b = cpts[pos + 2] | 32;
    if (a == 114 && b == 101) || (a == 118 && b == 101) || (a == 108 && b == 108) {
        return pos + 3;
    }
    return pos;
}

fn pt_inkling_space(cpt: i64) -> bool {
    return cpt == 32 || (cpt >= 9 && cpt <= 13) ||
        (cpt >= 128 && (uf_flags(cpt) & PT_SPACE) != 0);
}

fn pt_inkling_punctuation(cpt: i64) -> bool {
    return !pt_inkling_space(cpt) && (uf_flags(cpt) & (PT_LETTER | PT_NUM)) == 0;
}

fn pt_inkling_end(cpts: i64[:], n: i64, pos: i64) -> i64 {
    let c = cpts[pos];
    let f = uf_flags(c);
    let prefix_allowed = c != 10 && c != 13 && (f & (PT_LETTER | PT_NUM)) == 0;
    for alternative in 2 {
        let lower_required = alternative == 0;
        if prefix_allowed {
            let prefixed = pt_inkling_word(cpts, n, pos + 1, lower_required);
            if prefixed > pos + 1 { return pt_inkling_apostrophe(cpts, n, prefixed); }
        }
        let word = pt_inkling_word(cpts, n, pos, lower_required);
        if word > pos { return pt_inkling_apostrophe(cpts, n, word); }
    }
    if (f & PT_NUM) != 0 {
        var at = pos + 1;
        while at < n && at - pos < 3 && (uf_flags(cpts[at]) & PT_NUM) != 0 { at += 1; }
        return at;
    }

    let punct = if c == 32 { pos + 1 } else { pos };
    if punct < n && pt_inkling_punctuation(cpts[punct]) {
        var at = punct + 1;
        while at < n && pt_inkling_punctuation(cpts[at]) { at += 1; }
        while at < n && (cpts[at] == 13 || cpts[at] == 10 || cpts[at] == 47) { at += 1; }
        return at;
    }

    var at = pos;
    var newline = pos;
    while at < n && pt_inkling_space(cpts[at]) {
        if cpts[at] == 13 || cpts[at] == 10 { newline = at + 1; }
        at += 1;
    }
    if newline > pos { return newline; }
    if at < n && at - pos > 1 { return at - 1; }
    return if at > pos { at } else { pos + 1 };
}

fn pt_inkling2_body(text: const u8[:], cpts: i64[:], seg: i64[:], enc: u8[:], info: i64[:]) -> i64 {
    if cpts.len < text.len || seg.len < text.len * 4 || enc.len < text.len * 8 || info.len < 2 {
        return -1;
    }
    let n = uc_decode_body(text, cpts);
    if n < 0 { return -1; }
    var start: i64 = 0;
    var count: i64 = 0;
    var out: i64 = 0;
    while start < n {
        let end = pt_inkling_end(cpts, n, start);
        seg[4 * count] = start;
        seg[4 * count + 1] = end - start;
        seg[4 * count + 2] = out;
        out = pt_put_word(cpts, start, end - start, enc, out);
        seg[4 * count + 3] = out - seg[4 * count + 2];
        start = end;
        count += 1;
    }
    info[0] = n;
    info[1] = out;
    return count;
}
