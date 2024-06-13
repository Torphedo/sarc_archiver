#ifndef INT_H
#define INT_H

#include <stdint.h>

// Shorthands for the standard-size int types
typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef int8_t s8;
typedef int16_t s16;
typedef int32_t s32;
typedef int64_t s64;

// Round a number up to any boundary
#define ALIGN_UP(x, bound) (((x - 1) & (bound * 0xF)) + bound)

// Prevent conflicts with other headers that have their own MIN/MAX
#undef MIN
#undef MAX
// Return the larger of 2 values
#define MAX(a, b) ((a > b) ? a : b)
// Return the smaller of 2 values
#define MIN(a, b) ((a < b) ? a : b)

#define MAGIC(a, b, c, d) ((u32)a | ((u32)b << 8) | ((u32)c << 16) | ((u32)d << 24))

#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof(*arr))
#endif // INT_H
