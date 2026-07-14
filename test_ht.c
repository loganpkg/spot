/*
 * Copyright (c) 2026 Logan Ryan McLintock. All rights reserved.
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

/* Test Hash table. */

#include "hash_table.h"
#include <string.h>

int main(void)
{
    Hash_table ht = NULL;
    char *k, *v;

    if ((ht = ht_init(2, 2)) == NULL)
        return 1;

    ht_print(ht);

    k = "cool";
    v = "wow";

    if (ht_insert(ht, k, strlen(k), v, strlen(v)))
        goto error;

    if (ht_insert(ht, k, strlen(k), v, strlen(v)) != DUPLICATE_KEY)
        goto error;

    k = "world";

    if (ht_insert(ht, k, strlen(k), v, strlen(v)))
        goto error;

    ht_print(ht);

    k = "big";
    v = "elephant";
    if (ht_insert(ht, k, strlen(k), v, strlen(v)))
        goto error;

    ht_print(ht);

    ht_free(ht);
    return 0;

error:
    ht_free(ht);
    return 1;
}
