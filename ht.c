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

/* Hash table module */

#include "toucanlib.h"


static struct entry *init_entry(void)
{
    struct entry *e;

    if ((e = malloc(sizeof(struct entry))) == NULL) {
        fprintf(stderr, "%s:%d: Error\n", __FILE__, __LINE__);
        return NULL;
    }

    e->name = NULL;
    e->def = NULL;
    e->func_p = NULL;
    return e;
}

static void free_entry(struct entry *e)
{
    if (e != NULL) {
        free(e->name);
        free(e->def);
        free(e);
    }
}

struct ht *init_ht(size_t num_buckets)
{
    struct ht *ht;
    size_t i;

    if ((ht = malloc(sizeof(struct ht))) == NULL) {
        fprintf(stderr, "%s:%d: Error\n", __FILE__, __LINE__);
        return NULL;
    }

    if (mof(num_buckets, sizeof(struct entry *), SIZE_MAX)) {
        fprintf(stderr, "%s:%d: Error\n", __FILE__, __LINE__);
        return NULL;
    }

    if ((ht->b = malloc(num_buckets * sizeof(struct entry *))) == NULL) {
        fprintf(stderr, "%s:%d: Error\n", __FILE__, __LINE__);
        return NULL;
    }

    for (i = 0; i < num_buckets; ++i)
        ht->b[i] = NULL;

    ht->n = num_buckets;

    return ht;
}

void free_ht(struct ht *ht)
{
    size_t i;
    struct entry *e, *e_next;

    if (ht != NULL) {
        if (ht->b != NULL) {
            for (i = 0; i < ht->n; ++i) {
                e = ht->b[i];
                while (e != NULL) {
                    e_next = e->next;
                    free_entry(e);
                    e = e_next;
                }
            }
            free(ht->b);
        }
        free(ht);
    }
}

static size_t hash_func(const char *str, size_t n)
{
    /* djb2 */
    unsigned char ch;
    size_t h = 5381;

    while ((ch = *str) != '\0') {
        h = h * 33 ^ ch;
        ++str;
    }
    return h % n;               /* Bucket index */
}

struct entry *lookup(struct ht *ht, const char *name)
{
    size_t bucket;
    struct entry *e;

    bucket = hash_func(name, ht->n);
    e = ht->b[bucket];

    while (e != NULL) {
        if (!strcmp(name, e->name))
            return e;           /* Match */

        e = e->next;
    }
    return NULL_OK;             /* Not found */
}

int delete_entry(struct ht *ht, const char *name)
{
    size_t bucket;
    struct entry *e, *e_prev;

    bucket = hash_func(name, ht->n);
    e = ht->b[bucket];
    e_prev = NULL;
    while (e != NULL) {
        if (!strcmp(name, e->name)) {
            /* Link around */
            if (e_prev != NULL)
                e_prev->next = e->next;
            else
                ht->b[bucket] = e->next;        /* At head of list */

            free_entry(e);
            return 0;
        }
        e_prev = e;
        e = e->next;
    }

    /* Error as not found */
    fprintf(stderr, "%s:%d: Error\n", __FILE__, __LINE__);
    return ERR;
}

int upsert(struct ht *ht, const char *name, const char *def, Fptr func_p)
{
    struct entry *e;
    size_t bucket;
    char *name_copy, *def_copy = NULL;

    e = lookup(ht, name);

    if (e == NULL) {
        /* Make a new entry */
        if ((e = init_entry()) == NULL) {
            fprintf(stderr, "%s:%d: Error\n", __FILE__, __LINE__);
            return ERR;
        }

        if ((e->name = strdup(name)) == NULL) {
            free_entry(e);
            fprintf(stderr, "%s:%d: Error\n", __FILE__, __LINE__);
            return ERR;
        }

        if (def != NULL && (e->def = strdup(def)) == NULL) {
            free_entry(e);
            fprintf(stderr, "%s:%d: Error\n", __FILE__, __LINE__);
            return ERR;
        }

        e->func_p = func_p;

        /* Link in at the head of the bucket collision list */
        bucket = hash_func(name, ht->n);
        e->next = ht->b[bucket];
        ht->b[bucket] = e;

    } else {
        /* Update the existing entry */
        if ((name_copy = strdup(name)) == NULL) {
            fprintf(stderr, "%s:%d: Error\n", __FILE__, __LINE__);
            return ERR;
        }

        if (def != NULL && (def_copy = strdup(def)) == NULL) {
            free(name_copy);
            fprintf(stderr, "%s:%d: Error\n", __FILE__, __LINE__);
            return ERR;
        }

        free(e->name);
        free(e->def);
        e->name = name_copy;
        e->def = def_copy;
    }
    return 0;
}
