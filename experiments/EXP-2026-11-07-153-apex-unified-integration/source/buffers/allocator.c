#include "/home/end0/AI/LlamaServer/franken-goose-exp153-apex-core-20261107/source/graph-allocator/bridge.h"
#include "/home/end0/AI/LlamaServer/franken-goose-exp153-apex-core-20261107/source/memory/bridge.h"
#include "bridge.h"
#include "ggml-alloc.h"
#include "ggml-backend-impl.h"
#include "ggml.h"
#include "ggml-impl.h"

#include <assert.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdatomic.h>

#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define MAX_FREE_BLOCKS 256

//#define GGML_ALLOCATOR_DEBUG

//#define AT_PRINTF(...) GGML_LOG_DEBUG(__VA_ARGS__)
#define AT_PRINTF(...)

// ops that return true for this function must not use restrict pointers for their backend implementations
bool ggml_op_can_inplace(enum ggml_op op) {
    switch (op) {
        case GGML_OP_FILL:
        case GGML_OP_SCALE:
        case GGML_OP_DIAG_MASK_ZERO:
        case GGML_OP_DIAG_MASK_INF:
        case GGML_OP_ADD:
        case GGML_OP_ADD_ID:
        case GGML_OP_ADD1:
        case GGML_OP_SUB:
        case GGML_OP_MUL:
        case GGML_OP_DIV:
        case GGML_OP_SQR:
        case GGML_OP_SQRT:
        case GGML_OP_LOG:
        case GGML_OP_UNARY:
        case GGML_OP_ROPE:
        case GGML_OP_ROPE_BACK:
        case GGML_OP_SILU_BACK:
        case GGML_OP_RMS_NORM:
        case GGML_OP_RMS_NORM_BACK:
        case GGML_OP_CLAMP:
        case GGML_OP_SOFT_MAX:
        case GGML_OP_SOFT_MAX_BACK:
            return true;

        default:
            return false;
    }
}

static size_t aligned_offset(const void * buffer, size_t offset, size_t alignment) {
    assert(alignment && !(alignment & (alignment - 1))); // power of 2
    size_t align = (alignment - (((uintptr_t)buffer + offset) % alignment)) % alignment;
    return offset + align;
}

// tallocr

struct ggml_tallocr ggml_tallocr_new(ggml_backend_buffer_t buffer) {
    if(fg153_buffers_enabled()){struct ggml_tallocr result;fg153_buffers(2,(intptr_t)&result,(intptr_t)buffer,0);return result;}
    void * base = ggml_backend_buffer_get_base(buffer);
    size_t align = ggml_backend_buffer_get_alignment(buffer);

    assert(align && !(align & (align - 1))); // power of 2

    struct ggml_tallocr talloc = (struct ggml_tallocr) {
        /*.buffer    = */ buffer,
        /*.base      = */ base,
        /*.alignment = */ align,
        /*.offset    = */ aligned_offset(base, 0, align),
    };
    return talloc;
}

enum ggml_status ggml_tallocr_alloc(struct ggml_tallocr * talloc, struct ggml_tensor * tensor) {
    if(fg153_buffers_enabled()){return (enum ggml_status)fg153_buffers(3,(intptr_t)talloc,(intptr_t)tensor,0);}
    size_t size = ggml_backend_buffer_get_alloc_size(talloc->buffer, tensor);
    size = GGML_PAD(size, talloc->alignment);

    if (talloc->offset + size > ggml_backend_buffer_get_size(talloc->buffer)) {
        GGML_LOG_ERROR("%s: not enough space in the buffer to allocate %s (needed %zu, available %zu)\n",
                __func__, tensor->name, size, ggml_backend_buffer_get_size(talloc->buffer) - talloc->offset);
        GGML_ABORT("not enough space in the buffer");
    }

    void * addr = (char *)ggml_backend_buffer_get_base(talloc->buffer) + talloc->offset;
    talloc->offset += size;

    assert(((uintptr_t)addr % talloc->alignment) == 0);

    return ggml_backend_tensor_alloc(talloc->buffer, tensor, addr);
}

// dynamic tensor allocator

#define GGML_VBUFFER_MAX_CHUNKS 16

// relative memory address within an allocation that can be split into multiple buffers (chunks)
struct buffer_address {
    int chunk;     // index of a backend buffer
    size_t offset; // local memory offset within the buffer
};

static const struct buffer_address GGML_BUFFER_ADDRESS_INVALID = { -1, SIZE_MAX };

static bool ggml_buffer_address_less(struct buffer_address a, struct buffer_address b) {
    return a.chunk != b.chunk ? a.chunk < b.chunk : a.offset < b.offset;
}

struct free_block {
    size_t offset;
    size_t size;
};

struct tallocr_chunk {
    struct free_block free_blocks[MAX_FREE_BLOCKS];
    int n_free_blocks;
    size_t max_size;
};

struct ggml_dyn_tallocr {
    size_t alignment;
    size_t max_chunk_size;
    struct tallocr_chunk * chunks[GGML_VBUFFER_MAX_CHUNKS];
    int n_chunks;

#ifdef GGML_ALLOCATOR_DEBUG
    struct {
        const struct ggml_tensor * tensor;
        struct buffer_address addr;
    } allocated_tensors[1024];
#endif
};

static void ggml_dyn_tallocr_insert_block(struct tallocr_chunk * chunk, size_t offset, size_t size) {
    GGML_ASSERT(chunk->n_free_blocks < MAX_FREE_BLOCKS && "out of free blocks");
    // insert the new block in the correct position to keep the array sorted by address (to make merging blocks faster)
    int insert_pos = 0;
    while (insert_pos < chunk->n_free_blocks && chunk->free_blocks[insert_pos].offset < offset) {
        insert_pos++;
    }
    // shift all blocks from insert_pos onward to make room for the new block
    for (int i = chunk->n_free_blocks; i > insert_pos; i--) {
        chunk->free_blocks[i] = chunk->free_blocks[i-1];
    }
    // insert the new block
    chunk->free_blocks[insert_pos].offset = offset;
    chunk->free_blocks[insert_pos].size = size;
    chunk->n_free_blocks++;
}

static void ggml_dyn_tallocr_remove_block(struct tallocr_chunk * chunk, int idx) {
    // shift all elements after idx by 1 to the left, overwriting the element at idx
    for (int i = idx; i < chunk->n_free_blocks - 1; i++) {
        chunk->free_blocks[i] = chunk->free_blocks[i+1];
    }
    chunk->n_free_blocks--;
}

static int ggml_dyn_tallocr_new_chunk(struct ggml_dyn_tallocr * alloc, size_t min_size) {
    if(fg153_memory_enabled()){return fg153_memory(3,(intptr_t)alloc,min_size,0,0);}
    if (alloc->n_chunks >= GGML_VBUFFER_MAX_CHUNKS) {
        return -1;
    }
    struct tallocr_chunk * chunk = calloc(1, sizeof(struct tallocr_chunk));
    chunk->n_free_blocks = 1;
    chunk->free_blocks[0].offset = 0;
    // available space in a chunk is limited to max_chunk_size, but can be higher if:
    // 1. a single tensor exceeds the maximum, and cannot fit any other way
    // 2. we are running out of chunks
    // backends will either manage to allocate the larger size, or report an error.
    chunk->free_blocks[0].size = MAX(min_size, alloc->max_chunk_size);
    if (alloc->n_chunks == GGML_VBUFFER_MAX_CHUNKS - 1) {
        chunk->free_blocks[0].size = SIZE_MAX/2;
    }
    alloc->chunks[alloc->n_chunks] = chunk;
    alloc->n_chunks++;
    return alloc->n_chunks - 1;
}

#ifdef GGML_ALLOCATOR_DEBUG
static void add_allocated_tensor(struct ggml_dyn_tallocr * alloc, struct buffer_address addr, const struct ggml_tensor * tensor) {
    for (int i = 0; i < 1024; i++) {
        if (alloc->allocated_tensors[i].tensor == NULL) {
            alloc->allocated_tensors[i].tensor = tensor;
            alloc->allocated_tensors[i].addr = addr;
            return;
        }
    }
    GGML_ABORT("out of allocated_tensors");
}
static void remove_allocated_tensor(struct ggml_dyn_tallocr * alloc, struct buffer_address addr, const struct ggml_tensor * tensor) {
    for (int i = 0; i < 1024; i++) {
        if (alloc->allocated_tensors[i].addr.chunk == addr.chunk && alloc->allocated_tensors[i].addr.offset == addr.offset) {
            alloc->allocated_tensors[i].tensor = NULL;
            return;
        }
    }
    GGML_ABORT("tried to free tensor %s not found\n", tensor->name);
}
#endif


_Static_assert(sizeof(struct free_block) == 16, "Goose free-block ABI");
_Static_assert(offsetof(struct free_block, size) == 8, "Goose block size offset");
__attribute__((visibility("hidden"))) int64_t fg_selfhost_choose_reuse(const void *, int, size_t, size_t *);
static bool fg_alloc_enabled = true;
static bool fg_alloc_verify = false;
static int64_t fg_native_choose_reuse(const struct free_block *blocks, int count, size_t requested, size_t *available) {
    int64_t index = -1;
    size_t best_size = SIZE_MAX;
    *available = 0;
    for (int i = 0; i < count - 1; ++i) {
        *available = MAX(*available, blocks[i].size);
        if (blocks[i].size >= requested && blocks[i].size <= best_size) {
            index = i;
            best_size = blocks[i].size;
        }
    }
    return index;
}
__attribute__((constructor)) static void fg_alloc_config(void) {
    const char *on = getenv("GGML_GOOSE_ALLOC");
    const char *verify = getenv("GGML_GOOSE_ALLOC_VERIFY");
    if (on && atoi(on) == 0) fg_alloc_enabled = false;
    fg_alloc_verify = verify && atoi(verify) != 0;
}
#ifdef FG_ALLOC_TRACE
static atomic_uint_fast64_t fg_alloc_calls, fg_alloc_hits, fg_alloc_checks, fg_alloc_digest;
__attribute__((destructor)) static void fg_alloc_report(void) {
    fprintf(stderr, "FG_SELFHOST_ALLOC enabled=%d calls=%llu reuse_hits=%llu shadow_checks=%llu digest=%llu\n", fg_alloc_enabled,
        (unsigned long long)atomic_load(&fg_alloc_calls), (unsigned long long)atomic_load(&fg_alloc_hits),
        (unsigned long long)atomic_load(&fg_alloc_checks), (unsigned long long)atomic_load(&fg_alloc_digest));
}
#endif
static struct buffer_address ggml_dyn_tallocr_alloc(struct ggml_dyn_tallocr * alloc, size_t size, const struct ggml_tensor * tensor) {
    if(fg153_memory_enabled()){struct buffer_address result;fg153_memory(12,(intptr_t)alloc,size,(intptr_t)&result,0);return result;}
    size = aligned_offset(NULL, size, alloc->alignment);

