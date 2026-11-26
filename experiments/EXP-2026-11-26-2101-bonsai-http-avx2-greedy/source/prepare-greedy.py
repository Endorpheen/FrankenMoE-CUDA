"""Use the existing checked AVX2 scanner for Bonsai's strict greedy choice."""

from pathlib import Path
import sys


base, target = map(Path, sys.argv[1:])
text = base.read_text()
anchor = '''fn sm_greedy(lv: f32[:], n: i64) -> i64 {
    if n <= 0 || lv.len < n {
        return -1;
    }
    var sel: i64 = 0;'''
replacement = '''extern "fg_bonsai_host_scan_enabled" fn sm_bonsai_scan_enabled() -> bool;

fn sm_greedy(lv: f32[:], n: i64) -> i64 {
    if n <= 0 || lv.len < n {
        return -1;
    }
    var sel: i64 = 0;
    if sm_bonsai_scan_enabled() {
        var best = lv[0];
        while sel + 1 < n {
            let next = sm_topk_first_above(lv, sel + 1, n, best);
            if next >= n { break; }
            sel = next;
            best = lv[sel];
        }
        return sel;
    }'''
assert text.count(anchor) == 1
assert 'fn sm_topk_first_above(' in text
target.write_text(text.replace(anchor, replacement))
