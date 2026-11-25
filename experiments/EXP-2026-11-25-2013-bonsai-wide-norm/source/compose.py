"""Select exact RMSNorm width 5120 inside the Bonsai-only dispatcher."""
from pathlib import Path
import sys

base, out = map(Path, sys.argv[1:])
s = base.read_text()


def change(a, b):
    global s
    assert s.count(a) == 1, a
    s = s.replace(a, b)


change("MV_Q8_2, KERNEL_COUNT", "MV_Q8_2, NORM_WIDE_SMALL, NORM_WIDE_LARGE, KERNEL_COUNT")
change("    int q8_mode, in_decode;", "    int q8_mode, in_decode, norm_wide;")
change("        case NORM:\n            return", """        case NORM:
            if (m->norm_wide && mode == 0 && n == 5120 && rows == 1 && stride == n) {
                const int small = count <= 512;
                return launch(m, small ? NORM_WIDE_SMALL : NORM_WIDE_LARGE, count, 1, 1,
                    small ? 512 : 128, (void *[]){ &p[0], &p[1], &p[3], &m->epsilon });
            }
            return""")
change("    CUdevice device;", """    const char *norm = getenv("BONSAI_WIDE_NORM");
    m->norm_wide = !norm || strcmp(norm, "1") == 0;
    if (m->norm_wide) fputs("Bonsai RMSNorm: parallel width 5120\\n", stderr);
    CUdevice device;""")
change('"bonsai_mv_pack_q8", "bonsai_pq2_mvq_q8_1", "bonsai_pq2_mvq_q8_2",',
       '"bonsai_mv_pack_q8", "bonsai_pq2_mvq_q8_1", "bonsai_pq2_mvq_q8_2", "bonsai_norm_wide_512_1", "bonsai_norm_wide_128_0",')
out.write_text(s)
