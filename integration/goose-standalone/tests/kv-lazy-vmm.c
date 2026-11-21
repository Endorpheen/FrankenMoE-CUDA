#include "goose-unit.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

extern void fg143_cpu_init(int argc, char ** argv);
extern int64_t fg_cuda_load_model_state(sl_i64 ranges, int64_t bytes);
extern int64_t fg_cuda_reset_model_state(void);
extern int64_t fg_cuda_state_write(uint64_t host, int64_t bytes);
extern int64_t fg_cuda_state_read(uint64_t host, int64_t bytes);
extern int64_t fg_cuda_model_state_bytes(void);
extern int64_t fg_cuda_model_state_startup_bytes(void);
extern int64_t fg_model_zero_host_pages(uint64_t address, int64_t bytes);

static void check_bytes(const unsigned char * bytes, size_t count, unsigned char expected) {
    for (size_t i = 0; i < count; ++i) assert(bytes[i] == expected);
}

static void check_host_pages(void) {
    const long page = sysconf(_SC_PAGESIZE);
    assert(page > 0);
    const size_t bytes = 4 * (size_t) page;
    unsigned char * memory = mmap(NULL, bytes, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    assert(memory != MAP_FAILED);
    memset(memory, 0x29, bytes);
    assert(fg_model_zero_host_pages((uintptr_t) memory + 1, bytes - 2) == 1);
    unsigned char flags[4];
    assert(mincore(memory, bytes, flags) == 0);
    assert((flags[1] & 1) == 0 && (flags[2] & 1) == 0);
    check_bytes(memory + 1, bytes - 2, 0);
    assert(memory[0] == 0x29 && memory[bytes - 1] == 0x29);
    memory[page] = 73;
    assert(fg_model_zero_host_pages((uintptr_t) memory + 1, bytes - 2) == 1);
    assert(memory[page] == 0);
    assert(munmap(memory, bytes) == 0);
    puts("Host KV discards whole pages, zeroes edges and preserves adjacent bytes");
}

int main(int argc, char ** argv) {
    if (argc == 2 && strcmp(argv[1], "--host-pages") == 0) {
        check_host_pages();
        return 0;
    }
    enum { mib = 1024 * 1024, span = 5 * mib, probe = 256 };
    unsigned char * key = malloc(span), * value = malloc(span);
    unsigned char recurrent[512];
    assert(key && value);
    int64_t ranges[] = {
        (int64_t) (uintptr_t) key, span, 0, 0,
        (int64_t) (uintptr_t) recurrent, sizeof(recurrent), span, 1,
        (int64_t) (uintptr_t) value, span, span + sizeof(recurrent), 0,
    };
    const int64_t logical = 2 * span + sizeof(recurrent);
    goose_init(argc, argv);
    fg143_cpu_init(argc, argv);
    assert(fg_cuda_load_model_state((sl_i64) {ranges, 12}, logical) == logical);
    assert(fg_cuda_model_state_bytes() == logical);
    const int64_t startup = fg_cuda_model_state_startup_bytes();
    assert(startup == sizeof(recurrent) || startup == logical);
    if (argc == 2) assert(startup == strtoll(argv[1], NULL, 10));
    printf("KV logical bytes: %lld, eager startup bytes: %lld\n", (long long) logical, (long long) startup);
    memset(key, 0xff, probe);
    assert(fg_cuda_state_read((uintptr_t) key, probe) == 0);
    check_bytes(key, probe, 0);
    memset(key, 0x34, probe);
    assert(fg_cuda_state_write((uintptr_t) key, probe) == 0);
    unsigned char * tail = key + 3 * mib;
    memset(tail, 0xff, probe);
    assert(fg_cuda_state_read((uintptr_t) tail, probe) == 0);
    check_bytes(tail, probe, 0);
    unsigned char * last = key + span - probe;
    memset(last, 0xff, probe);
    assert(fg_cuda_state_read((uintptr_t) last, probe) == 0);
    check_bytes(last, probe, 0);
    memset(key, 0xff, probe);
    assert(fg_cuda_state_read((uintptr_t) key, probe) == 0);
    check_bytes(key, probe, 0x34);
    memset(value, 0x19, probe);
    assert(fg_cuda_state_write((uintptr_t) value, probe) == 0);
    memset(recurrent, 0x56, sizeof(recurrent));
    assert(fg_cuda_state_write((uintptr_t) recurrent, sizeof(recurrent)) == 0);
    assert(fg_cuda_reset_model_state() == 0);
    assert(fg_cuda_state_read((uintptr_t) key, probe) == 0);
    assert(fg_cuda_state_read((uintptr_t) value, probe) == 0);
    assert(fg_cuda_state_read((uintptr_t) recurrent, sizeof(recurrent)) == 0);
    check_bytes(key, probe, 0);
    check_bytes(value, probe, 0);
    check_bytes(recurrent, sizeof(recurrent), 0);
    assert(fg_cuda_model_state_startup_bytes() == startup);
    puts("KV growth and recurrent reset passed");
    free(value);
    free(key);
}
