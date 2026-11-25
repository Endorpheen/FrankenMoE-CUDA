"""Bind the validated MTP tensors and add the isolated diagnostic CLI entry point."""
from pathlib import Path
import sys

source, output = map(Path, sys.argv[1:])
backend = source.read_text()


def replace(original, replacement):
    global backend
    assert backend.count(original) == 1, original
    backend = backend.replace(original, replacement)


replace("MTP_EH, KERNEL_COUNT", "MTP_EH, MTP_MAT, KERNEL_COUNT")
replace("int64_t mtp[4];", "int64_t mtp[4];\n    int64_t mtp_layer[15];")
replace('"bonsai_mtp_eh_q4",', '"bonsai_mtp_eh_q4", "bonsai_mtp_mat_q4",')
replace('    fprintf(stderr, "Device GGUF payload:', '''    for (unsigned i = 0; i < 15; ++i) {
        const int64_t index = bound[LAYERS*15+i];
        m->mtp_layer[i] = i == 0 ? index : (index < 0 ? 0 : (int64_t)(m->weights + tensors[index*12+8]));
    }
    fprintf(stderr, "Device GGUF payload:''')
replace("static double seconds(void) {", '#include "head-probe.inc"\n\nstatic double seconds(void) {')
replace("    const double start = seconds();\n    for (unsigned at = 0; at < prompt", '''
    const char *head_output = getenv("BONSAI_MTP_HEAD_CAPTURE");
    if (head_output) {
        if (!status) status = capture_mtp_head(m, ids, prompt, head_output) != 0;
        goto cleanup;
    }
    const double start = seconds();
    for (unsigned at = 0; at < prompt''')
replace("    free(ids);\n    free(logits);", "cleanup:\n    free(ids);\n    free(logits);")
output.write_text(backend)
