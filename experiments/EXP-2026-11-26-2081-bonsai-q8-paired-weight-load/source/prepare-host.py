"""Select the exact paired weight loader on full Bonsai gate/up tiles."""

from pathlib import Path
import sys


def replace(text, old, new):
    if text.count(old) != 1:
        raise ValueError(f"Expected one host anchor: {old!r}")
    return text.replace(old, new)


base, output = map(Path, sys.argv[1:])
host = base.read_text()
host = replace(host, "FLASH_FP8_QKPV, KERNEL_COUNT",
               "FLASH_FP8_QKPV, FFN_Q8_PAIRLOAD, KERNEL_COUNT")
host = replace(host, "ffn_q8, q8_full;", "ffn_q8, q8_full, q8_pairload;")
host = replace(host, '"bonsai_flash_fp8_qkpv",',
               '"bonsai_flash_fp8_qkpv", "bonsai_pq2_mmq_int8_full_n128_pairload",')
host = replace(host, '    const char *ffn_q8 = getenv("BONSAI_FFN_Q8_PREFILL");',
               '''    const char *pairload = getenv("BONSAI_FFN_Q8_PAIRLOAD");
    m->q8_pairload = pairload && strcmp(pairload, "1") == 0;
    if (m->q8_pairload) fputs("Bonsai prefill: paired Q8 weight loads\\n", stderr);
    const char *ffn_q8 = getenv("BONSAI_FFN_Q8_PREFILL");''')
host = replace(host, '''                return launch(m, full_q8 ? FFN_Q8_FULL128 : FFN_Q8_MMQ,''',
               '''                const enum kernel kernel = full_q8 ?
                    (m->q8_pairload ? FFN_Q8_PAIRLOAD : FFN_Q8_FULL128) : FFN_Q8_MMQ;
                return launch(m, kernel,''')
output.write_text(host)