    AT_PRINTF("%s: allocating %s (%zu bytes) - ", __func__, tensor->name, size);

    int best_fit_chunk = -1;
    int best_fit_block = -1;
    size_t max_avail = 0;

    // find the best fitting free block besides the last block, within any chunk
    for (int c = 0; c < alloc->n_chunks; ++c) {
        struct tallocr_chunk * chunk = alloc->chunks[c];
        size_t available;
        int64_t choice = fg_alloc_enabled ? fg_selfhost_choose_reuse(chunk->free_blocks, chunk->n_free_blocks, size, &available)
            : fg_native_choose_reuse(chunk->free_blocks, chunk->n_free_blocks, size, &available);
        if (fg_alloc_enabled && fg_alloc_verify) {
            size_t expected_available;
            int64_t expected = fg_native_choose_reuse(chunk->free_blocks, chunk->n_free_blocks, size, &expected_available);
            GGML_ASSERT(choice == expected && available == expected_available);
#ifdef FG_ALLOC_TRACE
            atomic_fetch_add_explicit(&fg_alloc_checks, 1, memory_order_relaxed);
#endif
        }
        max_avail = MAX(max_avail, available);
        if (choice >= 0) {
            best_fit_chunk = c;
            best_fit_block = (int)choice;
        }
#ifdef FG_ALLOC_TRACE
        atomic_fetch_add_explicit(&fg_alloc_calls, 1, memory_order_relaxed);
        atomic_fetch_add_explicit(&fg_alloc_hits, choice >= 0, memory_order_relaxed);
        uint64_t fingerprint = size * UINT64_C(11400714819323198485) ^ available ^ (uint64_t)(choice + 1)
            ^ ((uint64_t)chunk->n_free_blocks << 32) ^ ((uint64_t)c << 48);
        atomic_fetch_add_explicit(&fg_alloc_digest, fingerprint, memory_order_relaxed);
#endif
    }

    if (best_fit_block == -1) {
        // no suitable block found, try the last block (this may grow a chunks size)
        int64_t best_reuse = INT64_MIN;
        for (int c = 0; c < alloc->n_chunks; ++c) {
            struct tallocr_chunk * chunk = alloc->chunks[c];
            if (chunk->n_free_blocks > 0) {
                struct free_block * block = &chunk->free_blocks[chunk->n_free_blocks - 1];
                max_avail = MAX(max_avail, block->size);
                int64_t reuse_factor = chunk->max_size - block->offset - size;
                // reuse_factor < 0 : amount of extra memory that needs to be allocated
                // reuse_factor = 0 : allocated free space exactly matches tensor size
                // reuse_factor > 0 : superfluous memory that will remain unused
                bool better_reuse = best_reuse < 0 && reuse_factor > best_reuse;
                bool better_fit = reuse_factor >= 0 && reuse_factor < best_reuse;
                if (block->size >= size && (better_reuse || better_fit)) {
                    best_fit_chunk = c;
                    best_fit_block = chunk->n_free_blocks - 1;
                    best_reuse = reuse_factor;
                }
            }
        }
    }

    if (best_fit_block == -1) {
        // none of the existing chunks have enough space left
        best_fit_chunk = ggml_dyn_tallocr_new_chunk(alloc, size);
        best_fit_block = 0;
    }
    if (best_fit_chunk == -1) {
        // since the last chunk always has virtually endless memory, this should never happen
        GGML_LOG_ERROR("%s: not enough space in the buffer to allocate %zu bytes, largest block available %zu bytes\n",
            __func__, size, max_avail);
        GGML_ABORT("graph allocation: failed to reserve memory");
    }

    struct tallocr_chunk * chunk = alloc->chunks[best_fit_chunk];
    struct free_block    * block = &chunk->free_blocks[best_fit_block];
    struct buffer_address  addr  = {.chunk = best_fit_chunk, .offset = block->offset };
    block->offset += size;
    block->size -= size;
    if (block->size == 0) {
        // remove block if empty
        ggml_dyn_tallocr_remove_block(chunk, best_fit_block);
    }

    AT_PRINTF("block %d, offset %zu, chunk %d\n", best_fit_block, addr.offset, addr.chunk);

#ifdef GGML_ALLOCATOR_DEBUG
    add_allocated_tensor(alloc, addr, tensor);
    size_t cur_max = addr.offset + size;
    if (cur_max > chunk->max_size) {
        // sort allocated_tensors by chunk/offset
        for (int i = 0; i < 1024; i++) {
            for (int j = i + 1; j < 1024; j++) {
                if (ggml_buffer_address_less(alloc->allocated_tensors[j].addr, alloc->allocated_tensors[i].addr)) {
                    const struct ggml_tensor * tmp_tensor = alloc->allocated_tensors[i].tensor;
                    struct buffer_address tmp_addr = alloc->allocated_tensors[i].addr;
                    alloc->allocated_tensors[i].tensor = alloc->allocated_tensors[j].tensor;
                    alloc->allocated_tensors[i].addr = alloc->allocated_tensors[j].addr;
                    alloc->allocated_tensors[j].tensor = tmp_tensor;
                    alloc->allocated_tensors[j].addr = tmp_addr;
                }
            }
        }
        GGML_LOG_DEBUG("max_size[%d] = %.2f MB: tensors: ", addr.chunk, cur_max / 1024.0 / 1024.0);
        for (int i = 0; i < 1024; i++) {
            if (alloc->allocated_tensors[i].tensor) {
                GGML_LOG_DEBUG("%s [%d: %zx-%zx] (%.2f MB) ", alloc->allocated_tensors[i].tensor->name,
                    alloc->allocated_tensors[i].addr.chunk,
                    alloc->allocated_tensors[i].addr.offset,
                    alloc->allocated_tensors[i].addr.offset + ggml_nbytes(alloc->allocated_tensors[i].tensor),
                    ggml_nbytes(alloc->allocated_tensors[i].tensor) / 1024.0 / 1024.0);
            }
        }
        GGML_LOG_DEBUG("\n");
    }
#endif

    chunk->max_size = MAX(chunk->max_size, addr.offset + size);

    return addr;

    GGML_UNUSED(tensor);
}

// this is a very naive implementation, but for our case the number of free blocks should be very small
static void fg_native_free_bytes(struct ggml_dyn_tallocr * alloc, struct buffer_address addr, size_t size) {
    size = aligned_offset(NULL, size, alloc->alignment);

    struct tallocr_chunk * chunk = alloc->chunks[addr.chunk];

    // see if we can merge with an existing block
    for (int i = 0; i < chunk->n_free_blocks; i++) {
        struct free_block * block = &chunk->free_blocks[i];
        // check if ptr is at the end of the block
        if (block->offset + block->size == addr.offset) {
            block->size += size;
            // check if we can merge with the next block
            if (i < chunk->n_free_blocks - 1) {
                struct free_block * next = &chunk->free_blocks[i+1];
                if (block->offset + block->size == next->offset) {
                    block->size += next->size;
                    ggml_dyn_tallocr_remove_block(chunk, i+1);
                }
            }
            return;
        }
        // check if ptr is at the beginning of the block
        if (addr.offset + size == block->offset) {
            block->offset = addr.offset;
            block->size += size;
            // check if we can merge with the previous block
            if (i > 0) {
                struct free_block * prev = &chunk->free_blocks[i-1];
                if (prev->offset + prev->size == block->offset) {
                    prev->size += block->size;
                    ggml_dyn_tallocr_remove_block(chunk, i);
                }
            }
            return;
        }
    }
    // otherwise, add a new block
    ggml_dyn_tallocr_insert_block(chunk, addr.offset, size);
}

__attribute__((visibility("hidden"))) int fg_selfhost_release_block(void *, int, size_t, size_t);
static bool fg_coalesce_enabled = true;
__attribute__((constructor)) static void fg_coalesce_config(void) {
    const char *on = getenv("GGML_GOOSE_COALESCE");
    if (on && atoi(on) == 0) fg_coalesce_enabled = false;
}
#ifdef FG_COALESCE_TRACE
static atomic_uint_fast64_t fg_coalesce_calls, fg_coalesce_checks, fg_coalesce_digest;
__attribute__((destructor)) static void fg_coalesce_report(void) {
    fprintf(stderr, "FG_SELFHOST_COALESCE enabled=%d calls=%llu shadow_checks=%llu digest=%llu\n", fg_coalesce_enabled,
        (unsigned long long)atomic_load(&fg_coalesce_calls), (unsigned long long)atomic_load(&fg_coalesce_checks),
        (unsigned long long)atomic_load(&fg_coalesce_digest));
}
#endif
__attribute__((noinline)) static void fg_checked_release(struct ggml_dyn_tallocr *alloc, struct buffer_address addr, size_t size) {
    struct tallocr_chunk *chunk = alloc->chunks[addr.chunk];
    struct tallocr_chunk expected = *chunk;
    struct ggml_dyn_tallocr reference = *alloc;
    reference.chunks[addr.chunk] = &expected;
    fg_native_free_bytes(&reference, addr, size);
    chunk->n_free_blocks = fg_selfhost_release_block(chunk->free_blocks, chunk->n_free_blocks, addr.offset, size);
    GGML_ASSERT(chunk->n_free_blocks == expected.n_free_blocks);
    GGML_ASSERT(memcmp(chunk->free_blocks, expected.free_blocks, (size_t)chunk->n_free_blocks * sizeof(struct free_block)) == 0);
#ifdef FG_COALESCE_TRACE
    atomic_fetch_add_explicit(&fg_coalesce_checks, 1, memory_order_relaxed);
#endif
}
static void ggml_dyn_tallocr_free_bytes(struct ggml_dyn_tallocr *alloc, struct buffer_address addr, size_t size) {
    size = aligned_offset(NULL, size, alloc->alignment);
    struct tallocr_chunk *chunk = alloc->chunks[addr.chunk];
    if (!fg_coalesce_enabled) {
        fg_native_free_bytes(alloc, addr, size);
    } else if (fg_alloc_verify) {
        fg_checked_release(alloc, addr, size);
    } else {
        chunk->n_free_blocks = fg_selfhost_release_block(chunk->free_blocks, chunk->n_free_blocks, addr.offset, size);
    }
#ifdef FG_COALESCE_TRACE
    uint64_t fingerprint = addr.offset ^ size * UINT64_C(11400714819323198485) ^ (uint64_t)chunk->n_free_blocks;
    for (int i = 0; i < chunk->n_free_blocks; ++i) {
        fingerprint = (fingerprint ^ chunk->free_blocks[i].offset ^ chunk->free_blocks[i].size) * UINT64_C(1099511628211);
    }
    atomic_fetch_add_explicit(&fg_coalesce_calls, 1, memory_order_relaxed);
    atomic_fetch_add_explicit(&fg_coalesce_digest, fingerprint, memory_order_relaxed);
#endif
}

