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

#include "debug.h"
#include "doubly_linked_list.h"

int dll_add_node(Dlln *p, void *data)
{
    Dlln n = NULL;

    if (p == NULL)
        debug(return 1);

    if ((n = calloc(1, sizeof(struct dll_node))) == NULL)
        debug(return 1);

    n->data = data;

    if (*p != NULL) {
        /* Link in on the left. */
        n->prev = (*p)->prev;
        n->next = *p;
        (*p)->prev = n;
    } else {
        n->prev = NULL;
        n->next = NULL;
    }

    /* Update pointer. */
    *p = n;
    return 0;
}

int free_dll_node(Dlln *p, Free_data_func fdf)
{
    Dlln t = NULL;
    int r = 0;

    if (p == NULL)
        debug(return 1);

    if (*p == NULL)
        return 0;

    /* Link around. */
    if ((*p)->prev != NULL) {
        t = (*p)->prev;
        (*p)->prev->next = (*p)->next;
    }

    if ((*p)->next != NULL) {
        if (t == NULL)
            t = (*p)->next;

        (*p)->next->prev = (*p)->prev;
    }

    r = (*fdf)((*p)->data);

    free(*p);
    *p = t;
    return r;
}

int free_dll(Dlln *p, Free_data_func fdf)
{
    int r = 0;
    if (p == NULL)
        debug(return 1);

    while (*p != NULL)
        if (free_dll_node(p, fdf))
            r = 1;

    return r;
}
