typedef uchar uint8_t;
typedef ushort uint16_t;
typedef uint uint32_t;
typedef ulong uint64_t;
typedef int int32_t;
typedef long int64_t;
#define GG_UNALIGNED
#define GG_ALIGN1 __attribute__((aligned(1)))
#define UINT64_C(x) x##UL
#define gg_double_bits(x) as_double((ulong)(x))
