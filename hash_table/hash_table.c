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

/* Hash table. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hash_table.h"
#include "xxh64.h"

#ifdef _WIN32
#define lu "zu"
#define lx "llx"
#else
#define lu "lu"
#define lx "lx"
#endif

#define PROBE_LEN_THRESHOLD 40

/* Entry states. */
#define FREE    0
#define DELETED 1
#define USED    2

struct entry {
    int state;
    uint64_t h; /* Hash value before modulo. */
    size_t ki;  /* Key index into storage. */
    size_t ks;  /* Key size. */
    size_t vi;  /* Value index into storage. */
    size_t vs;  /* Value size. */
};

struct hash_table {
    struct entry *t;   /* Table. */
    size_t n;          /* Number of elements in table. */
    size_t c;          /* Count of USED and DELETED entries. */
    size_t pl;         /* Probe length (global maximum encountered). */
    unsigned char *sa; /* Storage for keys and values. */
    size_t ss;         /* Storage size. */
    size_t si;         /* Storage index of the next write. */
};

#define add_overflow(a, b)  ((b) > SIZE_MAX - (a))
#define mult_overflow(a, b) ((a) && (b) > SIZE_MAX / (a))

void ht_free(Hash_table ht)
{
    if (ht != NULL) {
        if (ht->t != NULL)
            free(ht->t);

        if (ht->sa != NULL)
            free(ht->sa);

        free(ht);
    }
}

Hash_table ht_init(size_t num_elements, size_t storage_size)
{
    Hash_table ht = NULL;

    if ((ht = calloc(1, sizeof(struct hash_table))) == NULL)
        goto error;

    ht->t = NULL;
    ht->sa = NULL;

    if (mult_overflow(num_elements, sizeof(struct entry)))
        goto error;

    if ((ht->t = calloc(num_elements, sizeof(struct entry))) == NULL)
        goto error;

    ht->n = num_elements;

    if ((ht->sa = calloc(storage_size, 1)) == NULL)
        goto error;

    ht->ss = storage_size;

    return ht;

error:
    ht_free(ht);
    return NULL;
}

static int store(Hash_table ht, const void *mem, size_t mem_size)
{
    unsigned char *new_sa;
    size_t new_ss;

    if (mem_size > ht->ss - ht->si) {
        /* Need to grow. */
        if (add_overflow(ht->si, mem_size))
            return 1;

        new_ss = ht->si + mem_size;

        if (mult_overflow(new_ss, 2))
            return 1;

        new_ss *= 2;

        if ((new_sa = realloc(ht->sa, new_ss)) == NULL)
            return 1;

        ht->sa = new_sa;
        ht->ss = new_ss;
    }

    memcpy(ht->sa + ht->si, mem, mem_size);
    ht->si += mem_size;
    return 0;
}

static int ht_grow(Hash_table ht)
{
    struct entry *new_t = NULL;
    size_t new_n, new_c, pl, new_pl, i, j;

    if (ht->pl <= PROBE_LEN_THRESHOLD && ht->c <= ht->n / 2)
        return 0; /* No need to grow. */

    if (mult_overflow(ht->n, 2))
        return 1;

    new_n = ht->n * 2;

    if (mult_overflow(new_n, sizeof(struct entry)))
        return 1;

    if ((new_t = calloc(new_n, sizeof(struct entry))) == NULL)
        return 1;

    /* Redistribute. */
    for (i = 0, j = 0, new_c = 0, pl = 0, new_pl = 0; i < ht->n; pl = 0, ++i)
        if (ht->t[i].state == USED) {
            j = ht->t[i].h % new_n;
            while (new_t[j].state == USED) {
                if (++j == new_n)
                    j = 0;

                ++pl;
            }
            /* Copy table element. */
            ++new_c;
            new_t[j] = ht->t[i];
            if (pl > new_pl)
                new_pl = pl;
        }

    /* Update. */
    free(ht->t);
    ht->t = new_t;
    ht->n = new_n;
    ht->c = new_c;
    ht->pl = new_pl;

    return 0;
}

int ht_insert(Hash_table ht, const void *key, size_t key_size,
    const void *value, size_t value_size)
{
    uint64_t hash;
    size_t i, pl = 0;

    if (ht_grow(ht))
        return 1;

    hash = xxh64(key, key_size);

    i = hash % ht->n;

    while (ht->t[i].state == USED) {
        /* Check for duplicate key. */
        if (hash == ht->t[i].h && key_size == ht->t[i].ks
            && !memcmp(key, ht->sa + ht->t[i].ki, key_size))
            return DUPLICATE_KEY;

        if (++i == ht->n)
            i = 0;

        ++pl;
    }

    /* Copy data. */
    ht->t[i].h = hash;

    ht->t[i].ki = ht->si;
    if (store(ht, key, key_size))
        return 1;

    ht->t[i].ks = key_size;

    ht->t[i].vi = ht->si;
    if (store(ht, value, value_size))
        return 1;

    ht->t[i].vs = value_size;

    ht->t[i].state = USED;
    ++ht->c;

    if (pl > ht->pl)
        ht->pl = pl; /* Update global longest probe length. */

    return 0;
}

void ht_print(Hash_table ht)
{
    size_t i;

    printf("============ HASH TABLE ============\n");
    printf("Number of elements: %" lu "\n", ht->n);
    printf("Count of USED and DELETED: %" lu "\n", ht->c);
    printf("Probe length: %" lu "\n", ht->pl);
    printf("Storage size: %" lu "\n", ht->ss);
    printf("Storage index: %" lu "\n", ht->si);
    for (i = 0; i < ht->n; ++i) {
        printf("============\n");
        printf("Table index: %" lu "\n", i);
        printf("State: %s\n",
            ht->t[i].state == FREE
                ? "FREE"
                : (ht->t[i].state == DELETED ? "DELETED" : "USED"));
        printf("Hash: %016" lx "\n", ht->t[i].h);
        printf("Key: ");
        fwrite(ht->sa + ht->t[i].ki, 1, ht->t[i].ks, stdout);
        printf("\nKey size: %" lu "\n", ht->t[i].ks);
        printf("Value: ");
        fwrite(ht->sa + ht->t[i].vi, 1, ht->t[i].vs, stdout);
        printf("\nValue size: %" lu "\n", ht->t[i].vs);
    }
}
