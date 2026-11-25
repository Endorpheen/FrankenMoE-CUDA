"""Specialize the measured exact projection and attention templates for four rows."""
from pathlib import Path
import sys


mvq, attention, conv, target = map(Path, sys.argv[1:])
q8 = mvq.read_text().replace("mvq_three", "mvq_four").replace("mvq_q8_three_", "mvq_q8_four_").replace("MVQ_VERIFY", "MVQ_VERIFY_FOUR")
q8 = q8.replace("[3]", "[4]").replace("t < 3", "t < 4").replace("{0, 0, 0}", "{0, 0, 0, 0}")
q4 = attention.read_text().replace("verify_grouped_tiles", "verify_grouped_tiles_four")
q4 = q4.replace("bonsai_verify_q4_gqa", "bonsai_verify_q4_four_gqa").replace("VERIFY_ATTN", "VERIFY_ATTN_GEN_FOUR")
q4 = q4.replace("[3]", "[4]").replace("t < 3", "t < 4").replace("first_keys + 2", "first_keys + 3")
target.write_text("\n".join((q8, q4, conv.read_text())))
