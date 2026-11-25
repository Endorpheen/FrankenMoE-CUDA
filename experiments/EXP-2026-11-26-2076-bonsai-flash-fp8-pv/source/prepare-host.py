"""Connect FP8 PV to the existing long-context FP8 QK dispatch."""

from pathlib import Path
import sys


def replace(text, old, new):
    if text.count(old) != 1:
        raise ValueError(f"Expected one host anchor: {old!r}")
    return text.replace(old, new)


base, out = map(Path, sys.argv[1:])
host = base.read_text()
host = replace(host, "FFN_Q8_FULL128, KERNEL_COUNT",
               "FFN_Q8_FULL128, FLASH_V_FP8, FLASH_FP8_QKPV, KERNEL_COUNT")
host = replace(host, "flash_fp8_qk, mmq_half2_down;",
               "flash_fp8_qk, mmq_half2_down, flash_fp8_pv;")
host = replace(host, '"bonsai_pq2_mmq_int8_full_n128",',
               '"bonsai_pq2_mmq_int8_full_n128", "bonsai_flash_stage_v_fp8", "bonsai_flash_fp8_qkpv",')
host = replace(host, '    const char *fp8 = getenv("BONSAI_FLASH_FP8_QK");',
               '''    const char *pv = getenv("BONSAI_FLASH_FP8_PV");
    m->flash_fp8_pv = pv && strcmp(pv,"1") == 0 &&
        bonsai_cuda.fn_cuFuncSetAttribute(m->kernel[FLASH_FP8_QKPV],
            CU_FUNC_ATTRIBUTE_MAX_DYNAMIC_SHARED_SIZE_BYTES, 41472) == CUDA_SUCCESS;
    const char *fp8 = getenv("BONSAI_FLASH_FP8_QK");''')
host = replace(host, "launch(m, FLASH_V_ONLY, chunks, 4, 1, 128,",
               "launch(m, m->flash_fp8_pv ? FLASH_V_FP8 : FLASH_V_ONLY, chunks, 4, 1, 128,")
host = replace(host, "fn_cuLaunchKernel(m->kernel[FLASH_FP8_QK],",
               "fn_cuLaunchKernel(m->kernel[m->flash_fp8_pv ? FLASH_FP8_QKPV : FLASH_FP8_QK],")
out.write_text(host)
