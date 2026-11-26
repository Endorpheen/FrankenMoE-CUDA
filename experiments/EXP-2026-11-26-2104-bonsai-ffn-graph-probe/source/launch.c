#include <cuda.h>
#include <stddef.h>

struct dispatch {
    CUfunction function;
    unsigned grid_x;
    unsigned threads;
    void **params;
};

int issue_ffn(const struct dispatch *nodes, size_t count, CUstream stream) {
    for (size_t i = 0; i < count; ++i) {
        const struct dispatch *node = &nodes[i];
        CUresult status = cuLaunchKernel(node->function, node->grid_x, 1, 1,
            node->threads, 1, 1, 0, stream, node->params, NULL);
        if (status != CUDA_SUCCESS) return status;
    }
    return CUDA_SUCCESS;
}

int issue_graphs(const CUgraphExec *graphs, size_t count, CUstream stream) {
    for (size_t i = 0; i < count; ++i) {
        CUresult status = cuGraphLaunch(graphs[i], stream);
        if (status != CUDA_SUCCESS) return status;
    }
    return CUDA_SUCCESS;
}
