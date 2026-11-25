"""Add read-only trace outputs to the EXP-2025 diagnostic adapter."""
from pathlib import Path
import sys


def replace(text, old, new):
    assert text.count(old) == 1, old
    return text.replace(old, new)


probe, output = map(Path, sys.argv[1:])
text = probe.read_text()
text = replace(text, "    FILE *file = NULL;", "    FILE *file = NULL, *hidden_file = NULL;")
text = replace(text, '    file = fopen(path, "wb");',
               '''    const char *hidden_path = getenv("BONSAI_MTP_HIDDEN_CAPTURE");
    if (!hidden_path || !(hidden_file = fopen(hidden_path, "wb")) ||
        fwrite(hidden, hidden_bytes, count, hidden_file) != count) goto done;
    file = fopen(path, "wb");''')
text = replace(text, '                dump_device_row(file, m->b[13], HIDDEN) ||',
               '                dump_device_row(file, m->b[12], HIDDEN) ||\n'
               '                dump_device_row(file, m->b[13], HIDDEN) ||')
text = replace(text, '    if (file && fclose(file) != 0) status = -1;',
               '    if (file && fclose(file) != 0) status = -1;\n'
               '    if (hidden_file && fclose(hidden_file) != 0) status = -1;')
output.write_text(text)
