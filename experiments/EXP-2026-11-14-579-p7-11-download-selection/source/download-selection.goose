// Download selection policy, ported from common/download.cpp.
//
// Owned decisions:
//   - get_gguf_split_info: the `-NNNNN-of-NNNNN.gguf` split form, the tag after
//     the last '-' or '.', upper-cased, and the ".gguf" suffix requirement;
//   - extract_quant_bits: the first digit run inside the tag;
//   - common_download_get_all_parts: one URL per split part, `-%05d-of-%05d`;
//   - get_split_files: siblings with the same count and prefix;
//   - find_best_model: tag order, the literal `tag.`/`tag-` match, the
//     split-index-1 rule, and the empty-tag fallback;
//   - find_best_sibling: same directory components, keyword in the path, exact
//     tag preference, then the closest quant bits;
//   - the sidecar table and the plan selection;
//   - gguf_filename_is_model / name_is_sidecar;
//   - common_download_split_repo_tag;
//   - the resume decision inside common_pull_file (range request and the
//     206/200/416 response rules).
//
// Not owned: httplib, the progress callback, ofstream, docker, the cache walk
// and the destructive removal paths.
//
// File lists are one blob plus per-file offset/length arrays, the same flat
// shape the hf-cache slice uses for names. Split info is written as offsets
// into the input name.
//
// Deliberate deviation: find_best_model builds `std::regex(tag + "[.-]")` from
// the tag, so regex metacharacters inside a tag are interpreted. The port
// matches the tag literally, identical for every GGUF quant tag and different
// only for an adversarial tag.

fn d_eq(a: const u8[:], b: const u8[:]) -> bool {
    if a.len != b.len {
        return false;
    }
    var i: i64 = 0;

    while i < a.len {
        if a[i] != b[i] {
            return false;
        }
        i = i + 1;
    }
    return true;
}

fn d_lower(c: u8) -> u8 {
    if c >= 65 && c <= 90 {
        return c + 32;
    }
    return c;
}

fn d_upper(c: u8) -> u8 {
    if c >= 97 && c <= 122 {
        return c - 32;
    }
    return c;
}

fn d_ends(s: const u8[:], suf: const u8[:]) -> bool {
    if suf.len > s.len {
        return false;
    }
    var i: i64 = 0;

    while i < suf.len {
        if s[s.len - suf.len + i] != suf[i] {
            return false;
        }
        i = i + 1;
    }
    return true;
}

fn d_find_from(s: const u8[:], from: i64, ch: u8) -> i64 {
    var i: i64 = from;

    while i < s.len {
        if s[i] == ch {
            return i;
        }
        i = i + 1;
    }
    return 0 - 1;
}

fn d_is_digit(c: u8) -> bool {
    return c >= 48 && c <= 57;
}

fn d_is_tag_char(c: u8) -> bool {
    return d_is_digit(c) || (c >= 65 && c <= 90) || (c >= 97 && c <= 122) || c == 95;
}

fn d_sub_eq(blob: const u8[:], a: i64, alen: i64, b: i64, blen: i64) -> bool {
    if alen != blen {
        return false;
    }
    var i: i64 = 0;

    while i < alen {
        if blob[a + i] != blob[b + i] {
            return false;
        }
        i = i + 1;
    }
    return true;
}

// Case-sensitive literal search, the native std::string::find.
fn d_contains(s: const u8[:], pat: const u8[:]) -> bool {
    if pat.len == 0 || pat.len > s.len {
        return false;
    }
    var i: i64 = 0;

    while i + pat.len <= s.len {
        var ok: bool = true;
        var k: i64 = 0;

        while k < pat.len {
            if s[i + k] != pat[k] {
                ok = false;
                k = pat.len;
            } else {
                k = k + 1;
            }
        }
        if ok {
            return true;
        }
        i = i + 1;
    }
    return false;
}

// Case-insensitive literal followed by '.' or '-', the find_best_model regex.
fn d_contains_tag(s: const u8[:], tag: const u8[:]) -> bool {
    if tag.len == 0 || tag.len + 1 > s.len {
        return false;
    }
    var i: i64 = 0;

    while i + tag.len < s.len {
        var ok: bool = true;
        var k: i64 = 0;

        while k < tag.len {
            if d_lower(s[i + k]) != d_lower(tag[k]) {
                ok = false;
                k = tag.len;
            } else {
                k = k + 1;
            }
        }
        if ok && (s[i + tag.len] == 46 || s[i + tag.len] == 45) {
            return true;
        }
        i = i + 1;
    }
    return false;
}