static void ggml_dyn_tallocr_reset(struct ggml_dyn_tallocr * alloc) {
    if(fg153_memory_enabled()){fg153_memory(2,(intptr_t)alloc,0,0,0);return;}
    for (int i = 0; i < GGML_VBUFFER_MAX_CHUNKS; i++) {
        free(alloc->chunks[i]);
        alloc->chunks[i] = NULL;
    }
    alloc->n_chunks = 0;

#ifdef GGML_ALLOCATOR_DEBUG
    for (int i = 0; i < 1024; i++) {
        alloc->allocated_tensors[i].tensor = NULL;
    }
#endif
}

static struct ggml_dyn_tallocr * ggml_dyn_tallocr_new(size_t alignment, size_t max_buffer_size) {
    if(fg153_memory_enabled()){return (void*)(intptr_t)fg153_memory(0,alignment,max_buffer_size,0,0);}
    struct ggml_dyn_tallocr * alloc = (struct ggml_dyn_tallocr *)malloc(sizeof(struct ggml_dyn_tallocr));

    *alloc = (struct ggml_dyn_tallocr) {
        /*.alignment      = */ alignment,
        /*.max_chunk_size = */ MIN(max_buffer_size, SIZE_MAX/2), // clamp to avoid overflows
        /*.chunks         = */ {NULL},
        /*.n_chunks       = */ 0,
#ifdef GGML_ALLOCATOR_DEBUG
        /*.allocated_tensors = */ {{0}},
#endif
    };

    ggml_dyn_tallocr_reset(alloc);

    return alloc;
}

static void ggml_dyn_tallocr_free(struct ggml_dyn_tallocr * alloc) {
    if(fg153_memory_enabled()){fg153_memory(1,(intptr_t)alloc,0,0,0);return;}
    for (int i = 0; i < alloc->n_chunks; ++i) {
        free(alloc->chunks[i]);
    }
    free(alloc);
}

static size_t ggml_dyn_tallocr_max_size(struct ggml_dyn_tallocr * alloc, int chunk) {
    if(fg153_memory_enabled()){return fg153_memory(4,(intptr_t)alloc,chunk,0,0);}
    return chunk < alloc->n_chunks ? alloc->chunks[chunk]->max_size : 0;
}


// virtual buffer with contiguous memory range, split into multiple backend buffers (chunks)

struct vbuffer {
    ggml_backend_buffer_t chunks[GGML_VBUFFER_MAX_CHUNKS];
};

static void ggml_vbuffer_free(struct vbuffer * buf) {
    if(fg153_memory_enabled()){fg153_memory(6,(intptr_t)buf,0,0,0);return;}
    if (buf == NULL) {
        return;
    }
    for (int i = 0; i < GGML_VBUFFER_MAX_CHUNKS; ++i) {
        ggml_backend_buffer_free(buf->chunks[i]);
    }
    free(buf);
}

static size_t ggml_vbuffer_chunk_size(struct vbuffer * buf, int chunk) {
    if(fg153_memory_enabled()){return fg153_memory(8,(intptr_t)buf,chunk,0,0);}
    return buf->chunks[chunk] ? ggml_backend_buffer_get_size(buf->chunks[chunk]) : 0;
}

static size_t ggml_vbuffer_size(struct vbuffer * buf) {
    if(fg153_memory_enabled()){return fg153_memory(7,(intptr_t)buf,0,0,0);}
    size_t size = 0;
    for (int i = 0; i < GGML_VBUFFER_MAX_CHUNKS && buf->chunks[i]; ++i) {
        size += ggml_backend_buffer_get_size(buf->chunks[i]);
    }
    return size;
}

// when the limit is above the single chunk size, the first chunk of a compute
// buffer may grow beyond the chunk size requested by the tallocr (up to
// min(4 * chunk_size, growth_limit, backend max) for a one-chunk buffer); a
// limit of 0 means no growth at all
static size_t ggml_vbuffer_alloc_grow_size(ggml_backend_buffer_type_t buft, size_t chunk_size, size_t growth_limit) {
    if(fg153_memory_enabled()){return fg153_memory(11,(intptr_t)buft,chunk_size,growth_limit,0);}
    if (growth_limit == 0) {
        return chunk_size;
    }
    // grow up to 4x the chunk size, but never overflow chunk_size * 4
    size_t grow_size = chunk_size > (SIZE_MAX - 3) / 4 ? SIZE_MAX : 4 * chunk_size;
    grow_size = growth_limit < grow_size ? growth_limit : grow_size;
    grow_size = ggml_backend_buft_get_max_size(buft) < grow_size ? ggml_backend_buft_get_max_size(buft) : grow_size;
    return grow_size > chunk_size ? grow_size : chunk_size;
}

static struct vbuffer * ggml_vbuffer_alloc(ggml_backend_buffer_type_t buft, const struct ggml_dyn_tallocr * talloc, enum ggml_backend_buffer_usage usage, size_t growth_limit) {
    if(fg153_memory_enabled()){return (void*)(intptr_t)fg153_memory(5,(intptr_t)buft,(intptr_t)talloc,usage,growth_limit);}
    struct vbuffer * buf = (struct vbuffer *)calloc(1, sizeof(struct vbuffer));
    if (buf == NULL) {
        return NULL;
    }

    for (int n = 0; n < talloc->n_chunks; n++) {
        size_t chunk_size = talloc->chunks[n]->max_size;
        size_t allocation_size = chunk_size;
        if (usage == GGML_BACKEND_BUFFER_USAGE_COMPUTE && talloc->n_chunks == 1 && growth_limit > chunk_size) {
            allocation_size = ggml_vbuffer_alloc_grow_size(buft, chunk_size, growth_limit);
        }
        buf->chunks[n] = ggml_backend_buft_alloc_buffer(buft, allocation_size);
        if (buf->chunks[n] == NULL && allocation_size > chunk_size) {
            // the backend rejected the larger size: fall back to the exact size once
            buf->chunks[n] = ggml_backend_buft_alloc_buffer(buft, chunk_size);
        }
        if (buf->chunks[n] == NULL) {
            ggml_vbuffer_free(buf);
            return NULL;
        }
        ggml_backend_buffer_set_usage(buf->chunks[n], usage);
    }
    return buf;
}

static void ggml_vbuffer_tensor_alloc(struct vbuffer * buf, struct ggml_tensor * tensor, struct buffer_address buf_addr) {
    if(fg153_memory_enabled()){fg153_memory(10,(intptr_t)buf,(intptr_t)tensor,buf_addr.chunk,buf_addr.offset);return;}
    void * base = ggml_backend_buffer_get_base(buf->chunks[buf_addr.chunk]);
    void * addr = (char *)base + buf_addr.offset;
    ggml_backend_tensor_alloc(buf->chunks[buf_addr.chunk], tensor, addr);
}

static void ggml_vbuffer_reset(struct vbuffer * buf) {
    if(fg153_memory_enabled()){fg153_memory(9,(intptr_t)buf,0,0,0);return;}
    for (int i = 0; i < GGML_VBUFFER_MAX_CHUNKS && buf->chunks[i]; ++i) {
        ggml_backend_buffer_reset(buf->chunks[i]);
    }
}


/////////////////////////////////////

// graph allocator

struct hash_node {
    int n_children;
    int n_views;
    int buffer_id;
    struct buffer_address addr;
    bool allocated;
};


_Static_assert(sizeof(struct hash_node) == 40, "Goose tensor lifetime ABI size");
_Static_assert(offsetof(struct hash_node, n_children) == 0, "children ABI offset");
_Static_assert(offsetof(struct hash_node, n_views) == 4, "views ABI offset");
_Static_assert(offsetof(struct hash_node, addr) + offsetof(struct buffer_address, offset) == 24, "buffer address ABI offset");
_Static_assert(offsetof(struct hash_node, allocated) == 32, "owner allocation flag ABI offset");
__attribute__((visibility("hidden"))) int fg_selfhost_parent_done(void *, bool);
__attribute__((visibility("hidden"))) bool fg_selfhost_view_done(void *);
static bool fg_lifetime_enabled = true;
__attribute__((constructor)) static void fg_lifetime_config(void) {
    const char *on = getenv("GGML_GOOSE_LIFETIME");
    if (on && atoi(on) == 0) fg_lifetime_enabled = false;
}
static int fg_native_parent_done(struct hash_node *state, bool is_view) {
    state->n_children -= 1;
    if (state->n_children == 0 && state->n_views == 0) {
        if (is_view) return 2;
        if (state->allocated) return 1;
    }
    return 0;
}
static bool fg_native_view_done(struct hash_node *state) {
    state->n_views -= 1;
    return state->n_views == 0 && state->n_children == 0 && state->allocated;
}
static bool fg_life_fields_equal(const struct hash_node *a, const struct hash_node *b) {
    return a->n_children == b->n_children && a->n_views == b->n_views && a->buffer_id == b->buffer_id
        && a->addr.chunk == b->addr.chunk && a->addr.offset == b->addr.offset && a->allocated == b->allocated;
}
#ifdef FG_LIFETIME_TRACE
static atomic_uint_fast64_t fg_life_parent_calls, fg_life_view_calls, fg_life_checks, fg_life_parent_release, fg_life_owner_release, fg_life_digest;
__attribute__((destructor)) static void fg_lifetime_report(void) {
    fprintf(stderr, "FG_SELFHOST_LIFETIME enabled=%d parents=%llu views=%llu shadow_checks=%llu parent_release_requests=%llu owner_release_requests=%llu digest=%llu\n", fg_lifetime_enabled,
        (unsigned long long)atomic_load(&fg_life_parent_calls), (unsigned long long)atomic_load(&fg_life_view_calls),
        (unsigned long long)atomic_load(&fg_life_checks), (unsigned long long)atomic_load(&fg_life_parent_release),
        (unsigned long long)atomic_load(&fg_life_owner_release), (unsigned long long)atomic_load(&fg_life_digest));
}
static void fg_life_observe(const struct hash_node *state, int action, bool owner) {
    uint64_t code = ((uint64_t)(uint32_t)state->n_children << 32) ^ (uint32_t)state->n_views
        ^ ((uint64_t)state->allocated << 40) ^ ((uint64_t)action << 48) ^ ((uint64_t)owner << 56);
    atomic_fetch_add_explicit(&fg_life_digest, code * UINT64_C(11400714819323198485), memory_order_relaxed);
    atomic_fetch_add_explicit(owner ? &fg_life_view_calls : &fg_life_parent_calls, 1, memory_order_relaxed);
    if (!owner && action == 1) atomic_fetch_add_explicit(&fg_life_parent_release, 1, memory_order_relaxed);
    if (owner && action) atomic_fetch_add_explicit(&fg_life_owner_release, 1, memory_order_relaxed);
}
#endif
__attribute__((noinline)) static int fg_life_checked_parent(struct hash_node *state, bool is_view) {
    struct hash_node expected = *state;
    int action = fg_selfhost_parent_done(state, is_view);
    int native_action = fg_native_parent_done(&expected, is_view);
    GGML_ASSERT(action == native_action && fg_life_fields_equal(state, &expected));
#ifdef FG_LIFETIME_TRACE
    atomic_fetch_add_explicit(&fg_life_checks, 1, memory_order_relaxed);
#endif
    return action;
}
__attribute__((noinline)) static bool fg_life_checked_view(struct hash_node *state) {
    struct hash_node expected = *state;
    bool action = fg_selfhost_view_done(state);
    bool native_action = fg_native_view_done(&expected);
    GGML_ASSERT(action == native_action && fg_life_fields_equal(state, &expected));
#ifdef FG_LIFETIME_TRACE
    atomic_fetch_add_explicit(&fg_life_checks, 1, memory_order_relaxed);
#endif
    return action;
}
static int fg_life_take_parent(struct hash_node *state, bool is_view) {
    int action = fg_alloc_verify ? fg_life_checked_parent(state, is_view) : fg_selfhost_parent_done(state, is_view);
#ifdef FG_LIFETIME_TRACE
    fg_life_observe(state, action, false);
#endif
    return action;
}
static bool fg_life_take_view(struct hash_node *state) {
    bool action = fg_alloc_verify ? fg_life_checked_view(state) : fg_selfhost_view_done(state);
#ifdef FG_LIFETIME_TRACE
    fg_life_observe(state, action, true);
#endif
    return action;
}
struct tensor_alloc {
    int buffer_id;
    struct buffer_address addr;
    size_t size_max; // 0 = pre-allocated, unused, or view
};

