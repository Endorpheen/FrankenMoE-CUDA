"""Reuse four-row transactions on EXP-2042 and dispatch the two-panel kernel."""
from pathlib import Path
import sys


draft_adapter = Path(sys.argv.pop(1)).resolve()
experiments = Path(__file__).resolve().parents[2]
wide = experiments / "EXP-2026-11-25-2039-bonsai-mtp-three-proposals/source/prepare.py"
text = wide.read_text()
text = text.replace("VERIFY_MERGE,", "VERIFY_ATTN_TAIL,")
text = text.replace('"bonsai_verify_merge_parallel",', '"bonsai_verify_q4_tail",')
text = text.replace('"bonsai_verify_q4_four_gqa2", "bonsai_conv_verify_prefix_four",',
                    '"bonsai_verify_pair_tail3", "bonsai_conv_verify_prefix_four",')
before = "launch(m, VERIFY_ATTN, 12, max_tiles, 1, 32,"
after = "launch(m, m->verify_rows == 4 ? VERIFY_ATTN_FOUR : VERIFY_ATTN, 12, max_tiles, 1, 32,"
assert text.count(before) == text.count(after) == 1
with_tail = "launch(m, m->verify_attn_tail ? VERIFY_ATTN_TAIL : VERIFY_ATTN, 12, max_tiles, 1, 32,"
two_panels = ("launch(m, m->verify_rows == 4 ? VERIFY_ATTN_FOUR : "
              "(m->verify_attn_tail ? VERIFY_ATTN_TAIL : VERIFY_ATTN), "
              "m->verify_rows == 4 ? 8 : 12, max_tiles, m->verify_rows == 4 ? 2 : 1, 32,")
text = text.replace(before, with_tail).replace(after, two_panels)
text = text.replace('(base.parent / "greedy.inc").read_text()', f'Path({str(draft_adapter)!r}).read_text()')
exec(compile(text, str(wide), "exec"), {"__file__": str(wide), "__name__": "__main__"})
