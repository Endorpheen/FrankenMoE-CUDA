import re
from pathlib import Path
import sys

out, module = Path(sys.argv[1]), sys.argv[2]
source = (out / f"{module}.c").read_text()
fragment = source.split("/* ---- prototypes ---- */", 1)[1].split("static void gs_init_globals(void)", 1)[0]
if "gs_sp" in fragment:
    raise SystemExit(f"{module} uses the host stack ABI")
if module == "gdn":
    # Keep launch-shape literals visible to Clang when lowering the device loops.
    fragment = re.sub(r"static void (gdn(?:_body|_decode)?_g\()",
                      r"static __attribute__((always_inline)) void \1", fragment)
    fragment = re.sub(r"^(\s*)(for \(int32_t r_g(?:_\d+)? = 0;)",
                      r"\1#pragma clang loop unroll(full)\n\1\2", fragment, flags=re.MULTILINE)
if module == "vector":
    # Inlining keeps the by-value shape plan in parameter space. Passing its
    # address through a device call would copy the plan to per-thread memory.
    fragment = fragment.replace("static GS_NOINLINE void cv_norm_g(",
                                "static __attribute__((always_inline)) void cv_norm_g(")
if module == "mmq":
    fragment = fragment.replace("static void fg_device_mmq_q4_large_g(",
                                "static __attribute__((always_inline)) void fg_device_mmq_q4_large_g(")
if module in ("q4-attention", "gdn", "rope", "ssm-conv", "fwht", "routing"):
    types = source.split("/* ---- types ---- */", 1)[1].split("/* ---- data ---- */", 1)[0]
    excluded = ("gs_rhdr;", "gs_rref;", "gs_pref;", "sl_u8;")
    if module == "routing":
        excluded += ("a2_f32;", "a8_f32;")
    types = "\n".join(line for line in types.splitlines()
                      if not line.endswith(excluded))
    fragment = types + "\n" + fragment
if module in ("gdn-mma", "flash-mma", "flash-vec", "rms-reference"):
    types = source.split("/* ---- types ---- */", 1)[1].split("/* ---- data ---- */", 1)[0]
    excluded = ("gs_rhdr;", "gs_rref;", "gs_pref;", "sl_u8;")
    if module != "flash-vec":
        excluded += ("a8_f32;",)
    types = "\n".join(line for line in types.splitlines()
                      if not line.endswith(excluded))
    if module == "gdn-mma":
        import re

        def gdn_prefix(text):
            return re.sub(r"\ba(?:4|16)_f32\b", lambda m: "GdnMma_" + m[0], text)

        types, fragment = gdn_prefix(types), gdn_prefix(fragment)
    if module == "flash-mma":
        import re

        def prefix(text):
            return re.sub(r"\b(?:F[24]_g|U[24]_g|a\d+_(?:f32|u32))\b", lambda m: "Flash_" + m[0], text)

        types, fragment = prefix(types), prefix(fragment)
    if module == "flash-vec":
        import re

        def vector_prefix(text):
            return re.sub(r"\ba\d+_(?:f32|i32|u32)\b", lambda m: "FlashVec_" + m[0], text)

        types, fragment = vector_prefix(types), vector_prefix(fragment)
    (out / f"{module}-types.h").write_text(types)
if module == "q4-attention":
    fragment = re.sub(r"\ba16_f32\b", "mla_a16_f32", fragment)
    fragment = re.sub(
        r"static void (giga_mla_mma_pv_(?:tile|indexed)_g\()",
        r"static __attribute__((always_inline)) void \1",
        fragment,
    )
(out / f"{module}-device.h").write_text(fragment)
if module == "mmq-quant":
    types = source.split("/* ---- types ---- */", 1)[1].split("/* ---- data ---- */", 1)[0]
    start = types.index("typedef struct MmqQ8Result_g ")
    end = types.index("};", start) + 2
    (out / "mmq-quant-types.h").write_text(types[start:end] + "\n")
