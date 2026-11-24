#include <stdint.h>

extern int64_t __real_fg_cuda_expert_upload(int64_t offset, uint64_t source, int64_t bytes);
extern int64_t __real_fg_cuda_expert_stage_upload(int64_t offset, uint64_t source, int64_t bytes);

int64_t __wrap_fg_cuda_expert_upload(int64_t offset, uint64_t source, int64_t bytes) {
    return bytes == 0 ? 0 : __real_fg_cuda_expert_upload(offset, source, bytes);
}

int64_t __wrap_fg_cuda_expert_stage_upload(int64_t offset, uint64_t source, int64_t bytes) {
    return bytes == 0 ? 0 : __real_fg_cuda_expert_stage_upload(offset, source, bytes);
}
