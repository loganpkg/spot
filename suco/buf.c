/*
 * Copyright (c) 2025 Logan Ryan McLintock. All rights reserved.
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "buf.h"
#include "debug.h"
#include "int.h"

struct buf {
    char *a;   /* Memory. */
    size_t i;  /* Index of next unused element. */
    size_t n;  /* Number of elements allocated. */
    size_t es; /* Element size. */
};

void free_buf(Buf b)
{
    if (b != NULL) {
        free(b->a);
        free(b);
    }
}

Buf init_buf(size_t init_num_elements, size_t element_size)
{
    Buf b = NULL;

    if (!init_num_elements || !element_size)
        debug(goto error);

    if ((b = calloc(1, sizeof(struct buf))) == NULL)
        debug(goto error);

    b->a = NULL; /* Do not assume that NULL is zero. */

    if (mult_overflow(init_num_elements, element_size))
        debug(goto error);

    if ((b->a = calloc(init_num_elements, element_size)) == NULL)
        debug(goto error);

    b->n = init_num_elements;
    b->es = element_size;

    return b;

error:
    free_buf(b);
    debug(return NULL);
}

int push(Buf b, void *object)
{
    /* Assumes object is of size b->es. */
    size_t new_n;
    void *t;

    if (b->i == b->n) {
        /* Need to grow the buffer. */
        if (mult_overflow(b->n, 2))
            debug(return 1);

        new_n = b->n * 2;

        if (mult_overflow(new_n, b->es))
            debug(return 1);

        if ((t = realloc(b->a, new_n * b->es)) == NULL)
            debug(return 1);

        b->a = t;
        b->n = new_n;
    }

    memmove(b->a + b->i++ * b->es, object, b->es);

    return 0;
}

int pop(Buf b, void *result)
{
    if (!b->i)
        return 1; /* Only returns 1 when empty. */

    memmove(result, b->a + --b->i * b->es, b->es);

    return 0;
}

void truncate_buf(Buf b)
{
    b->i = 0;
}

void *get_buf_element(Buf b, size_t element)
{
    /* Pointer can change after reallocation, so not safe to use after push. */
    if (element >= b->i)
        debug(return NULL); /* Out of bounds. */

    return b->a + element * b->es;
}

size_t buf_num_used_elements(Buf b)
{
    return b->i;
}
