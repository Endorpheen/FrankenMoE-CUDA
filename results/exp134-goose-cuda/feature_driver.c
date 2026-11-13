#define main goose_feature_main
#include "feature_probe.c"
#undef main
int main(int argc, char **argv) {
    int result = goose_feature_main(argc, argv);
    printf("CPU SIMD dispatch level: %d\n", gs_simd_level());
    return result;
}
