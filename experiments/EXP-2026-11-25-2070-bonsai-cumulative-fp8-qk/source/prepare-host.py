"""Connect the opt-in E4M3 path to the long Bonsai prefill dispatch."""

from pathlib import Path
import sys


def replace(text, old, new):
    if text.count(old) != 1:
        raise ValueError(f"Expected one host anchor: {old!r}")
    return text.replace(old, new)


base, out = map(Path, sys.argv[1:])
host = base.read_text()
host = replace(host, "MMQ_HALF_FULL6144, KERNEL_COUNT",
               "MMQ_HALF_FULL6144, FLASH_K_FP8, FLASH_V_ONLY, FLASH_FP8_QK, KERNEL_COUNT")
host = replace(host, "mmq_half_full6144;", "mmq_half_full6144, flash_fp8_qk;")
host = replace(host, '"bonsai_pq2_half_full_6144",',
               '"bonsai_pq2_half_full_6144", "bonsai_flash_stage_k_fp8",'
               ' "bonsai_flash_stage_v_only", "bonsai_flash_fp8_qk",')
host = replace(host, '    const char *fused = getenv("BONSAI_FLASH_FUSED_ORDERED");',
               '''    const char *fp8 = getenv("BONSAI_FLASH_FP8_QK");
    m->flash_fp8_qk = fp8 && strcmp(fp8,"1") == 0 &&
        bonsai_cuda.fn_cuFuncSetAttribute(m->kernel[FLASH_FP8_QK],
            CU_FUNC_ATTRIBUTE_MAX_DYNAMIC_SHARED_SIZE_BYTES, 41472) == CUDA_SUCCESS;
    const char *fused = getenv("BONSAI_FLASH_FUSED_ORDERED");''')
host = replace(host, "    if (staged && launch(m, FLASH_STAGE_KV", '''    if (panel && ordered && m->flash_fused_ordered && m->flash_fp8_qk &&
            count >= 512 && keys >= 16384) {
        if (launch(m, FLASH_K_FP8, chunks, 4, 1, 128,
                (void *[]){&k,&m->stage_k,&keys,&chunks}) ||
            launch(m, FLASH_V_ONLY, chunks, 4, 1, 128,
                (void *[]){&v,&m->stage_v,&keys,&chunks})) return -1;
        return check(bonsai_cuda.fn_cuLaunchKernel(m->kernel[FLASH_FP8_QK],
            groups/2, 1, 1, 256, 1, 1, 41472, NULL,
            (void *[]){&q,&m->stage_k,&m->stage_v,&dst,&count,&keys,&first,&tiles,&(unsigned){0},&chunks},
            NULL), "Bonsai FP8 QK");
    }
    if (staged && launch(m, FLASH_STAGE_KV''')
out.write_text(host)
