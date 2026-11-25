"""Use the exact compact merge only for the existing shared verifier tiles."""
from pathlib import Path
import sys


def replace(text, old, new):
    assert text.count(old) == 1, old
    return text.replace(old, new)


backend, operations, output = map(Path, sys.argv[1:])
text = backend.read_text()
text = replace(text, "CHOICE_FINAL, KERNEL_COUNT", "CHOICE_FINAL, VERIFY_MERGE, KERNEL_COUNT")
text = replace(text, "verify_native, verify_attention;", "verify_native, verify_attention, verify_merge;")
text = replace(text, '    m->q8_mode = 1;', '''    const char *verify_merge = getenv("BONSAI_MTP_VERIFY_PARALLEL_MERGE");
    m->verify_merge = verify_merge && strcmp(verify_merge, "1") == 0;
    m->q8_mode = 1;''')
text = replace(text, '"bonsai_choice_final",', '"bonsai_choice_final", "bonsai_verify_merge_parallel",')
(output / "backend.c").write_text(text)

ops = operations.read_text()
ops = replace(ops, "    const size_t partial_bytes = (size_t)24 * max_tiles * 258 * sizeof(float);",
              """    if (m->verify_merge) {
        CUdeviceptr output = p[3];
        return launch(m, VERIFY_MERGE, VERIFY_ROWS * 24, 1, 1, 128,
                      (void *[]){ &m->partial, &output, &first_keys, &max_tiles });
    }
    const size_t partial_bytes = (size_t)24 * max_tiles * 258 * sizeof(float);""")
(output / "verify-ops.inc").write_text(ops)
