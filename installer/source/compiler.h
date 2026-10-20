/*
 * compiler - optional compiler checks, as portable macros.
 *
 * The code is standard C99. These macros add checks that gcc and clang
 * (devkitARM, CI, the PC tests) understand, and expand to nothing elsewhere.
 */
#ifndef COMPILER_H
#define COMPILER_H

/* Marks a printf-style function: argument `fmt` (1-based) is the format and
 * the variable arguments start at `first`, so the compiler checks callers. */
#if defined(__GNUC__) || defined(__clang__)
#define PRINTF_LIKE(fmt, first) __attribute__((format(printf, fmt, first)))
#else
#define PRINTF_LIKE(fmt, first)
#endif

#endif
