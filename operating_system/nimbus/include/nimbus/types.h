/* ===========================================================================
 *  nimbus/include/nimbus/types.h  --  the vocabulary
 * ===========================================================================
 *
 *  Every type in the kernel is defined here, and every one of them says
 *  exactly how many bits it has. In application code `int` is fine because you
 *  rarely care; in a kernel almost every integer is either a hardware register,
 *  a field in a structure the CPU reads, or an address, and for all three the
 *  width is part of the contract.
 *
 *  Targets: i686 (32-bit x86), System V ABI, GCC.
 * =========================================================================== */
#ifndef NIMBUS_TYPES_H
#define NIMBUS_TYPES_H

typedef unsigned char       uint8_t;
typedef signed char         int8_t;
typedef unsigned short      uint16_t;
typedef signed short        int16_t;
typedef unsigned int        uint32_t;
typedef signed int          int32_t;
typedef unsigned long long  uint64_t;
typedef signed long long    int64_t;

/*  size_t is "big enough to index any object". On 32-bit x86 that is 32 bits.
 *  ssize_t is its signed twin, used by read()/write() so they can return -1.  */
typedef uint32_t            size_t;
typedef int32_t             ssize_t;

/*  uintptr_t is "an integer big enough to hold a pointer". Converting pointers
 *  through it rather than through uint32_t is what keeps the day you port to
 *  x86-64 from being a week.                                                  */
typedef uint32_t            uintptr_t;
typedef int32_t             ptrdiff_t;

/*  Two address types that are numerically identical and conceptually not.
 *  After Chapter 24 the difference between them is the difference between a
 *  working kernel and a triple fault, so the kernel names them apart and every
 *  function signature says which one it wants.                                */
typedef uint32_t            paddr_t;   /* physical: what goes on the bus       */
typedef uint32_t            vaddr_t;   /* virtual:  what your code dereferences*/

typedef int32_t             pid_t;
typedef uint32_t            off_t;
typedef uint32_t            mode_t;

#define NULL ((void *)0)

typedef _Bool bool;
#define true  1
#define false 0

/*  Varargs. <stdarg.h> is freestanding and we could include it, but these three
 *  macros are the entire interface and GCC implements them as builtins, so
 *  writing them out costs nothing and removes one header from the build.      */
typedef __builtin_va_list   va_list;
#define va_start(v, l)      __builtin_va_start(v, l)
#define va_arg(v, t)        __builtin_va_arg(v, t)
#define va_end(v)           __builtin_va_end(v)
#define va_copy(d, s)       __builtin_va_copy(d, s)

/*  Attributes we use often enough to deserve short names.                     */
#define PACKED      __attribute__((packed))
#define ALIGNED(n)  __attribute__((aligned(n)))
#define NORETURN    __attribute__((noreturn))
#define UNUSED      __attribute__((unused))
#define ALWAYS_INLINE inline __attribute__((always_inline))

/*  PACKED deserves a warning. It tells GCC "lay this struct out with no
 *  padding", which is exactly right for a structure the *hardware* defines --
 *  a GDT descriptor, an ELF header, a FAT directory entry. It is exactly wrong
 *  for anything else: unaligned loads are slower, and taking the address of a
 *  packed field gives you a pointer GCC will not trust. Use it only where a
 *  chip or a file format dictated the layout.                                 */

#define ARRAY_LEN(a)     (sizeof(a) / sizeof((a)[0]))
#define MIN(a, b)        ((a) < (b) ? (a) : (b))
#define MAX(a, b)        ((a) > (b) ? (a) : (b))

/*  Round up/down to a power-of-two boundary, branchlessly.
 *  ALIGN_UP(0x1001, 0x1000) == 0x2000; ALIGN_DOWN(0x1001, 0x1000) == 0x1000.  */
#define ALIGN_DOWN(x, a) ((x) & ~((a) - 1))
#define ALIGN_UP(x, a)   ALIGN_DOWN((x) + (a) - 1, (a))
#define IS_ALIGNED(x, a) (((x) & ((a) - 1)) == 0)

#define KiB 1024U
#define MiB (1024U * KiB)
#define GiB (1024U * MiB)

#endif /* NIMBUS_TYPES_H */
