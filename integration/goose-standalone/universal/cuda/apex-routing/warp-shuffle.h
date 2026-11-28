#define GS_UNREACHABLE(file, line) __asm__("trap;")
#define GS_RANGE_U(value, high, type, file, line) (value)
#define gs_shr_u64(a,b,...) ((a) >> (b))
#define gs_shl_u64(a,b,...) ((a) << (b))
static uint route_shuffle(uint input, int offset) {
    uint output;
    __asm__("shfl.sync.bfly.b32 %0, %1, %2, 31, -1;" : "=r"(output) : "r"(input), "r"(offset));
    return output;
}
