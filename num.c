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

#include "toucanlib.h"


int str_to_num(const char *str, unsigned long max_val, unsigned long *res)
{
    unsigned char ch;
    unsigned long x = 0;

    if (str == NULL || *str == '\0') {
        fprintf(stderr, "%s:%d: Error\n", __FILE__, __LINE__);
        return ERR;
    }

    while ((ch = *str) != '\0') {
        if (isdigit(ch)) {
            if (mof(x, 10, max_val)) {
                fprintf(stderr, "%s:%d: Error\n", __FILE__, __LINE__);
                return ERR;
            }

            x *= 10;
            if (aof(x, ch - '0', max_val)) {
                fprintf(stderr, "%s:%d: Error\n", __FILE__, __LINE__);
                return ERR;
            }

            x += ch - '0';
        } else {
            fprintf(stderr,
                    "%s:%d: Syntax error: Character is not a digit\n",
                    __FILE__, __LINE__);
            return SYNTAX_ERR;
        }

        ++str;
    }
    *res = x;
    return 0;
}

int str_to_size_t(const char *str, size_t *res)
{
    unsigned long n;

    if (str_to_num(str, SIZE_MAX, &n)) {
        fprintf(stderr, "%s:%d: Error\n", __FILE__, __LINE__);
        return ERR;
    }

    *res = (size_t) n;
    return 0;
}

int hex_to_val(unsigned char h[2], unsigned char *res)
{
    unsigned char x;
    size_t i;

    x = 0;
    for (i = 0; i < 2; ++i) {
        if (i)
            x *= 16;

        if (!isxdigit(h[i])) {
            fprintf(stderr, "%s:%d: Error\n", __FILE__, __LINE__);
            return ERR;
        }

        if (isdigit(h[i]))
            x += h[i] - '0';
        else if (islower(h[i]))
            x += h[i] - 'a' + 10;
        else if (isupper(h[i]))
            x += h[i] - 'A' + 10;
    }
    *res = x;
    return 0;
}

int lop(long *a, long b, char op)
{
    /* long operation. Checks for signed long overflow. */
    if (op == '*') {
        if (!*a)
            return 0;

        if (!b) {
            *a = 0;
            return 0;
        }
        /* Same sign, result will be positive */
        if (*a > 0 && b > 0 && *a > LONG_MAX / b) {
            fprintf(stderr, "%s:%d: User overflow error\n", __FILE__,
                    __LINE__);
            return USER_OVERFLOW_ERR;
        }

        if (*a < 0 && b < 0 && *a < LONG_MAX / b) {
            fprintf(stderr, "%s:%d: User overflow error\n", __FILE__,
                    __LINE__);
            return USER_OVERFLOW_ERR;
        }

        /* Opposite sign, result will be negative */
        if (*a > 0 && b < 0 && b < LONG_MIN / *a) {
            fprintf(stderr, "%s:%d: User overflow error\n", __FILE__,
                    __LINE__);
            return USER_OVERFLOW_ERR;
        }

        if (*a < 0 && b > 0 && *a < LONG_MIN / b) {
            fprintf(stderr, "%s:%d: User overflow error\n", __FILE__,
                    __LINE__);
            return USER_OVERFLOW_ERR;
        }

        *a *= b;
    } else if (op == '/' || op == '%') {
        if (!b) {
            fprintf(stderr, "%s:%d: Divide by zero\n", __FILE__, __LINE__);
            return DIV_BY_ZERO_ERR;
        }

        if (*a == LONG_MIN && b == -1) {
            fprintf(stderr, "%s:%d: User overflow error\n", __FILE__,
                    __LINE__);
            return USER_OVERFLOW_ERR;
        }

        if (op == '/')
            *a /= b;
        else
            *a %= b;
    } else if (op == '+') {
        /* Need to be the same sign to overflow */
        if ((*a > 0 && b > 0 && *a > LONG_MAX - b)
            || (*a < 0 && b < 0 && *a < LONG_MIN - b)) {
            fprintf(stderr, "%s:%d: User overflow error\n", __FILE__,
                    __LINE__);
            return USER_OVERFLOW_ERR;
        }

        *a += b;
    } else if (op == '-') {
        if ((b < 0 && *a > LONG_MAX + b)
            || (b > 0 && *a < LONG_MIN + b)) {
            fprintf(stderr, "%s:%d: User overflow error\n", __FILE__,
                    __LINE__);
            return USER_OVERFLOW_ERR;
        }

        *a -= b;
    } else if (op == '^') {
        return lpow(a, b);
    }

    return 0;
}

int lpow(long *a, long b)
{
    int ret = ERR;
    long x;

    if (!b) {
        /* Anything to the power of zero is one */
        *a = 1;
        return 0;
    }
    if (!*a || b == 1) {
        return 0;
    }
    if (b < 0) {
        fprintf(stderr, "%s:%d: Syntax error: Negative exponent\n",
                __FILE__, __LINE__);
        return SYNTAX_ERR;
    }

    x = *a;
    while (--b)
        if ((ret = lop(&x, *a, '*')))
            return ret;

    *a = x;
    return 0;
}