struct leaf_alloc {
    struct tensor_alloc leaf;
};

struct node_alloc {
    struct tensor_alloc dst;
    struct tensor_alloc src[GGML_MAX_SRC];
};

struct ggml_gallocr {
    ggml_backend_buffer_type_t * bufts; // [n_buffers]
    struct vbuffer ** buffers; // [n_buffers]
    struct ggml_dyn_tallocr ** buf_tallocs; // [n_buffers]
    int n_buffers;
    size_t * buffer_growth_limits; // [n_buffers], best effort only


    struct ggml_hash_set hash_set;
    struct hash_node * hash_values; // [hash_set.size]

    struct node_alloc * node_allocs; // [n_nodes]
    int n_nodes;

    struct leaf_alloc * leaf_allocs; // [n_leafs]
    int n_leafs;
};

ggml_gallocr_t ggml_gallocr_new_n(ggml_backend_buffer_type_t * bufts, int n_bufs) {
    if(fg153_graphalloc_enabled()){return (void*)(intptr_t)fg153_graphalloc(0,(intptr_t)bufts,n_bufs,0,0,0);}
    ggml_gallocr_t galloc = (ggml_gallocr_t)calloc(1, sizeof(struct ggml_gallocr));
    GGML_ASSERT(galloc != NULL);

    galloc->bufts = calloc(n_bufs, sizeof(ggml_backend_buffer_type_t));
    GGML_ASSERT(galloc->bufts != NULL);

    galloc->buffers = calloc(n_bufs, sizeof(struct vbuffer *));
    GGML_ASSERT(galloc->buffers != NULL);

    galloc->buf_tallocs = calloc(n_bufs, sizeof(struct ggml_dyn_tallocr *));
    GGML_ASSERT(galloc->buf_tallocs != NULL);

    galloc->buffer_growth_limits = calloc(n_bufs, sizeof(size_t));
    GGML_ASSERT(galloc->buffer_growth_limits != NULL);

    for (int i = 0; i < n_bufs; i++) {
        galloc->bufts[i] = bufts[i];
        galloc->buffers[i] = NULL;

        // check if the same buffer type is used multiple times and reuse the same allocator
        for (int j = 0; j < i; j++) {
            if (bufts[i] == bufts[j]) {
                galloc->buf_tallocs[i] = galloc->buf_tallocs[j];
                break;
            }
        }

        if (galloc->buf_tallocs[i] == NULL) {
            size_t alignment = ggml_backend_buft_get_alignment(bufts[i]);
            size_t max_size = ggml_backend_buft_get_max_size(bufts[i]);
            galloc->buf_tallocs[i] = ggml_dyn_tallocr_new(alignment, max_size);
        }
    }
    galloc->n_buffers = n_bufs;

    return galloc;
}

ggml_gallocr_t ggml_gallocr_new(ggml_backend_buffer_type_t buft) {
    return ggml_gallocr_new_n(&buft, 1);
}

// best effort only: the allocator does not guarantee that any of the buffers
// stays within its limit, it only records the values here. When a buffer type
// is shared between several indices, the limit applies to all of them and is
// the minimum of what was requested for that buffer type; a limit of 0 means
// no headroom at all for that (group of) buffer(s).
void ggml_gallocr_set_buffer_growth_limits(ggml_gallocr_t galloc, const size_t * limits) {
    if(fg153_graphalloc_enabled()){fg153_graphalloc(2,(intptr_t)galloc,(intptr_t)limits,0,0,0);return;}
    if (galloc == NULL) {
        return;
    }
    if (limits == NULL) {
        for (int i = 0; i < galloc->n_buffers; i++) {
            galloc->buffer_growth_limits[i] = 0;
        }
        return;
    }
    for (int i = 0; i < galloc->n_buffers; i++) {
        size_t limit = limits[i];
        for (int j = 0; j < galloc->n_buffers; j++) {
            if (galloc->buf_tallocs[j] == galloc->buf_tallocs[i]) {
                limit = limits[j] < limit ? limits[j] : limit;
            }
        }
        galloc->buffer_growth_limits[i] = limit;
    }
}

void ggml_gallocr_free(ggml_gallocr_t galloc) {
    if(fg153_graphalloc_enabled()){fg153_graphalloc(1,(intptr_t)galloc,0,0,0,0);return;}
    if (galloc == NULL) {
        return;
    }

    for (int i = 0; i < galloc->n_buffers; i++) {
        if (galloc->buffers != NULL) {
            // skip if already freed
            bool freed = false;
            for (int j = 0; j < i; j++) {
                if (galloc->buffers[j] == galloc->buffers[i]) {
                    freed = true;
                    break;
                }
            }
            if (!freed) {
                ggml_vbuffer_free(galloc->buffers[i]);
            }
        }
        if (galloc->buf_tallocs != NULL) {
            // skip if already freed
            bool freed = false;
            for (int j = 0; j < i; j++) {
                if (galloc->buf_tallocs[j] == galloc->buf_tallocs[i]) {
                    freed = true;
                    break;
                }
            }
            if (!freed) {
                ggml_dyn_tallocr_free(galloc->buf_tallocs[i]);
            }
        }
    }

    ggml_hash_set_free(&galloc->hash_set);
    free(galloc->hash_values);
    free(galloc->bufts);
    free(galloc->buffers);
    free(galloc->buf_tallocs);
    free(galloc->buffer_growth_limits);
    free(galloc->node_allocs);
    free(galloc->leaf_allocs);
    free(galloc);
}

typedef struct ggml_gallocr * ggml_gallocr_t;

static struct hash_node * ggml_gallocr_hash_get(ggml_gallocr_t galloc, struct ggml_tensor * t) {
    size_t i = ggml_hash_find_or_insert(&galloc->hash_set, t);
    return &galloc->hash_values[i];
}

static bool ggml_gallocr_is_own(ggml_gallocr_t galloc, struct ggml_tensor * t) {
    return ggml_gallocr_hash_get(galloc, t)->allocated;
}

static bool ggml_gallocr_is_allocated(ggml_gallocr_t galloc, struct ggml_tensor * t) {
    return t->data != NULL // tensor data already set externally
        || t->buffer // tensor on external buffer (but not yet allocated)
        || ggml_gallocr_is_own(galloc, t); // tensor will be allocated by galloc
}

// free the extra space at the end if the new tensor is smaller
static void ggml_gallocr_free_extra_space(ggml_gallocr_t galloc, struct ggml_tensor * node, struct ggml_tensor * parent) {
    if(fg153_graphalloc_enabled()){fg153_graphalloc(10,(intptr_t)galloc,(intptr_t)node,(intptr_t)parent,0,0);return;}
    struct hash_node * hn = ggml_gallocr_hash_get(galloc, node);
    struct hash_node * p_hn = ggml_gallocr_hash_get(galloc, parent);

    size_t parent_size = ggml_backend_buft_get_alloc_size(galloc->bufts[p_hn->buffer_id], parent);
    size_t node_size = ggml_backend_buft_get_alloc_size(galloc->bufts[hn->buffer_id], node);

    GGML_ASSERT(parent_size >= node_size);

    // note: we want after the freeing the chunks to continue to be aligned
    struct ggml_dyn_tallocr * p_alloc = galloc->buf_tallocs[p_hn->buffer_id];
    parent_size = aligned_offset(NULL, parent_size, p_alloc->alignment);
    node_size = aligned_offset(NULL, node_size, p_alloc->alignment);

    if (parent_size > node_size) {
        struct buffer_address p_addr = p_hn->addr;
        p_addr.offset += node_size;
        size_t extra_size = parent_size - node_size;
        AT_PRINTF("freeing extra %zu bytes from parent %s for %s\n", extra_size, parent->name, node->name);
        ggml_dyn_tallocr_free_bytes(p_alloc, p_addr, extra_size);
    }
}

