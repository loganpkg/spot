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

#ifdef __linux__
#define _XOPEN_SOURCE 500
#endif

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "xxh64.h"

#ifdef _WIN32
#define lx "llx"
#else
#define lx "lx"
#endif

static int check_hash(const char *str, const char *hash)
{
    char res[17];
    snprintf(res, 17, "%016" lx, xxh64(str, strlen(str)));
    printf("%s\n", res);
    if (strcmp(res, hash)) {
        fprintf(stderr, "ERROR: Hash check failed\n");
        return 1;
    } else {
        printf("Hash OK\n");
    }
    return 0;
}

int main(void)
{
    if (check_hash("", "ef46db3751d8e999"))
        return 1;

    if (check_hash("a", "d24ec4f1a98c6e5b"))
        return 1;

    if (check_hash("ab", "65f708ca92d04a61"))
        return 1;

    if (check_hash("abc", "44bc2cf5ad770999"))
        return 1;

    if (check_hash("abcd", "de0327b0d25d92cc"))
        return 1;

    if (check_hash("abcde", "07e3670c0c8dc7eb"))
        return 1;

    if (check_hash("abcdefgh", "3ad351775b4634b7"))
        return 1;

    if (check_hash("012345678901234567890123456789a", "1a0caa81af517a5a"))
        return 1;

    if (check_hash("012345678901234567890123456789ab", "f0a1034068425577"))
        return 1;

    if (check_hash("012345678901234567890123456789abc", "17f4ec809ab3fe02"))
        return 1;

    if (check_hash(
            "012345678901234567890123456789012345678901234567890123456789",
            "66ea70f4211f2a4f"))
        return 1;

    if (check_hash(
            "012345678901234567890123456789012345678901234567890123456789\n"
            "012345678901234567890123456789012345678901234567890123456789\n"
            "012345678901234567890123456789012345678901234567890123456789\n"
            "012345678901234567890123456789012345678901234567890123456789\n"
            "012345678901234567890123456789012345678901234567890123456789\n"
            "012345678901234567890123456789012345678901234567890123456789\n"
            "012345678901234567890123456789012345678901234567890123456789\n",
            "0c80b035e36fdb56"))
        return 1;

    return 0;
}