// `-TAG.` anywhere, case insensitive, as find_best_sibling builds it.
fn d_contains_dash_tag_dot(s: const u8[:], tag: const u8[:]) -> bool {
    if tag.len == 0 || tag.len + 2 > s.len {
        return false;
    }
    var i: i64 = 0;

    while i + tag.len + 1 < s.len {
        if s[i] == 45 {
            var ok: bool = true;
            var k: i64 = 0;

            while k < tag.len {
                if d_lower(s[i + 1 + k]) != d_lower(tag[k]) {
                    ok = false;
                    k = tag.len;
                } else {
                    k = k + 1;
                }
            }
            if ok && s[i + 1 + tag.len] == 46 {
                return true;
            }
        }
        i = i + 1;
    }
    return false;
}

// stoi-like digit run: stops at the first non-digit, -1 when none was consumed.
fn d_digit_value(s: const u8[:], from: i64) -> i64 {
    var i: i64 = from;
    var v: i64 = 0;
    var n: i64 = 0;

    while i < s.len && d_is_digit(s[i]) {
        v = v * 10 + (s[i] as i64 - 48);
        n = n + 1;
        i = i + 1;
    }
    if n == 0 {
        return 0 - 1;
    }
    return v;
}

fn d_first_digit(s: const u8[:]) -> i64 {
    var i: i64 = 0;

    while i < s.len {
        if d_is_digit(s[i]) {
            return i;
        }
        i = i + 1;
    }
    return 0 - 1;
}

// get_gguf_split_info. out: [prefix_off, prefix_len, tag_off, tag_len, index, count].
// The tag is upper-cased, as native does, so it is written to tag_out and its
// offset is always 0. Returns 0 when the name has the .gguf suffix, 1 otherwise.
fn d_split_info(name: const u8[:], out: i64[:], tag_out: u8[:]) -> i64 {
    out[0] = 0;
    out[1] = 0;
    out[2] = 0;
    out[3] = 0;
    out[4] = 1;
    out[5] = 1;
    if !d_ends(name, ".gguf") {
        out[4] = 0;
        out[5] = 0;
        return 1;
    }
    var p_end: i64 = name.len - 5;
    var index: i64 = 1;
    var count: i64 = 1;
    // ^(.+)-([0-9]{5})-of-([0-9]{5})$ : the suffix is a fixed 15 bytes
    if p_end >= 16 {
        let at: i64 = p_end - 15;
        var ok: bool = name[at] == 45;
        var k: i64 = 1;

        while ok && k < 6 {
            if !d_is_digit(name[at + k]) {
                ok = false;
            }
            k = k + 1;
        }
        if ok && name[at + 6] == 45 && name[at + 7] == 111 && name[at + 8] == 102 && name[at + 9] == 45 {
            k = 10;
            while ok && k < 15 {
                if !d_is_digit(name[at + k]) {
                    ok = false;
                }
                k = k + 1;
            }
        } else {
            ok = false;
        }
        if ok {
            index = d_digit_value(name, at + 1);
            count = d_digit_value(name, at + 10);
            p_end = at;
        }
    }
    out[4] = index;
    out[5] = count;
    out[1] = p_end;
    // tag: [-.]([A-Z0-9_]+)$ over the prefix, upper-cased
    var te: i64 = p_end;
    var scanning: bool = true;

    while scanning && te > 0 {
        if d_is_tag_char(name[te - 1]) {
            te = te - 1;
        } else {
            scanning = false;
        }
    }
    if te > 0 && (name[te - 1] == 45 || name[te - 1] == 46) {
        out[2] = 0;
        out[3] = p_end - te;
        var k: i64 = 0;
        while k < p_end - te {
            tag_out[k] = d_upper(name[te + k]);
            k = k + 1;
        }
    }
    return 0;
}

fn d_quant_bits(name: const u8[:]) -> i64 {
    var info: i64[6] = [0, 0, 0, 0, 1, 1];
    var tagb: u8[64] = [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0];

    if d_split_info(name, info, tagb) != 0 || info[3] == 0 {
        return 0;
    }
    let tag = tagb[0 .. info[3]];
    let at = d_first_digit(tag);

    if at < 0 {
        return 0;
    }
    let v = d_digit_value(tag, at);

    if v < 0 {
        return 0;
    }
    return v;
}

