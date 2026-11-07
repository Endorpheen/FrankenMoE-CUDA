#include <stdint.h>
#include <stddef.h>
#include <assert.h>
#define MAX_FREE_BLOCKS 256
#define GGML_ASSERT assert
struct free_block {size_t offset,size;};
struct tallocr_chunk {struct free_block *free_blocks;int n_free_blocks;};
struct buffer_address {int chunk;size_t offset;};
struct ggml_dyn_tallocr {size_t alignment;struct tallocr_chunk *chunks[16];};
static size_t aligned_offset(const void *p,size_t n,size_t alignment){assert(p==NULL && alignment==1);return n;}
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

static void ggml_dyn_tallocr_free_bytes(struct ggml_dyn_tallocr * alloc, struct buffer_address addr, size_t size) {
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


int fg_native_release_block(void *blocks,int count,size_t offset,size_t size){
 struct tallocr_chunk chunk={blocks,count};struct ggml_dyn_tallocr alloc={.alignment=1,.chunks={&chunk}};
 ggml_dyn_tallocr_free_bytes(&alloc,(struct buffer_address){0,offset},size);return chunk.n_free_blocks;
}
