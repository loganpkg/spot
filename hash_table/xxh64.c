/*
 * Copyright (c) 2026 Logan Ryan McLintock. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * Implementation of XXH64.
 *
 * Reference:
 * xxHash fast digest algorithm, Yann Collet & Simon Josefsson (editor),
 *     11 October 2025, Internet-Draft: draft-josefsson-xxhash-00,
 *     Internet Engineering Task Force (IETF),
 *     [Please note that this draft is a work in progress].
 */

#include "xxh64.h"

#define SEED 0

#define P1 0x9E3779B185EBCA87
#define P2 0xC2B2AE3D27D4EB4F
#define P3 0x165667B19E3779F9
#define P4 0x85EBCA77C2B2AE63
#define P5 0x27D4EB2F165667C5

/*
 * 64-bit circular ROtate Left (rol).
 * Evaluates the arguments twice.
 */
#define rol(x, n) ((x) << (n) | (x) >> (64 - (n)))

/* These macros have side-effects. Do not use them out of context. */

/*
 * Loading byte-by-byte avoids alignment issues when `mem % 8 != 0.'
 * This code also works on both little endian and big endian systems.
 */
#define load_8                                                                \
    do {                                                                      \
        y = (unsigned char *) &x;                                             \
        if (le) {                                                             \
            *y++ = *q++;                                                      \
            *y++ = *q++;                                                      \
            *y++ = *q++;                                                      \
            *y++ = *q++;                                                      \
            *y++ = *q++;                                                      \
            *y++ = *q++;                                                      \
            *y++ = *q++;                                                      \
            *y = *q++;                                                        \
        } else {                                                              \
            y += 7;                                                           \
            *y = *q++;                                                        \
            *--y = *q++;                                                      \
            *--y = *q++;                                                      \
            *--y = *q++;                                                      \
            *--y = *q++;                                                      \
            *--y = *q++;                                                      \
            *--y = *q++;                                                      \
            *--y = *q++;                                                      \
        }                                                                     \
    } while (0)

#define load_4                                                                \
    do {                                                                      \
        y = (unsigned char *) &x;                                             \
        x = 0;                                                                \
        if (le) {                                                             \
            *y++ = *q++;                                                      \
            *y++ = *q++;                                                      \
            *y++ = *q++;                                                      \
            *y = *q++;                                                        \
        } else {                                                              \
            y += 7;                                                           \
            *y = *q++;                                                        \
            *--y = *q++;                                                      \
            *--y = *q++;                                                      \
            *--y = *q++;                                                      \
        }                                                                     \
    } while (0)

#define load_1                                                                \
    do {                                                                      \
        y = (unsigned char *) &x;                                             \
        x = 0;                                                                \
        if (!le)                                                              \
            y += 7;                                                           \
                                                                              \
        *y = *q++;                                                            \
    } while (0)

#define mix(aN)                                                               \
    do {                                                                      \
        load_8;                                                               \
        aN += x * P2;                                                         \
        aN = rol(aN, 31) * P1;                                                \
    } while (0)

#define merge(aN)                                                             \
    do {                                                                      \
        x = aN * P2;                                                          \
        a ^= rol(x, 31) * P1;                                                 \
        a *= P1;                                                              \
        a += P4;                                                              \
    } while (0)

static int is_little_endian(void)
{
    long x;
    unsigned char *q;

    x = 1;
    q = (unsigned char *) &x;
    if (*q)
        return 1; /* Little endian system. */

    return 0;
}

uint64_t xxh64(const void *mem, size_t size)
{
    /* Implementation of XXH64. */
    uint64_t a, a1, a2, a3, a4, x;
    const unsigned char *q, *q_stop;
    unsigned char *y;
    size_t r;
    int le;

    le = is_little_endian();

    a1 = SEED + P1 + P2;
    a2 = SEED + P2;
    a3 = SEED;
    a4 = SEED - P1;

    q = (unsigned char *) mem;

    if (size < 32) {
        a = SEED + P5;
    } else {
        q_stop = q + (size / 32) * 32;

        while (q < q_stop) {
            mix(a1);
            mix(a2);
            mix(a3);
            mix(a4);
        }

        a = rol(a1, 1) + rol(a2, 7) + rol(a3, 12) + rol(a4, 18);
        merge(a1);
        merge(a2);
        merge(a3);
        merge(a4);
    }

    a += size;

    r = size % 32; /* Remaining size. */

    while (r >= 8) {
        load_8;
        x *= P2;
        a ^= rol(x, 31) * P1;
        a = rol(a, 27) * P1 + P4;
        r -= 8;
    }

    if (r >= 4) {
        load_4;
        a ^= x * P1;
        a = rol(a, 23) * P2 + P3;
        r -= 4;
    }

    while (r >= 1) {
        load_1;
        a ^= x * P5;
        a = rol(a, 11) * P1;
        --r;
    }

    a ^= a >> 33;
    a *= P2;
    a ^= a >> 29;
    a *= P3;
    a ^= a >> 32;

    return a;
}
