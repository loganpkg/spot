/*
 * Copyright (c) 2023 Logan Ryan McLintock
 *
 * Permission to use, copy, modify, and/or distribute this software for any
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

/* Hash table module */

#include "toucanlib.h"


static struct entry *init_entry(void)
{
    struct entry *e;

    if ((e = calloc(1, sizeof(struct entry))) == NULL)
        mreturn(NULL);

    return e;
}

static void free_entry(struct entry *e)
{
    /* Frees an entry and its connected history */
    struct entry *e_hist;
    while (e != NULL) {
        e_hist = e->hist;
        free(e->name);
        free(e->def);
        free(e);
        e = e_hist;
    }
}

struct ht *init_ht(size_t num_buckets)
{
    struct ht *ht;

    if ((ht = calloc(1, sizeof(struct ht))) == NULL)
        mreturn(NULL);

    if (mof(num_buckets, sizeof(struct entry *), SIZE_MAX))
        mreturn(NULL);

    if ((ht->b = calloc(num_buckets, sizeof(struct entry *))) == NULL)
        mreturn(NULL);

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

int delete_entry(struct ht *ht, const char *name, int pop_hist)
{
    size_t bucket;
    struct entry *e;

    e = lookup(ht, name);

    if (e == NULL)
        return ERROR;           /* Error as not found */

    bucket = hash_func(name, ht->n);

    if (pop_hist && e->hist != NULL) {
        /* Link in history, if present */
        if (e->prev != NULL) {
            e->prev->next = e->hist;
            e->hist->prev = e->prev;
        } else {
            /* At head of collision chain */
            ht->b[bucket] = e->hist;
        }

        if (e->next != NULL) {
            e->hist->next = e->next;
            e->next->prev = e->hist;
        }

        /* Isolate history */
        e->hist = NULL;
    } else {
        /* Link around. History will be deleted. */
        if (e->prev != NULL) {
            e->prev->next = e->next;
            if (e->next != NULL)
                e->next->prev = e->prev;
        } else {
            /* At head of collision chain */
            ht->b[bucket] = e->next;
            if (e->next != NULL)
                e->next->prev = NULL;
        }
    }

    free_entry(e);              /* Will free history too if not isolated */
    return 0;
}

int upsert(struct ht *ht, const char *name, const char *def, Fptr func_p,
           int push_hist)
{
    struct entry *e, *new_e = NULL;
    size_t bucket;
    char *name_copy, *def_copy = NULL;

    if ((name_copy = strdup(name)) == NULL)
        mreturn(ERROR);

    if (def != NULL && (def_copy = strdup(def)) == NULL) {
        free(name_copy);
        mreturn(ERROR);
    }

    e = lookup(ht, name);

    if (e == NULL || push_hist) {
        /* Make a new entry */
        if ((new_e = init_entry()) == NULL) {
            free(name_copy);
            free(def_copy);
            mreturn(ERROR);
        }
    }

    if (e == NULL) {
        /* New def */
        /* Link in at the head of the bucket collision chain */
        bucket = hash_func(name, ht->n);
        if (ht->b[bucket] != NULL) {
            new_e->next = ht->b[bucket];
            ht->b[bucket]->prev = new_e;
        }
        ht->b[bucket] = new_e;

        new_e->name = name_copy;
        new_e->def = def_copy;
        new_e->func_p = func_p;
    } else if (push_hist) {
        /*
         * To preserve prev and next links, link in the new node below hist
         * head. Copy the existing contents of hist head to the new node,
         * then update the contents of hist head with the new information.
         */
        new_e->hist = e->hist;
        e->hist = new_e;

        new_e->name = e->name;
        new_e->def = e->def;
        new_e->func_p = e->func_p;

        e->name = name_copy;
        e->def = def_copy;
        e->func_p = func_p;
    } else {
        /* Update. Links remain unchanged. */
        free(e->name);
        free(e->def);
        e->name = name_copy;
        e->def = def_copy;
        e->func_p = func_p;
    }

    return 0;
}
