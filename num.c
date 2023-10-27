/*
 * Copyright (c) 2023 Logan Ryan McLintock
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

/* Number module */

#include <ctype.h>
#include <limits.h>
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
                mreturn(1);

            x *= 10;
            if (aof(x, ch - '0', max_val))
                mreturn(1);

            x += ch - '0';
        } else {
            mreturn(1);
        }

        ++str;
    }
    *res = x;
    mreturn(0);
}

int str_to_size_t(const char *str, size_t *res)
{
    unsigned long n;

    if (str_to_num(str, SIZE_MAX, &n))
        mreturn(1);

    *res = (size_t) n;
    mreturn(0);
}

int lop(long *a, long b, char op)
{
    /* long operation. Checks for signed long overflow. */
    if (op == '*') {
        if (!*a)
            mreturn(0);

        if (!b) {
            *a = 0;
            mreturn(0);
        }
        /* Same sign, result will be positive */
        if (*a > 0 && b > 0 && *a > LONG_MAX / b)
            mreturn(1);

        if (*a < 0 && b < 0 && *a < LONG_MAX / b)
            mreturn(1);

        /* Opposite sign, result will be negative */
        if (*a > 0 && b < 0 && b < LONG_MIN / *a)
            mreturn(1);

        if (*a < 0 && b > 0 && *a < LONG_MIN / b)
            mreturn(1);

        *a *= b;
    } else if (op == '/' || op == '%') {
        if (!b || (*a == LONG_MIN && b == -1))
            mreturn(1);

        if (op == '/')
            *a /= b;
        else
            *a %= b;
    } else if (op == '+') {
        /* Need to be the same sign to overflow */
        if ((*a > 0 && b > 0 && *a > LONG_MAX - b)
            || (*a < 0 && b < 0 && *a < LONG_MIN - b))
            mreturn(1);

        *a += b;
    } else if (op == '-') {
        if ((b < 0 && *a > LONG_MAX + b)
            || (b > 0 && *a < LONG_MIN + b))
            mreturn(1);

        *a -= b;
    } else if (op == '^') {
        mreturn(lpow(a, b));
    }

    mreturn(0);
}

int lpow(long *a, long b)
{
    long x;

    if (!b) {
        /* Anything to the power of zero is one */
        *a = 1;
        mreturn(0);
    }
    if (!*a || b == 1) {
        mreturn(0);
    }
    if (b < 0)
        mreturn(1);

    x = *a;
    while (--b)
        if (lop(&x, *a, '*'))
            mreturn(1);

    *a = x;
    mreturn(0);
}
