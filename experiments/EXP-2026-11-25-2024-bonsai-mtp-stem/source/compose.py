"""Add a model-scoped capture of the MTP input projection to the diagnostic CLI."""
from pathlib import Path
import sys

source,out=map(Path,sys.argv[1:])
s=source.read_text()


def change(a,b):
    global s
    assert s.count(a)==1,a
    s=s.replace(a,b)


change("MMQ_CACHE128, KERNEL_COUNT","MMQ_CACHE128, MTP_EH, KERNEL_COUNT")
change("    float epsilon;","    float epsilon;\n    int64_t mtp[4];")
change('    fprintf(stderr, "Device GGUF payload:',
       '''    for (unsigned i = 0; i < 4; ++i) {
        m->mtp[i] = (int64_t)(m->weights + tensors[draft[i]*12+8]);
    }
    fprintf(stderr, "Device GGUF payload:''')
change('"bonsai_pq2_cached_64x128_k64",','"bonsai_pq2_cached_64x128_k64", "bonsai_mtp_eh_q4",')
change("static double seconds(void) {",'''int64_t bonsai_mtp_eh(uint64_t context, int64_t weights, int64_t input, int64_t output) {
    model *m = (model *)(uintptr_t)context;
    CUdeviceptr w = weights, x = input, y = output;
    return launch(m, MTP_EH, 1280, 1, 1, 128, (void *[]){ &w, &x, &y });
}

static int dump_device_row(FILE *file, CUdeviceptr ptr, size_t count) {
    float *buf = malloc(count * sizeof(*buf));
    if (!buf) return -1;
    const int ok = !check(bonsai_cuda.fn_cuMemcpyDtoH(buf, ptr, count * sizeof(*buf)), "MTP capture") &&
        fwrite(buf, sizeof(*buf), count, file) == count;
    free(buf);
    return ok ? 0 : -1;
}

static int capture_mtp_stem(model *m, unsigned token, const char *path) {
    FILE *f = fopen(path, "wb");
    if (!f) return -1;
    int result = dump_device_row(f, (CUdeviceptr)m->b[0], 5120);
    if (!result) result = (int)bonsai_mtp_stem((uintptr_t)m, slice(m->mtp, 4), slice(m->b, 24),
        m->globals[0], token);
    if (!result) result = dump_device_row(f, (CUdeviceptr)m->b[2], 5120);
    if (!result) result = dump_device_row(f, (CUdeviceptr)m->b[7], 10240);
    if (!result) result = dump_device_row(f, (CUdeviceptr)m->b[12], 5120);
    if (fclose(f) != 0) result = -1;
    return result;
}

static double seconds(void) {''')
change("    const double prefill = seconds() - start;",
       '''    const double prefill = seconds() - start;
    const char *stem_output = getenv("BONSAI_MTP_STEM_CAPTURE");
    if (stem_output && !status) {
        const char *probe = getenv("BONSAI_MTP_STEM_TOKEN");
        unsigned long token = probe ? strtoul(probe, &end, 10) : VOCAB;
        if (!probe || *end || end == probe || token >= VOCAB ||
                capture_mtp_stem(m, (unsigned)token, stem_output) != 0) status = 1;
    }''')
out.write_text(s)
