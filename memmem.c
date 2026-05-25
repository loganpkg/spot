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

#include "memmem.h"
#include <limits.h>
#include <stdint.h>

void *memmem(
    const void *big, size_t big_size, const void *small, size_t small_size)
{
    /*
     * Searches for an exact match of `small' inside of `big'
     * using the "Simplified Quick Search" algorithm.
     *
     * Reference:
     * Daniel M. Sunday, A Very Fast Substring Search Algorithm,
     *     Substring Search, COMMUNICATIONS OF THE ACM,
     *     August 1990, Vol.33, No.8, pp. 140, 142.
     *
     * This is a fantastic algorithm that is fast and easy to implement.
     * Thank you!
     */

    size_t jump[UCHAR_MAX + 1];

    /*
     * Casting the pointers allows for pointer arithmetic, and the
     * unsigned char data type allows for using the characters as
     * indices into the jump array.
     */

    /* Pointer to `big' that advances by the jumps as matches fail. */
    const unsigned char *b;

    const unsigned char *b_end; /* End of `big' (exclusive). */

    /* Last position in `big' where `small' could fit (inclusive). */
    const unsigned char *b_max;

    const unsigned char *p; /* Cast version of `small.' */

    /* Used for checking a particular potenital match. */
    const unsigned char *b_check, *p_check;

    const unsigned char *p_end; /* End of `small' (exclusive). */

    unsigned char u;
    size_t i;

    if (small_size > big_size)
        return NULL;

    /* A zero-sized `small' is deemed to match `big' immediately. */
    if (!small_size)
        return (void *) big;

    b = big;
    b_end = b + big_size;
    b_max = b + big_size - small_size;
    p = small;
    p_end = p + small_size;

    if (small_size == SIZE_MAX) {
        /* Special case. Just check for an exact match. */
        for (i = 0; i < small_size; ++i)
            if (*(b + i) != *(p + i))
                break;

        if (i == small_size)
            return (void *) b; /* Match. */
        else
            return NULL; /* No match. */
    }

    if (small_size == 1) {
        /* Special case. Just search for the character. */
        u = *p;

        while (b < b_end)
            if (*b++ == u)
                return (void *) --b; /* Decrement to undo the increment. */

        return NULL;
    }

    /*
     * Initialise the jump sizes to the maximum jump of
     * small_size plus one, which will be used for characters
     * that are not present in `small.'
     */
    for (i = 0; i < UCHAR_MAX + 1; ++i) jump[i] = small_size + 1;

    /*
     * Reduce jumps for characters in `small.' The closer a character
     * appears to the end of `small,' the smaller the jump.
     * The minimum jump is one, which will occur for the last character
     * in `small.' Conversely, if a character only appears in the first
     * position of `small', then it will have a jump of small_size.
     */
    for (i = 0; i < small_size; ++i) jump[*(p + i)] = small_size - i;

    /* Keep looping while `small' could possibly fit. */
    while (b <= b_max) {

        /* Check for match. */
        b_check = b;
        p_check = p;
        while (p_check < p_end)
            if (*p_check++ != *b_check++)
                break;

        if (p_check == p_end)
            return (void *) b; /* Match. */

        /*
         * Only the jump value advances b.
         * The biggest jump is small_size plus one,
         * so b will only exceed `big' memory buffer by one,
         * which is guaranted not to overflow by the C standard.
         */
        b += jump[*(b + small_size)];
    }

    return NULL; /* Reached the end and no match. */
}