static void ggml_gallocr_allocate_node(ggml_gallocr_t galloc, struct ggml_tensor * node, int buffer_id) {
    if(fg153_graphalloc_enabled()){fg153_graphalloc(8,(intptr_t)galloc,(intptr_t)node,buffer_id,0,0);return;}
    GGML_ASSERT(buffer_id >= 0);
    struct hash_node * hn = ggml_gallocr_hash_get(galloc, node);

    if (!ggml_gallocr_is_allocated(galloc, node) && !ggml_impl_is_view(node)) {
        hn->allocated = true;
        assert(hn->addr.offset == 0);

        // try to reuse a parent's buffer (inplace)
        if (ggml_op_can_inplace(node->op)) {
            for (int i = 0; i < GGML_MAX_SRC; i++) {
                struct ggml_tensor * parent = node->src[i];
                if (parent == NULL) {
                    continue;
                }

                // if the node's data is external, then we cannot re-use it
                if (!ggml_gallocr_is_own(galloc, parent)) {
                    AT_PRINTF("not reusing parent %s for %s as %p is external\n", parent->name, node->name, parent->data);
                    continue;
                }

                // outputs cannot be reused
                if (parent->flags & GGML_TENSOR_FLAG_OUTPUT || (parent->view_src != NULL && parent->view_src->flags & GGML_TENSOR_FLAG_OUTPUT)) {
                    AT_PRINTF("not reusing parent %s for %s as it is an output\n", parent->name, node->name);
                    continue;
                }

                if (!ggml_are_same_layout(node, parent)) {
                    AT_PRINTF("not reusing parent %s for %s as layouts are different\n", parent->name, node->name);
                    continue;
                }

                struct hash_node * p_hn = ggml_gallocr_hash_get(galloc, parent);
                if (p_hn->n_children == 1 && p_hn->n_views == 0) {
                    if (ggml_impl_is_view(parent)) {
                        struct ggml_tensor * view_src = parent->view_src;
                        struct hash_node * view_src_hn = ggml_gallocr_hash_get(galloc, view_src);
                        if (view_src_hn->n_views == 1 && view_src_hn->n_children == 0 && view_src->data == parent->data) {
                            AT_PRINTF("reusing view parent %s (%s) for %s\n", parent->name, view_src->name, node->name);
                            assert(view_src_hn->addr.chunk == p_hn->addr.chunk && view_src_hn->addr.offset == p_hn->addr.offset);
                            hn->buffer_id = p_hn->buffer_id;
                            hn->addr = p_hn->addr;
                            p_hn->allocated = false; // avoid freeing the parent
                            view_src_hn->allocated = false;
                            ggml_gallocr_free_extra_space(galloc, node, view_src);
                            return;
                        }
                    } else {
                        AT_PRINTF("reusing parent %s for %s\n", parent->name, node->name);
                        hn->buffer_id = p_hn->buffer_id;
                        hn->addr = p_hn->addr;
                        p_hn->allocated = false; // avoid freeing the parent
                        ggml_gallocr_free_extra_space(galloc, node, parent);
                        return;
                    }
                }
            }
        }
        // allocate tensor from the buffer
        struct ggml_dyn_tallocr * alloc = galloc->buf_tallocs[buffer_id];
        ggml_backend_buffer_type_t buft = galloc->bufts[buffer_id];
        size_t size = ggml_backend_buft_get_alloc_size(buft, node);
        hn->buffer_id = buffer_id;
        hn->addr = ggml_dyn_tallocr_alloc(alloc, size, node);
    }
}

static void ggml_gallocr_free_node(ggml_gallocr_t galloc, struct ggml_tensor * node) {
    if(fg153_graphalloc_enabled()){fg153_graphalloc(9,(intptr_t)galloc,(intptr_t)node,0,0,0);return;}
    // graph outputs are never freed
    if (node->flags & GGML_TENSOR_FLAG_OUTPUT) {
        AT_PRINTF("not freeing output %s\n", node->name);
        return;
    }

    struct hash_node * hn = ggml_gallocr_hash_get(galloc, node);
    int buffer_id = hn->buffer_id;
    struct ggml_dyn_tallocr * alloc = galloc->buf_tallocs[buffer_id];
    ggml_backend_buffer_type_t buft = galloc->bufts[buffer_id];
    size_t size = ggml_backend_buft_get_alloc_size(buft, node);

    AT_PRINTF("%s: freeing %s at {chunk=%d, offset=%zu} (%zu bytes) - n_free_blocks = %d\n",
        __func__, node->name, hn->addr.chunk, hn->addr.offset, size, alloc->chunks[hn->addr.chunk]->n_free_blocks);
#ifdef GGML_ALLOCATOR_DEBUG
    remove_allocated_tensor(alloc, hn->addr, node);
#endif

    ggml_dyn_tallocr_free_bytes(alloc, hn->addr, size);
    hn->allocated = false;
}

static int get_node_buffer_id(const int * node_buffer_ids, int i) {
    return node_buffer_ids ? node_buffer_ids[i] : 0;
}

static void fg_native_gallocr_alloc_graph_impl(ggml_gallocr_t galloc, struct ggml_cgraph * graph, const int * node_buffer_ids, const int * leaf_buffer_ids) {
    // clear hash tables
    ggml_hash_set_reset(&galloc->hash_set);
    memset(galloc->hash_values, 0, sizeof(struct hash_node) * galloc->hash_set.size);

    // allocate leafs
    // these may be tensors that the application is not using in the graph, but may still want to allocate for other purposes
    for (int i = 0; i < graph->n_leafs; i++) {
        struct ggml_tensor * leaf = graph->leafs[i];
        ggml_gallocr_allocate_node(galloc, leaf, get_node_buffer_id(leaf_buffer_ids, i));
    }

    // count number of children and views
    // allocate other graph inputs and leafs first to avoid overwriting them
    for (int i = 0; i < graph->n_nodes; i++) {
        struct ggml_tensor * node = graph->nodes[i];

        // TODO: better way to add external dependencies
        // GGML_OP_NONE does not appear normally in the graph nodes, but is used by ggml-backend to add dependencies to
        // control when some tensors are allocated and freed. in this case, the dependencies are in `src`, but the node
        // itself is never used and should not be considered a dependency
        if (ggml_impl_is_view(node) && node->op != GGML_OP_NONE) {
            struct ggml_tensor * view_src = node->view_src;
            ggml_gallocr_hash_get(galloc, view_src)->n_views += 1;
        }

        if (node->flags & GGML_TENSOR_FLAG_INPUT) {
            ggml_gallocr_allocate_node(galloc, graph->nodes[i], get_node_buffer_id(node_buffer_ids, i));
        }

        for (int j = 0; j < GGML_MAX_SRC; j++) {
            struct ggml_tensor * src = node->src[j];
            if (src == NULL) {
                continue;
            }

            ggml_gallocr_hash_get(galloc, src)->n_children += 1;

            // allocate explicit inputs
            if (src->flags & GGML_TENSOR_FLAG_INPUT) {
                ggml_gallocr_allocate_node(galloc, src, get_node_buffer_id(node_buffer_ids, i));
            }
        }
    }

    // allocate tensors
    for (int i = 0; i < graph->n_nodes; i++) {
        struct ggml_tensor * node = graph->nodes[i];
        int buffer_id = get_node_buffer_id(node_buffer_ids, i);

        // allocate parents (only leafs need to be allocated at this point)
        for (int j = 0; j < GGML_MAX_SRC; j++) {
            struct ggml_tensor * parent = node->src[j];
            if (parent == NULL) {
                continue;
            }
            ggml_gallocr_allocate_node(galloc, parent, buffer_id);
        }

        // allocate node
        ggml_gallocr_allocate_node(galloc, node, buffer_id);

        AT_PRINTF("exec: %s (%s) <= ", ggml_op_desc(node), node->name);
        for (int j = 0; j < GGML_MAX_SRC; j++) {
            struct ggml_tensor * parent = node->src[j];
            if (parent == NULL) {
                continue;
            }
            AT_PRINTF("%s", parent->name);
            if (j < GGML_MAX_SRC - 1 && node->src[j + 1] != NULL) {
                AT_PRINTF(", ");
            }
        }
        AT_PRINTF("\n");

        // update parents
        for (int j = 0; j < GGML_MAX_SRC; j++) {
            struct ggml_tensor * parent = node->src[j];
            if (parent == NULL) {
                continue;
            }
            struct hash_node * p_hn = ggml_gallocr_hash_get(galloc, parent);
            if (fg_lifetime_enabled) {
                int action = fg_life_take_parent(p_hn, ggml_impl_is_view(parent));
                if (action == 2) {
                    struct ggml_tensor *owner = parent->view_src;
                    struct hash_node *owner_hn = ggml_gallocr_hash_get(galloc, owner);
                    if (fg_life_take_view(owner_hn)) ggml_gallocr_free_node(galloc, owner);
                } else if (action == 1) {
                    ggml_gallocr_free_node(galloc, parent);
                }
                continue;
            }
            p_hn->n_children -= 1;

            AT_PRINTF("parent %s: %d children, %d views, allocated: %d\n",
                parent->name, p_hn->n_children, p_hn->n_views, p_hn->allocated);

            if (p_hn->n_children == 0 && p_hn->n_views == 0) {
                if (ggml_impl_is_view(parent)) {
                    struct ggml_tensor * view_src = parent->view_src;
                    struct hash_node * view_src_hn = ggml_gallocr_hash_get(galloc, view_src);
                    view_src_hn->n_views -= 1;
                    AT_PRINTF("view_src %s: %d children, %d views\n",
                        view_src->name, view_src_hn->n_children, view_src_hn->n_views);
                    if (view_src_hn->n_views == 0 && view_src_hn->n_children == 0 && view_src_hn->allocated) {
                        ggml_gallocr_free_node(galloc, view_src);
                    }
                }
                else if (p_hn->allocated) {
                    ggml_gallocr_free_node(galloc, parent);
                }
            }
            AT_PRINTF("\n");
        }
    }
}


