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

#define BLOCK_SIZE 512

/* EOF cannot be 1, so OK */
#define ERR 1

/*
 * unget is used when the characters in the buffer are stored in reverse order.
 * put is used when the characters are stored in normal order.
 */
#define unget_ch put_ch

/* All unget commands reverse the order of the characters */

struct buf {
    char *a;                    /* Memory */
    size_t i;                   /* Write index */
    size_t s;                   /* Allocated size in bytes */
};

#include "buf_func_dec.h"

#endif
