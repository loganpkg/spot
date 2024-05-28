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
    unsigned int base = 10;     /* Decimal */
    unsigned int n;

    if (str == NULL || *str == '\0') {
        fprintf(stderr, "%s:%d: Error\n", __FILE__, __LINE__);
        return ERR;
    }

    if (*str == '0') {
        ++str;                  /* Eat char */
        if (*str == 'x' || *str == 'X') {
            ++str;              /* Eat char */
            base = 16;          /* Hexadecimal */
        } else {
            base = 8;           /* Octal */
        }
    }

    while ((ch = *str) != '\0') {
        if ((base == 10 && isdigit(ch)) || (base == 16 && isxdigit(ch))
            || (base == 8 && ch >= '0' && ch <= '7')) {
            if (mof(x, base, max_val)) {
                fprintf(stderr, "%s:%d: Error\n", __FILE__, __LINE__);
                return ERR;
            }

            x *= base;

            if (isdigit(ch))
                n = ch - '0';
            else if (islower(ch))
                n = 10 + ch - 'a';
            else
                n = 10 + ch - 'A';

            if (aof(x, n, max_val)) {
                fprintf(stderr, "%s:%d: Error\n", __FILE__, __LINE__);
                return ERR;
            }

            x += n;
        } else {
            fprintf(stderr,
                    "%s:%d: Syntax error: Character is not a %s digit\n",
                    __FILE__, __LINE__,
                    base == 10 ? "decimal" : (base ==
                                              16 ? "hexadecimal" :
                                              "octal"));
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

int lop(long *a, long b, unsigned char op)
{
    /*
     * long operation. Checks for signed long overflow.
     * Result is stored in a. b is only used in binary operations.
     */

    switch (op) {
    case POSITIVE:             /* Nothing to do */
        break;
    case NEGATIVE:
        return lop(a, -1, MULTIPLICATION);
    case BITWISE_COMPLEMENT:
        *a = ~*a;
        break;
    case LOGICAL_NEGATION:
        *a = !*a;
        break;
    case EXPONENTIATION:
        return lpow(a, b);
    case MULTIPLICATION:
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
        break;
    case DIVISION:
    case MODULO:
        if (!b) {
            fprintf(stderr, "%s:%d: Divide by zero\n", __FILE__, __LINE__);
            return DIV_BY_ZERO_ERR;
        }

        if (*a == LONG_MIN && b == -1) {
            fprintf(stderr, "%s:%d: User overflow error\n", __FILE__,
                    __LINE__);
            return USER_OVERFLOW_ERR;
        }
        if (op == DIVISION)
            *a /= b;
        else
            *a %= b;

        break;
    case ADDITION:
        /* Need to be the same sign to overflow */
        if ((*a > 0 && b > 0 && *a > LONG_MAX - b)
            || (*a < 0 && b < 0 && *a < LONG_MIN - b)) {
            fprintf(stderr, "%s:%d: User overflow error\n", __FILE__,
                    __LINE__);
            return USER_OVERFLOW_ERR;
        }
        *a += b;
        break;
    case SUBTRACTION:
        if ((b < 0 && *a > LONG_MAX + b)
            || (b > 0 && *a < LONG_MIN + b)) {
            fprintf(stderr, "%s:%d: User overflow error\n", __FILE__,
                    __LINE__);
            return USER_OVERFLOW_ERR;
        }
        *a -= b;
        break;
    case BITWISE_LEFT_SHIFT:
        *a <<= b;
        break;
    case BITWISE_RIGHT_SHIFT:
        *a >>= b;
        break;
    case LESS_THAN:
        *a = *a < b;
        break;
    case LESS_THAN_OR_EQUAL:
        *a = *a <= b;
        break;
    case GREATER_THAN:
        *a = *a > b;
        break;
    case GREATER_THAN_OR_EQUAL:
        *a = *a >= b;
        break;
    case EQUAL:
        *a = *a == b;
        break;
    case NOT_EQUAL:
        *a = *a != b;
        break;
    case BITWISE_AND:
        *a &= b;
        break;
    case BITWISE_XOR:
        *a ^= b;
        break;
    case BITWISE_OR:
        *a |= b;
        break;
    case LOGICAL_AND:
        *a = *a && b;
        break;
    case LOGICAL_OR:
        *a = *a || b;
        break;
    default:
        fprintf(stderr, "%s:%d: Syntax error: Invalid operator\n",
                __FILE__, __LINE__);
        return SYNTAX_ERR;
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
        if ((ret = lop(&x, *a, MULTIPLICATION)))
            return ret;

    *a = x;
    return 0;
}