__attribute__((visibility("hidden"))) void fg_selfhost_graph_plan(void *, const void *, int, const void *, int, const int *, const int *, int, unsigned, int, bool);
static bool fg_graph_enabled = true;
__attribute__((constructor)) static void fg_graph_config(void) {
    const char *on = getenv("GGML_GOOSE_GRAPH_PLAN");
    if (on && atoi(on) == 0) fg_graph_enabled = false;
}
#ifdef FG_GRAPH_TRACE
static atomic_uint_fast64_t fg_graph_walks, fg_graph_nodes, fg_graph_leafs, fg_graph_allocs, fg_graph_releases, fg_graph_checks[4];
__attribute__((destructor)) static void fg_graph_report(void) {
    fprintf(stderr, "FG_SELFHOST_GRAPH enabled=%d walks=%llu nodes=%llu leafs=%llu alloc_requests=%llu release_requests=%llu checks=%llu,%llu,%llu,%llu\n", fg_graph_enabled,
        (unsigned long long)atomic_load(&fg_graph_walks), (unsigned long long)atomic_load(&fg_graph_nodes),
        (unsigned long long)atomic_load(&fg_graph_leafs), (unsigned long long)atomic_load(&fg_graph_allocs),
        (unsigned long long)atomic_load(&fg_graph_releases), (unsigned long long)atomic_load(&fg_graph_checks[0]),
        (unsigned long long)atomic_load(&fg_graph_checks[1]), (unsigned long long)atomic_load(&fg_graph_checks[2]),
        (unsigned long long)atomic_load(&fg_graph_checks[3]));
}
#endif
__attribute__((visibility("hidden"))) void *fg_graph_hash(uint64_t allocator, uint64_t tensor) {
    return ggml_gallocr_hash_get((ggml_gallocr_t)(uintptr_t)allocator, (const struct ggml_tensor *)(uintptr_t)tensor);
}
__attribute__((visibility("hidden"))) uint64_t fg_graph_source(uint64_t tensor, int64_t index) {
    GGML_ASSERT(index >= 0 && index < GGML_MAX_SRC);
    return (uintptr_t)((const struct ggml_tensor *)(uintptr_t)tensor)->src[index];
}
__attribute__((visibility("hidden"))) uint64_t fg_graph_owner(uint64_t tensor) {
    return (uintptr_t)((const struct ggml_tensor *)(uintptr_t)tensor)->view_src;
}
__attribute__((visibility("hidden"))) bool fg_graph_is_view(uint64_t tensor) {
    return ggml_impl_is_view((const struct ggml_tensor *)(uintptr_t)tensor);
}
__attribute__((visibility("hidden"))) uint32_t fg_graph_flags(uint64_t tensor) {
    return ((const struct ggml_tensor *)(uintptr_t)tensor)->flags;
}
__attribute__((visibility("hidden"))) int32_t fg_graph_op(uint64_t tensor) {
    return ((const struct ggml_tensor *)(uintptr_t)tensor)->op;
}
__attribute__((visibility("hidden"))) void fg_graph_allocate(uint64_t allocator, uint64_t tensor, int32_t buffer_id) {
#ifdef FG_GRAPH_TRACE
    atomic_fetch_add_explicit(&fg_graph_allocs, 1, memory_order_relaxed);
#endif
    ggml_gallocr_allocate_node((ggml_gallocr_t)(uintptr_t)allocator, (struct ggml_tensor *)(uintptr_t)tensor, buffer_id);
}
__attribute__((visibility("hidden"))) void fg_graph_release(uint64_t allocator, uint64_t tensor) {
#ifdef FG_GRAPH_TRACE
    atomic_fetch_add_explicit(&fg_graph_releases, 1, memory_order_relaxed);
#endif
    ggml_gallocr_free_node((ggml_gallocr_t)(uintptr_t)allocator, (struct ggml_tensor *)(uintptr_t)tensor);
}
__attribute__((visibility("hidden"))) void fg_graph_verify(const void *pointer, int32_t children, int32_t views, bool allocated, int32_t kind, int32_t action, bool is_view) {
    const struct hash_node *state = pointer;
    struct hash_node expected = *state;
    expected.n_children = children;
    expected.n_views = views;
    expected.allocated = allocated;
    int native_action = 0;
    if (kind == 0) expected.n_views += 1;
    else if (kind == 1) expected.n_children += 1;
    else if (kind == 2) native_action = fg_native_parent_done(&expected, is_view);
    else if (kind == 3) native_action = fg_native_view_done(&expected);
    else GGML_ABORT("invalid graph transition kind");
    GGML_ASSERT(native_action == action && fg_life_fields_equal(state, &expected));
#ifdef FG_GRAPH_TRACE
    atomic_fetch_add_explicit(&fg_graph_checks[kind], 1, memory_order_relaxed);
#endif
}
static void ggml_gallocr_alloc_graph_impl(ggml_gallocr_t galloc, struct ggml_cgraph *graph, const int *node_buffer_ids, const int *leaf_buffer_ids) {
    if (!fg_graph_enabled) {
        fg_native_gallocr_alloc_graph_impl(galloc, graph, node_buffer_ids, leaf_buffer_ids);
        return;
    }
    ggml_hash_set_reset(&galloc->hash_set);
    memset(galloc->hash_values, 0, sizeof(struct hash_node) * galloc->hash_set.size);
#ifdef FG_GRAPH_TRACE
    atomic_fetch_add_explicit(&fg_graph_walks, 1, memory_order_relaxed);
    atomic_fetch_add_explicit(&fg_graph_nodes, (uint64_t)graph->n_nodes, memory_order_relaxed);
    atomic_fetch_add_explicit(&fg_graph_leafs, (uint64_t)graph->n_leafs, memory_order_relaxed);
#endif
    fg_selfhost_graph_plan(galloc, graph->nodes, graph->n_nodes, graph->leafs, graph->n_leafs,
        node_buffer_ids, leaf_buffer_ids, GGML_MAX_SRC, GGML_TENSOR_FLAG_INPUT, GGML_OP_NONE, fg_alloc_verify);
}
static bool ggml_gallocr_reserve_n_impl(
        ggml_gallocr_t galloc, struct ggml_cgraph * graph, const int * node_buffer_ids, const int * leaf_buffer_ids, bool no_alloc) {
    if(fg153_graphalloc_enabled())return fg153_graphalloc(3,(intptr_t)galloc,(intptr_t)graph,(intptr_t)node_buffer_ids,(intptr_t)leaf_buffer_ids,no_alloc)!=0;
    size_t min_hash_size = graph->n_nodes + graph->n_leafs;
    // add 25% margin to avoid hash collisions
    min_hash_size += min_hash_size / 4;

    // initialize hash table
    if (galloc->hash_set.size < min_hash_size) {
        ggml_hash_set_free(&galloc->hash_set);
        galloc->hash_set = ggml_hash_set_new(min_hash_size);
        GGML_ASSERT(galloc->hash_set.keys != NULL);

        free(galloc->hash_values);
        galloc->hash_values = malloc(sizeof(struct hash_node) * galloc->hash_set.size);
        GGML_ASSERT(galloc->hash_values != NULL);
    }

    // reset allocators
    for (int i = 0; i < galloc->n_buffers; i++) {
        ggml_dyn_tallocr_reset(galloc->buf_tallocs[i]);
    }

    // allocate in hash table
    ggml_gallocr_alloc_graph_impl(galloc, graph, node_buffer_ids, leaf_buffer_ids);

    // set the node_allocs from the hash table
    if (galloc->n_nodes < graph->n_nodes) {
        free(galloc->node_allocs);
        galloc->node_allocs = calloc(graph->n_nodes, sizeof(struct node_alloc));
        GGML_ASSERT(galloc->node_allocs != NULL);
    }
    galloc->n_nodes = graph->n_nodes;
    for (int i = 0; i < graph->n_nodes; i++) {
        struct ggml_tensor * node = graph->nodes[i];
        struct node_alloc * node_alloc = &galloc->node_allocs[i];
        if (node->view_src || node->data) {
            node_alloc->dst.buffer_id = -1;
            node_alloc->dst.addr = GGML_BUFFER_ADDRESS_INVALID;
            node_alloc->dst.size_max = 0;
        } else {
            struct hash_node * hn = ggml_gallocr_hash_get(galloc, node);
            node_alloc->dst.buffer_id = hn->buffer_id;
            node_alloc->dst.addr = hn->addr;
            node_alloc->dst.size_max  = ggml_backend_buft_get_alloc_size(galloc->bufts[hn->buffer_id], node);
        }
        for (int j = 0; j < GGML_MAX_SRC; j++) {
            struct ggml_tensor * src = node->src[j];
            if (!src || src->view_src || src->data) {
                node_alloc->src[j].buffer_id = -1;
                node_alloc->src[j].addr = GGML_BUFFER_ADDRESS_INVALID;
                node_alloc->src[j].size_max = 0;
            } else {
                struct hash_node * hn = ggml_gallocr_hash_get(galloc, src);
                node_alloc->src[j].buffer_id = hn->buffer_id;
                node_alloc->src[j].addr = hn->addr;
                node_alloc->src[j].size_max = ggml_backend_buft_get_alloc_size(galloc->bufts[hn->buffer_id], src);
            }
        }
    }
    if (galloc->n_leafs < graph->n_leafs) {
        free(galloc->leaf_allocs);
        galloc->leaf_allocs = calloc(graph->n_leafs, sizeof(galloc->leaf_allocs[0]));
        GGML_ASSERT(galloc->leaf_allocs != NULL);
    }
    galloc->n_leafs = graph->n_leafs;
    for (int i = 0; i < graph->n_leafs; i++) {
        struct ggml_tensor * leaf = graph->leafs[i];
        struct hash_node * hn = ggml_gallocr_hash_get(galloc, leaf);
        if (leaf->view_src || leaf->data) {
            galloc->leaf_allocs[i].leaf.buffer_id = -1;
            galloc->leaf_allocs[i].leaf.addr = GGML_BUFFER_ADDRESS_INVALID;
            galloc->leaf_allocs[i].leaf.size_max = 0;
        } else {
            galloc->leaf_allocs[i].leaf.buffer_id = hn->buffer_id;
            galloc->leaf_allocs[i].leaf.addr = hn->addr;
            galloc->leaf_allocs[i].leaf.size_max = ggml_backend_buft_get_alloc_size(galloc->bufts[hn->buffer_id], leaf);
        }
    }

    // reallocate buffers if needed
    for (int i = 0; i < galloc->n_buffers; i++) {
        // if the buffer type is used multiple times, we reuse the same buffer
        for (int j = 0; j < i; j++) {
            if (galloc->buf_tallocs[j] == galloc->buf_tallocs[i]) {
                galloc->buffers[i] = galloc->buffers[j];
                break;
            }
        }

        // even if there are no tensors allocated in this buffer, we still need to allocate it to initialize views
        bool realloc = galloc->buffers[i] == NULL;
        size_t new_size = 0;
        for (int c = 0; c < galloc->buf_tallocs[i]->n_chunks; c++) {
            size_t cur_chunk_size = galloc->buffers[i] ? ggml_vbuffer_chunk_size(galloc->buffers[i], c) : 0;
            size_t new_chunk_size = ggml_dyn_tallocr_max_size(galloc->buf_tallocs[i], c);
            new_size += new_chunk_size;
            if (new_chunk_size > cur_chunk_size) {
                realloc = true;
            }
        }
        if (realloc) {
#ifndef NDEBUG
            {
                size_t cur_size = galloc->buffers[i] ? ggml_vbuffer_size(galloc->buffers[i]) : 0;
                if (cur_size > 0) {
                    GGML_LOG_DEBUG("%s: reallocating %s buffer from size %.02f MiB to %.02f MiB\n",
                        __func__, ggml_backend_buft_name(galloc->bufts[i]), cur_size / 1024.0 / 1024.0, new_size / 1024.0 / 1024.0);
                }
            }
#endif
            ggml_vbuffer_free(galloc->buffers[i]);
            if (no_alloc) {
                galloc->buffers[i] = NULL;
            } else {
                galloc->buffers[i] = ggml_vbuffer_alloc(galloc->bufts[i], galloc->buf_tallocs[i], GGML_BACKEND_BUFFER_USAGE_COMPUTE, galloc->buffer_growth_limits[i]);
                if (galloc->buffers[i] == NULL) {
                    GGML_LOG_ERROR("%s: failed to allocate %s buffer of size %zu\n", __func__, ggml_backend_buft_name(galloc->bufts[i]), new_size);
                    return false;
                }
            }
        }
    }

    return true;
}

