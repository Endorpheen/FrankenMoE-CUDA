from pathlib import Path
import sys

s = Path(sys.argv[1]).read_text()


def replace(old, new):
    global s
    if s.count(old) != 1:
        raise ValueError(old)
    s = s.replace(old, new)


replace("// ---- composed from goose/sampling/sampler-chain-full.goose ----",
"""// ---- composed from goose/sampling/sampler-chain-full.goose ----
extern "fg_i2_sampler_scan_enabled" fn sm_scan_enabled(count: i64) -> bool;
extern "fg_i2_sampler_first_above" fn sm_first_above(lv: f32[:], start: i64, count: i64, floor: f32) -> i64;
extern "fg_i2_sampler_init_ids" fn sm_init_ids(ids: i64[:], count: i64) -> i64;""")
replace("""    make_heap(lv, id, 0, npartial);
    var i: i64 = npartial;
    while i < n {
        if lv[i] > lv[0] {
            pop_heap_to(lv, id, 0, npartial, i);
        }
        i = i + 1;
    }
    sort_heap""",
"""    make_heap(lv, id, 0, npartial);
    var i: i64 = npartial;
    if npartial < n && sm_scan_enabled(n) {
        while i < n {
            i = sm_first_above(lv, i, n, lv[0]);
            if i == n { break; }
            pop_heap_to(lv, id, 0, npartial, i);
            i += 1;
        }
    } else {
        while i < n {
            if lv[i] > lv[0] {
                pop_heap_to(lv, id, 0, npartial, i);
            }
            i = i + 1;
        }
    }
    sort_heap""")
replace("""    var sel: i64 = 0;
    var i: i64 = 1;
    while i < n {
        if lv[i] > lv[sel]""",
"""    var sel: i64 = 0;
    var i: i64 = 1;
    if sm_scan_enabled(n) {
        while i < n {
            i = sm_first_above(lv, i, n, lv[sel]);
            if i == n { break; }
            sel = i;
            i += 1;
        }
        return sel;
    }
    while i < n {
        if lv[i] > lv[sel]""")
replace("""    while i < n {
        id[i] = i;
        i = i + 1;
    }

    // top_k
    var sorted: i64 = 0;""",
"""    if sm_scan_enabled(n) {
        if sm_init_ids(id, n) != 0 { return -1; }
    } else {
        while i < n {
            id[i] = i;
            i = i + 1;
        }
    }

    // top_k
    var sorted: i64 = 0;""")
Path(sys.argv[2]).write_text(s)
