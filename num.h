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

#ifndef NUM_H
#define NUM_H

#include <stddef.h>

/* Unsigned overflow tests */
/* Addition */
#define aof(a, b, max_val) ((a) > (max_val) - (b))

/* Multiplication */
#define mof(a, b, max_val) ((a) && (b) > (max_val) / (a))

/* Function declarations */
int str_to_num(const char *str, unsigned long max_val, unsigned long *res);
int str_to_size_t(const char *str, size_t *res);
int hex_to_val(unsigned char h[2], unsigned char *res);
int lop(long *a, long b, char op);
int lpow(long *a, long b);

#endif