fn d_part_count(name: const u8[:]) -> i64 {
    var info: i64[6] = [0, 0, 0, 0, 1, 1];

    var tagb: u8[64] = [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0];

    if d_split_info(name, info, tagb) != 0 || info[5] <= 1 {
        return 1;
    }
    return info[5];
}

fn d_copy(dst: u8[:], at: i64, src: const u8[:], from: i64, len: i64) -> i64 {
    var i: i64 = 0;

    while i < len {
        dst[at + i] = src[from + i];
        i = i + 1;
    }
    return at + len;
}

fn d_puts(dst: u8[:], at: i64, s: const u8[:]) -> i64 {
    return d_copy(dst, at, s, 0, s.len);
}

fn d_put_pad5(dst: u8[:], at: i64, v: i64) -> i64 {
    var p = at;
    var d: i64[5] = [0, 0, 0, 0, 0];
    var x = v;
    var i: i64 = 4;

    while i >= 0 {
        d[i] = x % 10;
        x = x / 10;
        i = i - 1;
    }
    i = 0;
    while i < 5 {
        dst[p] = (48 + d[i]) as u8;
        p = p + 1;
        i = i + 1;
    }
    return p;
}

// One part name; part numbers are 1-based, as native builds them.
fn d_part_name(name: const u8[:], part: i64, out: u8[:]) -> i64 {
    var info: i64[6] = [0, 0, 0, 0, 1, 1];

    var tagb: u8[64] = [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0];

    if d_split_info(name, info, tagb) != 0 || info[5] <= 1 {
        return d_copy(out, 0, name, 0, name.len);
    }
    var p: i64 = d_copy(out, 0, name, 0, info[1]);

    p = d_puts(out, p, "-");
    p = d_put_pad5(out, p, part);
    p = d_puts(out, p, "-of-");
    p = d_put_pad5(out, p, info[5]);
    return d_puts(out, p, ".gguf");
}

// get_split_files over explicit indices: same count and prefix bytes.
fn d_split_group(
    blob: const u8[:],
    off: i64[:],
    len: i64[:],
    n: i64,
    sel: i64,
    out: i64[:]) -> i64 {
    var si: i64[6] = [0, 0, 0, 0, 1, 1];
    var sb: u8[64] = [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0];

    d_split_info(blob[off[sel] .. off[sel] + len[sel]], si, sb);
    if si[5] <= 1 {
        out[0] = sel;
        return 1;
    }
    var cnt: i64 = 0;
    var j: i64 = 0;

    while j < n {
        var jj: i64[6] = [0, 0, 0, 0, 1, 1];
        var jb: u8[64] = [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0];

        d_split_info(blob[off[j] .. off[j] + len[j]], jj, jb);
        if jj[5] == si[5] && jj[1] == si[1] &&
           d_sub_eq(blob, off[sel], si[1], off[j], jj[1]) {
            out[cnt] = j;
            cnt = cnt + 1;
        }
        j = j + 1;
    }
    return cnt;
}

fn d_last_slash(s: const u8[:]) -> i64 {
    var i: i64 = s.len;
    var last: i64 = 0 - 1;

    while i > 0 {
        i = i - 1;
        if s[i] == 47 {
            last = i;
            i = 0;
        }
    }
    return last;
}

// Directory components as the native string_split('/') vector compares them:
// the same number of separators before the basename and the same bytes.
fn d_same_dir(a: const u8[:], b: const u8[:]) -> bool {
    let la = d_last_slash(a);
    let lb = d_last_slash(b);

    if la != lb {
        return false;
    }
    if la < 0 {
        return true;
    }
    var i: i64 = 0;

    while i < la {
        if a[i] != b[i] {
            return false;
        }
        i = i + 1;
    }
    return true;
}

fn d_basename_off(s: const u8[:]) -> i64 {
    let l = d_last_slash(s);

    if l < 0 {
        return 0;
    }
    return l + 1;
}

fn d_basename_len(s: const u8[:]) -> i64 {
    return s.len - d_basename_off(s);
}

fn d_name_is_sidecar(name: const u8[:]) -> i64 {
    if d_contains(name, "mmproj") || d_contains(name, "mtp-") ||
       d_contains(name, "dflash-") || d_contains(name, "eagle3-") ||
       d_contains(name, "dspark-") {
        return 1;
    }
    return 0;
}

