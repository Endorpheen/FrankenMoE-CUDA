#include <cuda.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { heads = 16, kv_heads = 2, dimension = 256, blocks_q4 = dimension / 32 };

static void check(CUresult status, const char *call) {
    if (status == CUDA_SUCCESS) return;
    const char *message = NULL;
    cuGetErrorString(status, &message);
    fprintf(stderr, "%s: %s\n", call, message ? message : "CUDA error");
    exit(2);
}
#define CHECK(call) check((call), #call)

static void launch(CUfunction f, int x, int y, int lanes, void **args, CUstream stream) {
    CHECK(cuLaunchKernel(f, x, y, 1, lanes, 1, 1, 0, stream, args, NULL));
}

static float measure(CUgraphExec graph, CUstream stream, CUevent start, CUevent finish) {
    enum { warmups = 8, replays = 96 };
    for (int i = 0; i < warmups; ++i) CHECK(cuGraphLaunch(graph, stream));
    CHECK(cuEventRecord(start, stream));
    for (int i = 0; i < replays; ++i) CHECK(cuGraphLaunch(graph, stream));
    CHECK(cuEventRecord(finish, stream));
    CHECK(cuEventSynchronize(finish));
    float ms;
    CHECK(cuEventElapsedTime(&ms, start, finish));
    return ms * 1000 / replays;
}

static void random_q4(uint8_t *data, size_t bytes, uint32_t seed) {
    for (size_t i = 0; i < bytes; i += 18) {
        data[i] = 0;
        data[i + 1] = (uint8_t)(44 + (i / 18) % 6);
        for (unsigned j = 2; j < 18; ++j) {
            seed = seed * UINT32_C(1664525) + UINT32_C(1013904223);
            data[i + j] = (uint8_t)(seed >> 24);
        }
    }
}

static void equal(const char *label, int keys, int tiles, CUdeviceptr left, CUdeviceptr right, size_t bytes) {
    uint8_t *a = malloc(bytes), *b = malloc(bytes);
    if (!a || !b) exit(2);
    CHECK(cuMemcpyDtoH(a, left, bytes));
    CHECK(cuMemcpyDtoH(b, right, bytes));
    if (memcmp(a, b, bytes) != 0) {
        size_t i = 0;
        while (i < bytes && a[i] == b[i]) ++i;
        fprintf(stderr, "%s mismatch n=%d parts=%d float=%zu\n", label, keys, tiles, i / sizeof(float));
        exit(1);
    }
    free(a);
    free(b);
}

static void probe(CUfunction pack, CUfunction original, CUfunction candidate, CUfunction combine,
                  CUevent before, CUevent after, int keys, int tiles, int numeric_case) {
    enum { layers = 10 };
    const int64_t per_head = blocks_q4 * 18, row_bytes = per_head * kv_heads;
    const size_t kv_bytes = (size_t)keys * row_bytes;
    const size_t part_bytes = (size_t)heads * tiles * dimension * sizeof(float);
    const size_t meta_bytes = (size_t)heads * tiles * 2 * sizeof(float);
    float q[heads * dimension];
    uint8_t *k = malloc(kv_bytes), *v = malloc(kv_bytes);
    if (!k || !v) exit(2);
    for (int i = 0; i < heads * dimension; ++i) {
        q[i] = numeric_case ? (float)((i * 43 + i / 13) % 97 - 48) / 32 : 0;
    }
    random_q4(k, kv_bytes, 31);
    random_q4(v, kv_bytes, 127);
    int64_t cfg[22] = {0};
    cfg[3] = heads;
    cfg[4] = kv_heads;
    cfg[8] = dimension * sizeof(float);
    cfg[9] = cfg[11] = row_bytes;
    cfg[10] = cfg[12] = per_head;
    CUdeviceptr q_dev, q_pack, k_dev, v_dev, cfg_dev, parts[2], meta[2], output[2];
    CHECK(cuMemAlloc(&q_dev, sizeof q));
    CHECK(cuMemAlloc(&q_pack, heads * 320));
    CHECK(cuMemAlloc(&k_dev, layers * kv_bytes));
    CHECK(cuMemAlloc(&v_dev, layers * kv_bytes));
    CHECK(cuMemAlloc(&cfg_dev, sizeof cfg));
    for (int arm = 0; arm < 2; ++arm) {
        CHECK(cuMemAlloc(&parts[arm], part_bytes));
        CHECK(cuMemAlloc(&meta[arm], meta_bytes));
        CHECK(cuMemAlloc(&output[arm], layers * sizeof q));
        CHECK(cuMemsetD8(parts[arm], 0, part_bytes));
        CHECK(cuMemsetD8(meta[arm], 0, meta_bytes));
        CHECK(cuMemsetD8(output[arm], 0, layers * sizeof q));
    }
    CHECK(cuMemcpyHtoD(q_dev, q, sizeof q));
    for (unsigned layer = 0; layer < layers; ++layer) {
        random_q4(k, kv_bytes, 31 + layer);
        random_q4(v, kv_bytes, 127 + layer);
        CHECK(cuMemcpyHtoD(k_dev + layer * kv_bytes, k, kv_bytes));
        CHECK(cuMemcpyHtoD(v_dev + layer * kv_bytes, v, kv_bytes));
    }
    CHECK(cuMemcpyHtoD(cfg_dev, cfg, sizeof cfg));
    float scale = 0.0625f;
    void *pack_args[] = {&q_dev, &q_pack, &cfg_dev, &scale};
    const CUfunction kernels[] = {original, candidate};
    CUgraph graphs[2];
    CUgraphExec exec[2];
    CUstream stream;
    CHECK(cuStreamCreate(&stream, CU_STREAM_DEFAULT));
    for (int arm = 0; arm < 2; ++arm) {
        CHECK(cuStreamBeginCapture(stream, CU_STREAM_CAPTURE_MODE_THREAD_LOCAL));
        for (unsigned layer = 0; layer < layers; ++layer) {
            CUdeviceptr key = k_dev + layer * kv_bytes;
            CUdeviceptr value = v_dev + layer * kv_bytes;
            CUdeviceptr layer_output = output[arm] + layer * sizeof q;
            int heads_arg = heads;
            void *args[] = {&q_pack, &key, &value, &parts[arm], &meta[arm], &cfg_dev, &keys, &scale};
            void *merge[] = {&parts[arm], &meta[arm], &layer_output, &tiles, &heads_arg};
            launch(pack, heads, 1, 32, pack_args, stream);
            launch(kernels[arm], heads / 4, tiles, 128, args, stream);
            launch(combine, heads, 1, 256, merge, stream);
        }
        CHECK(cuStreamEndCapture(stream, &graphs[arm]));
        CHECK(cuGraphInstantiateWithFlags(&exec[arm], graphs[arm], 0));
        CHECK(cuGraphUpload(exec[arm], stream));
        CHECK(cuGraphLaunch(exec[arm], stream));
        CHECK(cuStreamSynchronize(stream));
    }
    equal("10-layer outputs", keys, tiles, output[0], output[1], layers * sizeof q);
    equal("parts", keys, tiles, parts[0], parts[1], part_bytes);
    equal("metadata", keys, tiles, meta[0], meta[1], meta_bytes);
    float a = measure(exec[0], stream, before, after);
    float b = measure(exec[1], stream, before, after);
    float bb = measure(exec[1], stream, before, after);
    float aa = measure(exec[0], stream, before, after);
    printf("keys=%d parts=%d bitwise=%zu graph_us_10_layers reference=%.3f/%.3f candidate=%.3f/%.3f\n",
        keys, tiles, layers * sizeof q / sizeof(float), a, aa, b, bb);
    for (int arm = 0; arm < 2; ++arm) {
        CHECK(cuGraphExecDestroy(exec[arm]));
        CHECK(cuGraphDestroy(graphs[arm]));
    }
    CHECK(cuStreamDestroy(stream));
    for (int arm = 0; arm < 2; ++arm) {
        CHECK(cuMemFree(output[arm]));
        CHECK(cuMemFree(meta[arm]));
        CHECK(cuMemFree(parts[arm]));
    }
    CHECK(cuMemFree(cfg_dev));
    CHECK(cuMemFree(v_dev));
    CHECK(cuMemFree(k_dev));
    CHECK(cuMemFree(q_pack));
    CHECK(cuMemFree(q_dev));
    free(v);
    free(k);
}