void ggml_gallocr_reserve_n_size(
        ggml_gallocr_t galloc, struct ggml_cgraph * graph, const int * node_buffer_ids, const int * leaf_buffer_ids, size_t * sizes) {
    GGML_ASSERT(ggml_gallocr_reserve_n_impl(galloc, graph, node_buffer_ids, leaf_buffer_ids, /*no_alloc =*/ true));
    for (int i = 0; i < galloc->n_buffers; i++) {
        sizes[i] = 0;
        for (int c = 0; c < galloc->buf_tallocs[i]->n_chunks; c++) {
            sizes[i] += galloc->buf_tallocs[i]->chunks[c]->max_size;
        }
    }
}

bool ggml_gallocr_reserve_n(ggml_gallocr_t galloc, struct ggml_cgraph * graph, const int * node_buffer_ids, const int * leaf_buffer_ids) {
    return ggml_gallocr_reserve_n_impl(galloc, graph, node_buffer_ids, leaf_buffer_ids, /*no_alloc =*/ false);
}

bool ggml_gallocr_reserve(ggml_gallocr_t galloc, struct ggml_cgraph *graph) {
    return ggml_gallocr_reserve_n(galloc, graph, NULL, NULL);
}

static void ggml_gallocr_init_tensor(ggml_gallocr_t galloc, struct ggml_tensor * tensor, struct tensor_alloc * tensor_alloc) {
    if(fg153_graphalloc_enabled()){fg153_graphalloc(4,(intptr_t)galloc,(intptr_t)tensor,(intptr_t)tensor_alloc,0,0);return;}
    int buffer_id = tensor_alloc->buffer_id;
    assert(tensor->data || tensor->view_src || ggml_backend_buft_get_alloc_size(galloc->bufts[buffer_id], tensor) <= tensor_alloc->size_max);

    if (tensor->view_src != NULL) {
        if (tensor->buffer == NULL) {
            assert(tensor_alloc->addr.offset == SIZE_MAX);
            if (tensor->view_src->buffer == NULL) {
                // this tensor was allocated without ggml-backend
                return;
            }
            ggml_backend_view_init(tensor);
        }
    } else {
        if (tensor->data == NULL) {
            assert(tensor_alloc->addr.offset != SIZE_MAX);
            assert(ggml_backend_buft_get_alloc_size(galloc->bufts[buffer_id], tensor) <= tensor_alloc->size_max);
            ggml_vbuffer_tensor_alloc(galloc->buffers[buffer_id], tensor, tensor_alloc->addr);
        } else {
            if (tensor->buffer == NULL) {
                // this tensor was allocated without ggml-backend
                return;
            }
        }
    }
}

static bool ggml_gallocr_node_needs_realloc(ggml_gallocr_t galloc, struct ggml_tensor * node, struct tensor_alloc * talloc) {
    size_t node_size = 0;
    if (!node->data && !node->view_src) {
        // If we previously had data but don't now then reallocate
        if (talloc->buffer_id < 0) {
            return false;
        }
        node_size = ggml_backend_buft_get_alloc_size(galloc->bufts[talloc->buffer_id], node);
    }
    return talloc->size_max >= node_size;
}

static bool ggml_gallocr_needs_realloc(ggml_gallocr_t galloc, struct ggml_cgraph * graph) {
    if(fg153_graphalloc_enabled()){return fg153_graphalloc(5,(intptr_t)galloc,(intptr_t)graph,0,0,0)!=0;}
    if (galloc->n_nodes != graph->n_nodes) {
#ifndef NDEBUG
        GGML_LOG_DEBUG("%s: graph has different number of nodes\n", __func__);
#endif
        return true;
    }

    if (galloc->n_leafs != graph->n_leafs) {
#ifndef NDEBUG
        GGML_LOG_DEBUG("%s: graph has different number of leafs\n", __func__);
#endif
        return true;
    }

    for (int i = 0; i < graph->n_nodes; i++) {
        struct ggml_tensor * node = graph->nodes[i];
        struct node_alloc * node_alloc = &galloc->node_allocs[i];

        if (!ggml_gallocr_node_needs_realloc(galloc, node, &node_alloc->dst)) {
#ifndef NDEBUG
            GGML_LOG_DEBUG("%s: node %s is not valid\n", __func__, node->name);
#endif
            return true;
        }

        for (int j = 0; j < GGML_MAX_SRC; j++) {
            struct ggml_tensor * src = node->src[j];
            if (src == NULL) {
                continue;
            }
            if (!ggml_gallocr_node_needs_realloc(galloc, src, &node_alloc->src[j])) {
#ifndef NDEBUG
                GGML_LOG_DEBUG("%s: src %d (%s) of node %s is not valid\n", __func__, j, src->name, node->name);
#endif
                return true;
            }
        }
    }

    return false;
}

bool ggml_gallocr_alloc_graph(ggml_gallocr_t galloc, struct ggml_cgraph * graph) {
    if(fg153_graphalloc_enabled()){return fg153_graphalloc(6,(intptr_t)galloc,(intptr_t)graph,0,0,0)!=0;}
    if (ggml_gallocr_needs_realloc(galloc, graph)) {
        if (galloc->n_buffers == 1) {
#ifndef NDEBUG
            GGML_LOG_DEBUG("%s: reallocating buffers automatically\n", __func__);
#endif
            if (!ggml_gallocr_reserve(galloc, graph)) {
                return false;
            }
        } else {
#ifndef NDEBUG
            GGML_LOG_DEBUG("%s: cannot reallocate multi buffer graph automatically, call reserve\n", __func__);
#endif
            return false;
        }
    }

    // reset buffers
    for (int i = 0; i < galloc->n_buffers; i++) {
        if (galloc->buffers[i] != NULL) {
            ggml_vbuffer_reset(galloc->buffers[i]);
        }
    }

    // allocate the graph tensors from the previous assignments
    // leafs
    for (int i = 0; i < graph->n_leafs; i++) {
        struct ggml_tensor * leaf = graph->leafs[i];
        struct leaf_alloc * leaf_alloc = &galloc->leaf_allocs[i];
        ggml_gallocr_init_tensor(galloc, leaf, &leaf_alloc->leaf);
    }
    // nodes
    for (int i = 0; i < graph->n_nodes; i++) {
        struct ggml_tensor * node = graph->nodes[i];
        struct node_alloc * node_alloc = &galloc->node_allocs[i];
        for (int j = 0; j < GGML_MAX_SRC; j++) {
            struct ggml_tensor * src = node->src[j];
            if (src == NULL) {
                continue;
            }
            ggml_gallocr_init_tensor(galloc, src, &node_alloc->src[j]);
        }
        ggml_gallocr_init_tensor(galloc, node, &node_alloc->dst);
    }

    return true;
}

size_t ggml_gallocr_get_buffer_size(ggml_gallocr_t galloc, int buffer_id) {
    if(fg153_graphalloc_enabled()){return fg153_graphalloc(7,(intptr_t)galloc,buffer_id,0,0,0);}
    GGML_ASSERT(buffer_id >= 0 && buffer_id < galloc->n_buffers);

    if (galloc->buffers[buffer_id] == NULL) {
        return 0;
    }

    for (int i = 0; i < buffer_id; i++) {
        if (galloc->buffers[i] == galloc->buffers[buffer_id]) {
            // this buffer is the same as a previous one due to the same buffer type being used multiple times
            // only return the buffer size the first time it appears to avoid double counting
            return 0;
        }
    }

    return ggml_vbuffer_size(galloc->buffers[buffer_id]);
}

// utils

static struct ggml_tensor ** ggml_backend_alloc_ctx_tensors_from_buft_collect(
        struct ggml_context * ctx, int * n_tensors) {
    int n = 0;
    for (struct ggml_tensor * t = ggml_get_first_tensor(ctx); t != NULL; t = ggml_get_next_tensor(ctx, t)) {
        n++;
    }
    *n_tensors = n;
    if (n == 0) {
        return NULL;
    }

    struct ggml_tensor ** tensors = (struct ggml_tensor **) malloc(n * sizeof(struct ggml_tensor *));
    if (tensors == NULL) {
        GGML_LOG_ERROR("%s: failed to allocate %zu bytes\n", __func__, n * sizeof(struct ggml_tensor *));
        return NULL;
    }
    int i = 0;
    for (struct ggml_tensor * t = ggml_get_first_tensor(ctx); t != NULL; t = ggml_get_next_tensor(ctx, t)) {
        tensors[i++] = t;
    }
    return tensors;
}

ggml_backend_buffer_t ggml_backend_alloc_ctx_tensors_from_buft(struct ggml_context * ctx, ggml_backend_buffer_type_t buft) {
    if(fg153_buffers_enabled()){return (void*)(intptr_t)fg153_buffers(6,(intptr_t)ctx,(intptr_t)buft,0);}
    GGML_ASSERT(ggml_get_no_alloc(ctx) == true);

    int n_tensors = 0;
    struct ggml_tensor ** tensors = ggml_backend_alloc_ctx_tensors_from_buft_collect(ctx, &n_tensors);
    if (tensors == NULL) {
        return NULL;
    }

    ggml_backend_buffer_t buffer = ggml_backend_buft_alloc_buffer_n(buft, tensors, n_tensors);
    free(tensors);
    return buffer;
}

size_t ggml_backend_alloc_ctx_tensors_from_buft_size(struct ggml_context * ctx, ggml_backend_buffer_type_t buft) {
    if(fg153_buffers_enabled()){return fg153_buffers(7,(intptr_t)ctx,(intptr_t)buft,0);}
    GGML_ASSERT(ggml_get_no_alloc(ctx) == true);

    int n_tensors = 0;
    struct ggml_tensor ** tensors = ggml_backend_alloc_ctx_tensors_from_buft_collect(ctx, &n_tensors);
    if (tensors == NULL) {
        return 0;
    }

    size_t nbytes_total = ggml_backend_buft_get_alloc_size_n(buft, tensors, n_tensors);
    free(tensors);
    return nbytes_total;
}

ggml_backend_buffer_t ggml_backend_alloc_ctx_tensors(struct ggml_context * ctx, ggml_backend_t backend) {
    return ggml_backend_alloc_ctx_tensors_from_buft(ctx, ggml_backend_get_default_buffer_type(backend));
}

