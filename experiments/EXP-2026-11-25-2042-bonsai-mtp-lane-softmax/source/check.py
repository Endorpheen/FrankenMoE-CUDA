"""Check modified verifier tiles against EXP-2033 and the scalar-query oracle."""
from pathlib import Path


experiments = Path(__file__).resolve().parents[2]
source = experiments / "EXP-2026-11-25-2033-bonsai-mtp-attention-reuse/source/check.py"
text = source.read_text()
text = text.replace('heads = int(mode[-1])', 'heads = 2')
text = text.replace('for mode in ("shared1", "shared2"):', 'for mode in ("control", "lane", "tail", "lane_tail"):')
text = text.replace('("serial", "shared1", "shared2")}', '("control", "lane", "tail", "lane_tail")}')
text = text.replace('("serial", "shared1", "shared2", "shared2", "shared1", "serial") * 3:',
    '("control", "lane", "tail", "lane_tail", "lane_tail", "tail", "lane", "control") * 3:')
old = '''            "shared1": function(image, "bonsai_verify_q4_gqa1"),
            "shared2": function(image, "bonsai_verify_q4_gqa2"),'''
assert text.count(old) == 1
text = text.replace(old, '''            "control": function(image, "bonsai_verify_q4_gqa2"),
            "lane": function(image, "bonsai_verify_q4_lane"),
            "tail": function(image, "bonsai_verify_q4_tail"),
            "lane_tail": function(image, "bonsai_verify_q4_lane_tail"),''')
exec(compile(text, str(source), "exec"), {"__file__": str(source), "__name__": "__main__"})
