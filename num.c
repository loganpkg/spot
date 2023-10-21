/*
 * Copyright (c) 2023 Logan Ryan McLintock
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/* Number module */

#include <ctype.h>
#include <stddef.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "num.h"
#include "debug.h"

int str_to_num(const char *str, unsigned long max_val, unsigned long *res)
{
    unsigned char ch;
    unsigned long x = 0;

    if (str == NULL || *str == '\0')
        mreturn(1);

    while ((ch = *str) != '\0') {
        if (isdigit(ch)) {
            if (mof(x, 10, max_val))
                return 1;

            x *= 10;
            if (aof(x, ch - '0', max_val))
                return 1;

            x += ch - '0';
        } else {
            return 1;
        }

        ++str;
    }
    *res = x;
    return 0;
}

int str_to_size_t(const char *str, size_t *res)
{
    unsigned long n;

    if (str_to_num(str, SIZE_MAX, &n))
        return 1;

    *res = (size_t) n;
    return 0;
}