fn d_filename_is_model(path: const u8[:]) -> i64 {
    if !d_ends(path, ".gguf") {
        return 0;
    }
    let bo = d_basename_off(path);
    let base = path[bo .. path.len];

    if d_contains(base, "imatrix") {
        return 0;
    }
    if d_name_is_sidecar(base) != 0 {
        return 0;
    }
    return 1;
}

// Scan for one tag in list order, keeping the split-index-1 rule.
fn d_scan_tag(
    blob: const u8[:],
    off: i64[:],
    len: i64[:],
    n: i64,
    tag: const u8[:]) -> i64 {
    var j: i64 = 0;

    while j < n {
        let path = blob[off[j] .. off[j] + len[j]];

        if d_filename_is_model(path) != 0 && d_contains_tag(path, tag) {
            var info: i64[6] = [0, 0, 0, 0, 1, 1];
            var tb: u8[64] = [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0];

            d_split_info(path, info, tb);
            if info[5] <= 1 || info[4] == 1 {
                return j;
            }
        }
        j = j + 1;
    }
    return 0 - 1;
}

fn d_scan_first_model(
    blob: const u8[:],
    off: i64[:],
    len: i64[:],
    n: i64) -> i64 {
    var j: i64 = 0;

    while j < n {
        let path = blob[off[j] .. off[j] + len[j]];

        if d_filename_is_model(path) != 0 {
            var info: i64[6] = [0, 0, 0, 0, 1, 1];
            var tb: u8[64] = [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0];

            d_split_info(path, info, tb);
            if info[5] <= 1 || info[4] == 1 {
                return j;
            }
        }
        j = j + 1;
    }
    return 0 - 1;
}

fn d_best_model(
    blob: const u8[:],
    off: i64[:],
    len: i64[:],
    n: i64,
    tag: const u8[:]) -> i64 {
    if tag.len > 0 {
        return d_scan_tag(blob, off, len, n, tag);
    }
    var r = d_scan_tag(blob, off, len, n, "Q4_K_M");

    if r >= 0 {
        return r;
    }
    r = d_scan_tag(blob, off, len, n, "Q8_0");
    if r >= 0 {
        return r;
    }
    return d_scan_first_model(blob, off, len, n);
}

fn d_best_sibling(
    blob: const u8[:],
    off: i64[:],
    len: i64[:],
    n: i64,
    model: i64,
    keyword: const u8[:],
    tag: const u8[:]) -> i64 {
    let mpath = blob[off[model] .. off[model] + len[model]];
    var model_bits: i64 = 0;

    if tag.len > 0 {
        let at = d_first_digit(tag);

        if at >= 0 {
            model_bits = d_digit_value(tag, at);
        }
    } else {
        model_bits = d_quant_bits(mpath);
    }
    var best: i64 = 0 - 1;
    var best_diff: i64 = 0;
    var best_exact: bool = false;
    var j: i64 = 0;

    while j < n {
        let path = blob[off[j] .. off[j] + len[j]];

        if d_ends(path, ".gguf") && d_contains(path, keyword) && d_same_dir(path, mpath) {
            let bits = d_quant_bits(path);
            var diff = bits - model_bits;

            if diff < 0 {
                diff = 0 - diff;
            }
            var exact: bool = false;

            if tag.len > 0 && d_contains_dash_tag_dot(path, tag) {
                exact = true;
            }
            var take: bool = false;

            if best < 0 {
                take = true;
            } else if exact && !best_exact {
                take = true;
            } else if exact == best_exact && diff < best_diff {
                take = true;
            }
            if take {
                best = j;
                best_diff = diff;
                best_exact = exact;
            }
        }
        j = j + 1;
    }
    return best;
}

fn d_sidecar_index(name: const u8[:]) -> i64 {
    if d_eq(name, "mmproj") {
        return 0;
    }
    if d_eq(name, "mtp") {
        return 1;
    }
    if d_eq(name, "dflash") {
        return 2;
    }
    if d_eq(name, "eagle3") {
        return 3;
    }
    if d_eq(name, "dspark") {
        return 4;
    }
    return 0 - 1;
}

fn d_sidecar_keyword(i: i64, out: u8[:]) -> i64 {
    if i == 0 {
        return d_puts(out, 0, "mmproj");
    }
    if i == 1 {
        return d_puts(out, 0, "mtp-");
    }
    if i == 2 {
        return d_puts(out, 0, "dflash-");
    }
    if i == 3 {
        return d_puts(out, 0, "eagle3-");
    }
    if i == 4 {
        return d_puts(out, 0, "dspark-");
    }
    return 0;
}

