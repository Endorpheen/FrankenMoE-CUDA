"""Select the fused FFN tail through the Bonsai graph only."""
from pathlib import Path
import sys

s=Path(sys.argv[1]).read_text()
needle='export fn bonsai_graph('
assert s.count(needle)==1
s=s.replace(needle,'''extern "bonsai_ffn_tail_launch" fn launch_ffn_tail(c: u64, gate: i64, up: i64,
    signs: i64, packed_owner: i64) -> i64;

fn b_ffn_finish(c: u64, b: i64[:]) -> i64 {
    let fused = launch_ffn_tail(c, b[14], b[15], b[22], b[17]);
    if fused > 0 { return 0; }
    if fused < 0 || b_launch(c, 13, b[14], b[15], b[16], 0, 0, 0, 0, 0) != 0 { return -1; }
    return b_rotate(c, b[16], b[22], b[17], 17408, 0, 0);
}

'''+needle)
old='''           b_launch(c, 13, b[14], b[15], b[16], 0, 0, 0, 0, 0) != 0 ||
           b_rotate(c, b[16], b[22], b[17], 17408, 0, 0) != 0 ||'''
assert s.count(old)==1
print(s.replace(old,'           b_ffn_finish(c, b) != 0 ||'),end='')
