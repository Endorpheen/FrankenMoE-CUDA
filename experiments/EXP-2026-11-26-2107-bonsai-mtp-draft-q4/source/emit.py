"""Generate exact token-sharing variants of the existing Q4 projection."""

from pathlib import Path
import re
import sys

archive, batch, output = map(Path, sys.argv[1:])
source = archive.read_text()
helpers = []
for name in ("local_id", "work_group", "warp_sum", "bonsai_grid_y", "q4_value"):
    match = re.search(rf"^static (?:inline )?(?:uint|float) {name}\([^\n]*?\) \{{\n.*?^\}}",
                      source, flags=re.MULTILINE | re.DOTALL)
    if not match:
        raise ValueError(f"Missing CUDA helper: {name}")
    helpers.append(match.group())
kernel = batch.read_text().split("__kernel void bonsai_mtp_q4_four", 1)[1]
for count in (8, 16, 32):
    variant = kernel.replace("bonsai_grid_y() * 4", f"bonsai_grid_y() * {count}")
    variant = variant.replace("sum[4] = {0, 0, 0, 0}", f"sum[{count}] = {{0}}")
    variant = variant.replace("t < 4", f"t < {count}")
    helpers.append(f"__kernel void bonsai_mtp_q4_{count}" + variant)
output.write_text("\n\n".join(helpers))