#include "bridge.h"
int fg153_memory_enabled(void){const char*v=getenv("GGML_GOOSE_MEMORY");return !v||strcmp(v,"0")!=0;}
int64_t fg153_mg(int64_t p,int64_t k,int64_t i){struct ggml_dyn_tallocr*a=(void*)(intptr_t)p;struct tallocr_chunk*c=(void*)(intptr_t)p;struct vbuffer*v=(void*)(intptr_t)p;struct buffer_address*addr=(void*)(intptr_t)p;
 switch(k){case 0:return a->alignment;case 1:return a->max_chunk_size;case 2:GGML_ASSERT(i>=0&&i<GGML_VBUFFER_MAX_CHUNKS);return(intptr_t)a->chunks[i];case 3:return a->n_chunks;case 4:GGML_ASSERT(i>=0&&i<GGML_VBUFFER_MAX_CHUNKS);return(intptr_t)v->chunks[i];case 10:return c->n_free_blocks;case 11:GGML_ASSERT(i>=0&&i<MAX_FREE_BLOCKS);return c->free_blocks[i].offset;case 12:GGML_ASSERT(i>=0&&i<MAX_FREE_BLOCKS);return c->free_blocks[i].size;case 13:return c->max_size;case 20:return addr->chunk;case 21:return addr->offset;case 30:return sizeof(struct ggml_dyn_tallocr);case 31:return sizeof(struct tallocr_chunk);case 32:return sizeof(struct vbuffer);case 33:return GGML_VBUFFER_MAX_CHUNKS;case 34:return GGML_BACKEND_BUFFER_USAGE_COMPUTE;default:GGML_ABORT("bad Goose memory field");}}
void fg153_mp(int64_t p,int64_t k,int64_t i,int64_t value){struct ggml_dyn_tallocr*a=(void*)(intptr_t)p;struct tallocr_chunk*c=(void*)(intptr_t)p;struct vbuffer*v=(void*)(intptr_t)p;struct buffer_address*addr=(void*)(intptr_t)p;
 switch(k){case 0:a->alignment=value;return;case 1:a->max_chunk_size=value;return;case 2:GGML_ASSERT(i>=0&&i<GGML_VBUFFER_MAX_CHUNKS);a->chunks[i]=(void*)(intptr_t)value;return;case 3:GGML_ASSERT(value>=0&&value<=GGML_VBUFFER_MAX_CHUNKS);a->n_chunks=value;return;case 4:GGML_ASSERT(i>=0&&i<GGML_VBUFFER_MAX_CHUNKS);v->chunks[i]=(void*)(intptr_t)value;return;case 10:GGML_ASSERT(value>=0&&value<=MAX_FREE_BLOCKS);c->n_free_blocks=value;return;case 11:GGML_ASSERT(i>=0&&i<MAX_FREE_BLOCKS);c->free_blocks[i].offset=value;return;case 12:GGML_ASSERT(i>=0&&i<MAX_FREE_BLOCKS);c->free_blocks[i].size=value;return;case 13:c->max_size=value;return;case 20:addr->chunk=value;return;case 21:addr->offset=value;return;default:GGML_ABORT("bad Goose memory write");}}
int64_t fg153_ms(int64_t op,int64_t a,int64_t b,int64_t c){switch(op){case 0:GGML_ABORT("Goose graph memory invariant failed");case 1:return(intptr_t)calloc(1,(size_t)a);case 2:free((void*)(intptr_t)a);return 0;case 3:ggml_backend_buffer_free((void*)(intptr_t)a);return 0;case 4:return ggml_backend_buft_get_max_size((void*)(intptr_t)a);case 5:return(intptr_t)ggml_backend_buft_alloc_buffer((void*)(intptr_t)a,b);case 6:ggml_backend_buffer_set_usage((void*)(intptr_t)a,b);return 0;case 7:return ggml_backend_buffer_get_size((void*)(intptr_t)a);case 8:ggml_backend_buffer_reset((void*)(intptr_t)a);return 0;case 9:return(intptr_t)ggml_backend_buffer_get_base((void*)(intptr_t)a);case 10:return ggml_backend_tensor_alloc((void*)(intptr_t)a,(void*)(intptr_t)b,(void*)(intptr_t)c);case 15:{struct tallocr_chunk*chunk=(void*)(intptr_t)a;size_t available;return fg_selfhost_choose_reuse(chunk->free_blocks,chunk->n_free_blocks,b,&available);}default:GGML_ABORT("bad Goose memory primitive");}}

#include "bridge.h"
int fg153_graphalloc_enabled(void){const char*v=getenv("GGML_GOOSE_GRAPH_ALLOCATOR");return !v||strcmp(v,"0")!=0;}
int64_t fg153_gg(int64_t p,int64_t k,int64_t i){struct ggml_gallocr*g=(void*)(intptr_t)p;struct ggml_cgraph*c=(void*)(intptr_t)p;struct ggml_tensor*t=(void*)(intptr_t)p;struct hash_node*h=(void*)(intptr_t)p;struct tensor_alloc*a=(void*)(intptr_t)p;
 switch(k){case 0:return g->n_buffers;case 1:return(intptr_t)g->bufts[i];case 2:return(intptr_t)g->buffers[i];case 3:return(intptr_t)g->buf_tallocs[i];case 4:return g->buffer_growth_limits[i];case 5:return g->hash_set.size;case 6:return(intptr_t)g->hash_values;case 7:return(intptr_t)g->node_allocs;case 8:return g->n_nodes;case 9:return(intptr_t)g->leaf_allocs;case 10:return g->n_leafs;case 11:return(intptr_t)g->bufts;case 12:return(intptr_t)g->buffers;case 13:return(intptr_t)g->buf_tallocs;case 14:return(intptr_t)g->buffer_growth_limits;
 case 15:return(intptr_t)&g->node_allocs[i].dst;case 16:return(intptr_t)&g->node_allocs[i/GGML_MAX_SRC].src[i%GGML_MAX_SRC];case 17:return(intptr_t)&g->leaf_allocs[i].leaf;case 20:return c->n_nodes;case 21:return c->n_leafs;case 22:return(intptr_t)c->nodes[i];case 23:return(intptr_t)c->leafs[i];
 case 30:return(intptr_t)t->data;case 31:return(intptr_t)t->view_src;case 32:return(intptr_t)t->buffer;case 33:return t->flags;case 34:return t->op;case 35:return(intptr_t)t->src[i];case 36:return t->type;case 37:return t->ne[i];case 38:return t->nb[i];case 39:return(intptr_t)t->extra;
 case 40:return h->n_children;case 41:return h->n_views;case 42:return h->buffer_id;case 43:return h->addr.chunk;case 44:return h->addr.offset;case 45:return h->allocated;case 49:return(intptr_t)&h->addr;case 50:return a->buffer_id;case 51:return a->addr.chunk;case 52:return a->addr.offset;case 53:return a->size_max;
 case 70:return sizeof(struct ggml_gallocr);case 71:return sizeof(struct node_alloc);case 72:return sizeof(struct leaf_alloc);case 73:return sizeof(struct hash_node);case 74:return GGML_MAX_SRC;case 75:return GGML_TENSOR_FLAG_OUTPUT;case 78:return GGML_BACKEND_BUFFER_USAGE_COMPUTE;case 79:return GGML_STATUS_SUCCESS;case 84:return ((int64_t*)(intptr_t)p)[i];case 81:return((size_t*)(intptr_t)p)[i];case 82:return(intptr_t)((void**)(intptr_t)p)[i];default:GGML_ABORT("bad Goose graph allocator field");}}
void fg153_gp(int64_t p,int64_t k,int64_t i,int64_t v){struct ggml_gallocr*g=(void*)(intptr_t)p;struct hash_node*h=(void*)(intptr_t)p;struct tensor_alloc*a=(void*)(intptr_t)p;switch(k){
 case 0:g->n_buffers=v;return;case 1:g->bufts[i]=(void*)(intptr_t)v;return;case 2:g->buffers[i]=(void*)(intptr_t)v;return;case 3:g->buf_tallocs[i]=(void*)(intptr_t)v;return;case 4:g->buffer_growth_limits[i]=v;return;case 6:g->hash_values=(void*)(intptr_t)v;return;case 7:g->node_allocs=(void*)(intptr_t)v;return;case 8:g->n_nodes=v;return;case 9:g->leaf_allocs=(void*)(intptr_t)v;return;case 10:g->n_leafs=v;return;case 11:g->bufts=(void*)(intptr_t)v;return;case 12:g->buffers=(void*)(intptr_t)v;return;case 13:g->buf_tallocs=(void*)(intptr_t)v;return;case 14:g->buffer_growth_limits=(void*)(intptr_t)v;return;
 case 30:((struct ggml_tensor*)(intptr_t)p)->data=(void*)(intptr_t)v;return;case 32:((struct ggml_tensor*)(intptr_t)p)->buffer=(void*)(intptr_t)v;return;case 39:((struct ggml_tensor*)(intptr_t)p)->extra=(void*)(intptr_t)v;return;case 84:((int64_t*)(intptr_t)p)[i]=v;return;case 42:h->buffer_id=v;return;case 43:h->addr.chunk=v;return;case 44:h->addr.offset=v;return;case 45:h->allocated=v;return;case 50:a->buffer_id=v;return;case 51:a->addr.chunk=v;return;case 52:a->addr.offset=v;return;case 53:a->size_max=v;return;default:GGML_ABORT("bad Goose graph allocator write");}}
int64_t fg153_gs(int64_t op,int64_t a,int64_t b,int64_t c,int64_t d){struct ggml_gallocr*g=(void*)(intptr_t)a;switch(op){case 0:GGML_ABORT("Goose graph allocator invariant failed");case 1:return(intptr_t)calloc(1,a);case 2:free((void*)(intptr_t)a);return 0;case 3:return(intptr_t)ggml_gallocr_hash_get(g,(void*)(intptr_t)b);case 4:ggml_hash_set_free(&g->hash_set);return 0;
 case 5:return ggml_backend_buft_get_alignment((void*)(intptr_t)a);case 6:return ggml_backend_buft_get_max_size((void*)(intptr_t)a);case 7:return ggml_backend_buft_get_alloc_size((void*)(intptr_t)a,(void*)(intptr_t)b);
 case 8:ggml_hash_set_free(&g->hash_set);g->hash_set=ggml_hash_set_new(b);GGML_ASSERT(g->hash_set.keys);free(g->hash_values);g->hash_values=calloc(g->hash_set.size,sizeof(struct hash_node));GGML_ASSERT(g->hash_values);return 0;
 case 9:ggml_gallocr_alloc_graph_impl(g,(void*)(intptr_t)b,(void*)(intptr_t)c,(void*)(intptr_t)d);return 0;case 10:return ggml_backend_view_init((void*)(intptr_t)a);
 case 11:{struct tallocr_chunk*chunk=(void*)(intptr_t)a;return fg_selfhost_release_block(chunk->free_blocks,chunk->n_free_blocks,b,c);}default:GGML_ABORT("bad Goose graph allocator service");}}
