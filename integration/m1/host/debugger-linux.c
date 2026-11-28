#include <sys/prctl.h>

void fg_ggml_allow_debugger(int child) {
    prctl(PR_SET_PTRACER, (unsigned long)child, 0UL, 0UL, 0UL);
}