int main(int argc, char **argv) {
    if (argc != 4) return 2;
    CHECK(cuInit(0));
    CUdevice device;
    CUcontext context;
    CUmodule ref, baseline, dut;
    CUfunction pack, original, candidate, combine;
    CUevent a, b;
    CHECK(cuDeviceGet(&device, 0));
    CHECK(cuCtxCreate(&context, 0, device));
    CHECK(cuModuleLoad(&ref, argv[1]));
    CHECK(cuModuleLoad(&baseline, argv[2]));
    CHECK(cuModuleLoad(&dut, argv[3]));
    CHECK(cuModuleGetFunction(&pack, ref, "goose_flash_vec_pack_query"));
    CHECK(cuModuleGetFunction(&original, baseline, "goose_flash_vec_q4_packed_quad"));
    CHECK(cuModuleGetFunction(&candidate, dut, "goose_flash_vec_q4_packed_quad"));
    CHECK(cuModuleGetFunction(&combine, ref, "goose_flash_vec_combine"));
    int a_regs, b_regs;
    CHECK(cuFuncGetAttribute(&a_regs, CU_FUNC_ATTRIBUTE_NUM_REGS, original));
    CHECK(cuFuncGetAttribute(&b_regs, CU_FUNC_ATTRIBUTE_NUM_REGS, candidate));
    printf("Registers reference=%d candidate=%d\n", a_regs, b_regs);
    int resident_a, resident_b;
    CHECK(cuOccupancyMaxActiveBlocksPerMultiprocessor(&resident_a, original, 128, 0));
    CHECK(cuOccupancyMaxActiveBlocksPerMultiprocessor(&resident_b, candidate, 128, 0));
    printf("Active CTAs per SM reference=%d candidate=%d\n", resident_a, resident_b);
    CHECK(cuEventCreate(&a, 0));
    CHECK(cuEventCreate(&b, 0));
    probe(pack, original, candidate, combine, a, b, 17, 2, 0);
    probe(pack, original, candidate, combine, a, b, 129, 2, 1);
    probe(pack, original, candidate, combine, a, b, 32769, 64, 1);
    probe(pack, original, candidate, combine, a, b, 33279, 64, 1);
    probe(pack, original, candidate, combine, a, b, 65537, 64, 1);
    CHECK(cuEventDestroy(b));
    CHECK(cuEventDestroy(a));
    CHECK(cuModuleUnload(dut));
    CHECK(cuModuleUnload(baseline));
    CHECK(cuModuleUnload(ref));
    CHECK(cuCtxDestroy(context));
}
