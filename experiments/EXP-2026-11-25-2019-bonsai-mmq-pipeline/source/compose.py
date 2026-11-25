"""Choose the packed-code reuse path only for the existing K64 dispatch."""
from pathlib import Path
import sys

source,out=map(Path,sys.argv[1:])
s=source.read_text()


def change(a,b):
    global s
    assert s.count(a)==1,a
    s=s.replace(a,b)


change("FLASH_KV_PANEL, KERNEL_COUNT","FLASH_KV_PANEL, MMQ_CACHE64, MMQ_CACHE128, KERNEL_COUNT")
change("    int decay_small, flash_panel;","    int decay_small, flash_panel, mmq_cached;")
change("""                ? (tokens_per_tile == 128 ? MMQ_K64_128 : MMQ_K64_64)""",
       """                ? (m->mmq_cached ? (tokens_per_tile == 128 ? MMQ_CACHE128 : MMQ_CACHE64)
                                 : (tokens_per_tile == 128 ? MMQ_K64_128 : MMQ_K64_64))""")
change('    if (m->k64_mmq) fputs("PQ2 MMQ: K64 exact wide tiles\\n",stderr);',
       '''    if (m->k64_mmq) fputs("PQ2 MMQ: K64 exact wide tiles\\n",stderr);
    const char *cached = getenv("BONSAI_MMQ_CACHED_PQ2");
    m->mmq_cached = !cached || strcmp(cached, "1") == 0;
    if (m->k64_mmq && m->mmq_cached) fputs("PQ2 MMQ: reuse packed weight scales\\n", stderr);''')
change('"bonsai_flash_kv_lean2",','"bonsai_flash_kv_lean2", "bonsai_pq2_cached_64x64_k64", "bonsai_pq2_cached_64x128_k64",')
out.write_text(s)
