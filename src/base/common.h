#ifndef BASE_COMMON_H
#define BASE_COMMON_H

#include <stdbool.h>
#include <stdint.h>

#define ArraySize(a) (sizeof(a) / sizeof((a)[0]))

#define Likely(x)   __builtin_expect(x, 1)
#define Unlikely(x) __builtin_expect(x, 1)

#define Bytes(n) (n)
#define Kilobytes(n) (n << 10)
#define Megabytes(n) (n << 20)
#define Gigabytes(n) (((uint64_t)n) << 30)
#define Terabytes(n) (((uint64_t)n) << 40)

#define AlignPow2(x, b) (((x) + (b) - 1) & (~((b) - 1)))

#define Min(a, b) (((a) < (b)) ? (a) : (b))
#define Max(a, b) (((a) > (b)) ? (a) : (b))

// clang-format off
typedef uint8_t  u8;
typedef int8_t   s8;
typedef uint16_t u16;
typedef int16_t  s16;
typedef uint32_t u32;
typedef int32_t  s32;
typedef uint64_t u64;
typedef int64_t  s64;
// clang-format on

#endif // BASE_COMMON_H