fn d_plan_sidecars(present: i64[:], out: i64[:]) -> i64 {
    var cnt: i64 = 0;
    var i: i64 = 0;

    while i < 5 {
        if present[i] != 0 {
            out[cnt] = i;
            cnt = cnt + 1;
        }
        i = i + 1;
    }
    return cnt;
}

// common_download_split_repo_tag. out: [repo_off, repo_len, tag_off, tag_len].
fn d_split_repo_tag(s: const u8[:], out: i64[:]) -> i64 {
    let first = d_find_from(s, 0, 58);
    var lastc: i64 = 0 - 1;
    var i: i64 = 0;

    while i < s.len {
        if s[i] == 58 {
            lastc = i;
        }
        i = i + 1;
    }
    var repo_end: i64 = s.len;

    if first >= 0 {
        repo_end = first;
    }
    out[0] = 0;
    out[1] = repo_end;
    out[2] = 0;
    out[3] = 0;
    if lastc >= 0 {
        out[2] = lastc + 1;
        out[3] = s.len - lastc - 1;
    }
    var slashes: i64 = 0;
    i = 0;
    while i < repo_end {
        if s[i] == 47 {
            slashes = slashes + 1;
        }
        i = i + 1;
    }
    if repo_end == 0 || slashes != 1 {
        return 1;
    }
    return 0;
}

// common_pull_file response decision, after the range request:
// 0 proceed, 1 drop the partial then fail, 2 fail and keep the partial.
fn d_resume_action(downloaded: i64, status: i64) -> i64 {
    if downloaded > 0 && status != 206 {
        if status == 416 || status == 200 {
            return 1;
        }
        return 2;
    }
    if downloaded == 0 && status != 200 {
        return 2;
    }
    return 0;
}

fn d_resume_header(downloaded: i64, supports_ranges: i64) -> i64 {
    if supports_ranges != 0 && downloaded > 0 {
        return 1;
    }
    return 0;
}

// ---- exports ---------------------------------------------------------------

export fn dl_split_info(name: const u8[:], out: i64[:], tag_out: u8[:]) -> i64 {
    return d_split_info(name, out, tag_out);
}

export fn dl_quant_bits(name: const u8[:]) -> i64 {
    return d_quant_bits(name);
}

export fn dl_part_count(name: const u8[:]) -> i64 {
    return d_part_count(name);
}

export fn dl_part_name(name: const u8[:], part: i64, out: u8[:]) -> i64 {
    return d_part_name(name, part, out);
}

export fn dl_split_group(
    blob: const u8[:],
    off: i64[:],
    len: i64[:],
    n: i64,
    sel: i64,
    out: i64[:]) -> i64 {
    return d_split_group(blob, off, len, n, sel, out);
}

export fn dl_best_model(
    blob: const u8[:],
    off: i64[:],
    len: i64[:],
    n: i64,
    tag: const u8[:]) -> i64 {
    return d_best_model(blob, off, len, n, tag);
}

export fn dl_best_sibling(
    blob: const u8[:],
    off: i64[:],
    len: i64[:],
    n: i64,
    model: i64,
    keyword: const u8[:],
    tag: const u8[:]) -> i64 {
    return d_best_sibling(blob, off, len, n, model, keyword, tag);
}

export fn dl_name_is_sidecar(name: const u8[:]) -> i64 {
    return d_name_is_sidecar(name);
}

export fn dl_filename_is_model(path: const u8[:]) -> i64 {
    return d_filename_is_model(path);
}

export fn dl_sidecar_index(name: const u8[:]) -> i64 {
    return d_sidecar_index(name);
}

export fn dl_sidecar_keyword(i: i64, out: u8[:]) -> i64 {
    return d_sidecar_keyword(i, out);
}

export fn dl_plan_sidecars(present: i64[:], out: i64[:]) -> i64 {
    return d_plan_sidecars(present, out);
}

export fn dl_split_repo_tag(s: const u8[:], out: i64[:]) -> i64 {
    return d_split_repo_tag(s, out);
}

export fn dl_resume_action(downloaded: i64, status: i64) -> i64 {
    return d_resume_action(downloaded, status);
}

export fn dl_resume_header(downloaded: i64, supports_ranges: i64) -> i64 {
    return d_resume_header(downloaded, supports_ranges);
}
