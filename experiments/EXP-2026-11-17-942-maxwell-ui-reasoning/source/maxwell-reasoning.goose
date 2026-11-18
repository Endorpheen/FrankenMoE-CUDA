fn mx_find(hay: const u8[:], needle: const u8[:], from: i64, end: i64) -> i64 {
    var i: i64 = from;
    while i + needle.len <= end {
        if mx_prefix(hay, i, end, needle) == needle.len {
            return i;
        }
        i += 1;
    }
    return -1;
}

fn mx_prefix(s: const u8[:], at: i64, end: i64, marker: const u8[:]) -> i64 {
    var n: i64 = 0;
    while at + n < end && n < marker.len {
        if s[at + n] != marker[n] { break; }
        n += 1;
    }
    return n;
}

fn mx_skip_space(s: const u8[:], at: i64, end: i64) -> i64 {
    var i: i64 = at;
    while i < end {
        let b: u8 = s[i];
        if b != 32 && b != 9 && b != 10 && b != 13 { break; }
        i += 1;
    }
    return i;
}

fn mx_without_tag_suffix(s: const u8[:], start: i64, end: i64, marker: const u8[:], partial: i64) -> i64 {
    var i: i64 = end - marker.len;
    if i < start { i = start; }
    while i < end {
        let n: i64 = end - i;
        if (n >= 2 || partial != 0) && mx_prefix(s, i, end, marker) == n {
            return i;
        }
        i += 1;
    }
    return end;
}

export fn mx_is_simple_template(src: const u8[:]) -> i64 {
    if mx_find(src, "<|maxwell_start|>", 0, src.len) >= 0 &&
        mx_find(src, "<|maxwell_end|>", 0, src.len) >= 0 &&
        mx_find(src, "reasoning_content", 0, src.len) < 0 {
        return 1;
    }
    return 0;
}

export fn mx_response_spans(s: const u8[:], partial: i64, out: i64[:]) -> i64 {
    if out.len < 4 { return -2; }
    let open: const u8[:] = "<think>";
    let close: const u8[:] = "</think>";
    let end: i64 = mx_without_tag_suffix(s, 0, s.len, "<|maxwell_end|>", partial);
    let first: i64 = mx_skip_space(s, 0, end);
    out[0] = 0; out[1] = 0; out[2] = 0; out[3] = 0;
    let lead: i64 = mx_prefix(s, first, end, open);
    if lead != open.len {
        if lead == end - first && (partial != 0 || lead >= 2) { return 1; }
        out[3] = end;
        return 1;
    }

    let body: i64 = first + open.len;
    let stop: i64 = mx_find(s, close, body, end);
    out[0] = body;
    if stop < 0 {
        out[1] = mx_without_tag_suffix(s, body, end, close, partial) - body;
        return -1;
    }

    out[1] = stop - body;
    let answer: i64 = mx_skip_space(s, stop + close.len, end);
    out[2] = answer;
    out[3] = end - answer;
    return 0;
}
