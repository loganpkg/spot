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

#include <debug.h>
#include <doubly_linked_list.h>

void print_this_and_next_data(Dlln n)
{
    printf("n->data: %s\n", n != NULL ? (char *) n->data : "NULL");
    printf("n->next->data: %s\n\n",
        n != NULL && n->next != NULL ? (char *) n->next->data : "NULL");
}

int add_node_with_str(Dlln *n, char *str)
{
    size_t len;
    char *t;

    if (str == NULL)
        debug(return 1);

    len = strlen(str);

    if ((t = calloc(len + 1, sizeof(char))) == NULL)
        debug(return 1);

    memmove(t, str, len + 1);

    if (dll_add_node(n, t)) {
        free(t);
        debug(return 1);
    }

    return 0;
}

int custom_free(void *data)
{
    free(data);
    return 0;
}

int main(void)
{
    Dlln n = NULL;

    print_this_and_next_data(n);

    if (add_node_with_str(&n, "hello"))
        debug(goto error);

    print_this_and_next_data(n);

    if (add_node_with_str(&n, "world"))
        debug(goto error);

    print_this_and_next_data(n);

    if (add_node_with_str(&n, "elephant"))
        debug(goto error);

    print_this_and_next_data(n);

    if (add_node_with_str(&n, "wow"))
        debug(goto error);

    print_this_and_next_data(n);

    if (free_dll_node(&n, &custom_free))
        debug(goto error);

    print_this_and_next_data(n);

    if (add_node_with_str(&n, "goat"))
        debug(goto error);

    print_this_and_next_data(n);

    return free_dll(&n, &custom_free);

error:
    free_dll(&n, &custom_free);
    debug(return 1);
}
