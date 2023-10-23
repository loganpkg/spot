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
