"""Add phase markers and actual CPU consumption to an unchanged kernel build."""
from pathlib import Path
import sys

p=Path(sys.argv[1])
s=p.read_text()


def replace(a,b):
    global s
    if s.count(a)!=1:
        raise ValueError(a)
    s=s.replace(a,b)


replace('#include <time.h>', '''#include <time.h>

extern int nvtxRangePushA(const char *);
extern int nvtxRangePop(void);

static double cpu_seconds(void) {
    struct timespec now;
    clock_gettime(CLOCK_PROCESS_CPUTIME_ID,&now);
    return (double)now.tv_sec + (double)now.tv_nsec / 1000000000;
}''')
replace('    model *m = bonsai_open(', '''    const double cpu_load = cpu_seconds();
    nvtxRangePushA("bonsai.model_load");
    model *m = bonsai_open(''')
replace('    if (!m) return 1;\n    float *logits', '''    nvtxRangePop();
    fprintf(stderr,"cpu_load_ms=%.3f\\n",1000*(cpu_seconds()-cpu_load));
    if (!m) return 1;
    float *logits''')
replace('    const double start = seconds();', '''    const double cpu_prefill_start = cpu_seconds();
    nvtxRangePushA("bonsai.prefill");
    const double start = seconds();''')
replace('    const double prefill = seconds() - start;', '''    const double prefill = seconds() - start;
    nvtxRangePop();
    fprintf(stderr,"cpu_prefill_ms=%.3f\\n",1000*(cpu_seconds()-cpu_prefill_start));
    const double cpu_decode_start = cpu_seconds();
    nvtxRangePushA("bonsai.decode");''')
replace('    if (!status) fprintf(stderr,\n        "prompt_n=', '''    nvtxRangePop();
    fprintf(stderr,"cpu_decode_ms=%.3f\\n",1000*(cpu_seconds()-cpu_decode_start));
    if (!status) fprintf(stderr,
        "prompt_n=''')
Path(sys.argv[2]).write_text(s)
