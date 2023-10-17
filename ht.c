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

#ifdef __linux__
/* For: strdup */
#define _XOPEN_SOURCE 500
#endif

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ht.h"
#include "debug.h"
#include "num.h"


static struct entry *init_entry(void)
{
    struct entry *e;

    if ((e = malloc(sizeof(struct entry))) == NULL)
        mreturn(NULL);

    e->name = NULL;
    e->def = NULL;
    e->func_p = NULL;
    mreturn(e);
}

static void free_entry(struct entry *e)
{
    if (e != NULL) {
        free(e->name);
        free(e->def);
        free(e);
    }
}

struct entry **init_ht(void)
{
    struct entry **ht;
    size_t i;

    if (mof(NUM_BUCKETS, sizeof(struct entry *)))
        mreturn(NULL);

    if ((ht = malloc(NUM_BUCKETS * sizeof(struct entry *))) == NULL)
        mreturn(NULL);

    for (i = 0; i < NUM_BUCKETS; ++i)
        ht[i] = NULL;

    mreturn(ht);
}

void free_ht(struct entry **ht)
{
    size_t i;
    struct entry *e, *e_next;

    if (ht != NULL) {
        for (i = 0; i < NUM_BUCKETS; ++i) {
            e = ht[i];
            while (e != NULL) {
                e_next = e->next;
                free_entry(e);
                e = e_next;
            }
        }
        free(ht);
    }
}

static size_t hash_func(const char *str)
{
    /* djb2 */
    unsigned char ch;
    size_t h = 5381;

    while ((ch = *str) != '\0') {
        h = h * 33 ^ ch;
        ++str;
    }
    mreturn(h % NUM_BUCKETS);   /* Bucket index */
}

struct entry *lookup(struct entry **ht, const char *name)
{
    size_t bucket;
    struct entry *e;

    bucket = hash_func(name);
    e = ht[bucket];

    while (e != NULL) {
        if (!strcmp(name, e->name))
            mreturn(e);         /* Match */

        e = e->next;
    }
    mreturn(NULL_OK);           /* Not found */
}

int delete_entry(struct entry **ht, const char *name)
{
    size_t bucket;
    struct entry *e, *e_prev;

    bucket = hash_func(name);
    e = ht[bucket];
    e_prev = NULL;
    while (e != NULL) {
        if (!strcmp(name, e->name)) {
            /* Link around */
            if (e_prev != NULL)
                e_prev->next = e->next;
            else
                ht[bucket] = e->next;   /* At head of list */

            free_entry(e);
            mreturn(0);
        }
        e_prev = e;
        e = e->next;
    }

    mreturn(1);                 /* Not found */
}

int upsert(struct entry **ht, const char *name, const char *def,
           Fptr func_p)
{
    struct entry *e;
    size_t bucket;
    char *name_copy, *def_copy = NULL;

    e = lookup(ht, name);

    if (e == NULL) {
        /* Make a new entry */
        if ((e = init_entry()) == NULL)
            mreturn(1);

        if ((e->name = strdup(name)) == NULL) {
            free_entry(e);
            mreturn(1);
        }

        if (def != NULL && (e->def = strdup(def)) == NULL) {
            free_entry(e);
            mreturn(1);
        }

        e->func_p = func_p;

        /* Link in at the head of the bucket collision list */
        bucket = hash_func(name);
        e->next = ht[bucket];
        ht[bucket] = e;

    } else {
        /* Update the existing entry */
        if ((name_copy = strdup(name)) == NULL)
            mreturn(1);

        if (def != NULL && (def_copy = strdup(def)) == NULL) {
            free(name_copy);
            mreturn(1);
        }

        free(e->name);
        free(e->def);
        e->name = name_copy;
        e->def = def_copy;
    }
    mreturn(0);
}
