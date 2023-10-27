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

/* Buffer module */

#ifndef BUF_H
#define BUF_H

#include <stddef.h>

#define buf_members                                           \
    char *a;                    /* Memory */                  \
    size_t i;                   /* Write index */             \
    size_t s                    /* Allocated size in bytes */

/*
 * Input buffer. Characters are stored in reverse order.
 * Operated on by get and unget functions.
 */
struct ibuf {
    buf_members;
};

/*
 * Ouput buffer. Characters are stored in normal order.
 * Operated on by put functions.
 */
struct obuf {
    buf_members;
};

#undef buf_members

/* Buffer of long integers */
struct lbuf {
    long *a;                    /* Memory */
    size_t i;                   /* Write index */
    size_t n;                   /* Number of elements, not bytes */
};

#include "buf_func_dec.h"

#endif
