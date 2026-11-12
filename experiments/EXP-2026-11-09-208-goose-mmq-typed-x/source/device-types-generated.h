#pragma once
#if defined(_MSC_VER)
#define GG_ALIGN1
#if defined(_M_IX86)
#define GG_UNALIGNED
#else
#define GG_UNALIGNED __unaligned
#endif
#else
#define GG_UNALIGNED
#define GG_ALIGN1 __attribute__((aligned(1)))
#endif

typedef uint64_t fg208qd_gg_unaligned_1 GG_ALIGN1;
typedef int64_t fg208qd_gg_unaligned_2 GG_ALIGN1;
typedef float fg208qd_gg_unaligned_3 GG_ALIGN1;
typedef double fg208qd_gg_unaligned_4 GG_ALIGN1;
typedef const GG_UNALIGNED fg208qd_gg_unaligned_3 *fg208qd_gg_type_5;
typedef fg208qd_gg_type_5 fg208qd_gg_unaligned_5 GG_ALIGN1;
typedef int32_t fg208qd_gg_unaligned_6 GG_ALIGN1;
typedef uint32_t fg208qd_gg_unaligned_9 GG_ALIGN1;
typedef uint8_t fg208qd_gg_unaligned_10 GG_ALIGN1;
