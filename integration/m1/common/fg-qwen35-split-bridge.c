// Bridge for goose/misc/qwen35-split.goose - the Qwen pre-tokenisation splitter.
//
// The module is FFI-light by design: four accessors and nothing else, so the whole nine-rule state machine runs in
// Goose and the boundary is one call per invocation rather than one per character.

#include <stdint.h>
#include <stdlib.h>

int64_t q35s_split(int64_t packet);
void fg_qwen35_split_goose_init(int argc, char ** argv);

int64_t q35s_ld_u32(uint64_t base, int64_t byte) { return (int64_t) *(const uint32_t *) (uintptr_t) (base + (uint64_t) byte); }
int64_t q35s_ld_u16(uint64_t base, int64_t byte) { return (int64_t) *(const uint16_t *) (uintptr_t) (base + (uint64_t) byte); }
void    q35s_st_i64(uint64_t base, int64_t byte, int64_t v) { *(int64_t *) (uintptr_t) (base + (uint64_t) byte) = v; }
void    q35s_fail(int64_t code) { (void) code; abort(); }

// Initialisation is explicit here because the splitter is only reached through q35s_split; the working runtime
// calls this from its own ensure_init, the gate calls it directly.
void fg_qwen35_split_init(void) { fg_qwen35_split_goose_init(0, (char **) 0); }

int64_t q35s_ld_i64(uint64_t base, int64_t byte) { return *(const int64_t *) (uintptr_t) (base + (uint64_t) byte); }
