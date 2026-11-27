#include <stdio.h>
#include <string.h>

int gooseinfer_cuda_entry(int argc, char **argv);
int gooseinfer_vulkan_entry(int argc, char **argv);

int main(int argc, char **argv) {
    if (argc > 1 && strncmp(argv[1], "--backend=", 10) == 0) {
        const char *backend = argv[1] + 10;
        int (*entry)(int, char **) = NULL;
        if (strcmp(backend, "cuda") == 0) entry = gooseinfer_cuda_entry;
        if (strcmp(backend, "vulkan") == 0) entry = gooseinfer_vulkan_entry;
        if (!entry) {
            fprintf(stderr, "Unsupported backend '%s'. Choose cuda or vulkan.\n", backend);
            return 2;
        }
        argv[1] = argv[0];
        return entry(argc - 1, argv + 1);
    }
    return gooseinfer_cuda_entry(argc, argv);
}
