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

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "csv.h"

/* Configuration. Cannot be zero. */
#define INIT_BUF_ELEMENTS   1
#define INIT_INDEX_ELEMENTS 1

/* State machine states. */
#define START    0
#define QUOTED   1
#define END      2
#define UNQUOTED 3

#define mult_overflow(a, b) ((b) && (a) > SIZE_MAX / (b))

/* Do not free the structs themselves, only the "a" elemment. */
struct buf {
    char *a;
    size_t i;
    size_t n;
};

struct index {
    char **a;
    size_t i;
    size_t n; /* Number of elements not the size in bytes. */
};

static int init_buf(struct buf *b)
{
    if ((b->a = calloc(INIT_BUF_ELEMENTS, 1)) == NULL)
        debug(return 1);

    b->i = 0;
    b->n = INIT_BUF_ELEMENTS;
    return 0;
}

static int init_index(struct index *idx)
{
    if (mult_overflow(INIT_INDEX_ELEMENTS, sizeof(char *)))
        debug(return 1);

    if ((idx->a = calloc(INIT_INDEX_ELEMENTS, sizeof(char *))) == NULL)
        debug(return 1);

    idx->i = 0;
    idx->n = INIT_INDEX_ELEMENTS;
    return 0;
}

static int insert_buf(struct buf *b, char ch)
{
    char *new_a;
    size_t new_n;

    if (b->i == b->n) {
        /* Need to grow. */
        if (mult_overflow(b->n, 2))
            debug(return 1);

        new_n = b->n * 2;

        if ((new_a = realloc(b->a, new_n)) == NULL)
            debug(return 1);

        b->a = new_a;
        b->n = new_n;
    }

    *(b->a + b->i++) = ch;
    return 0;
}

static int insert_index(struct index *idx, char *val)
{
    char **new_a;
    size_t new_n;

    if (idx->i == idx->n) {
        /* Need to grow. */
        if (mult_overflow(idx->n, 2))
            debug(return 1);

        new_n = idx->n * 2;

        if (mult_overflow(new_n, sizeof(char *)))
            debug(return 1);

        if ((new_a = realloc(idx->a, new_n * sizeof(char *))) == NULL)
            debug(return 1);

        idx->a = new_a;
        idx->n = new_n;
    }

    *(idx->a + idx->i++) = val;
    return 0;
}

char **split_csv_line(const char *csv, size_t csv_size, char delim, char quote,
    int process_quotes, char line_sep, size_t *num_cols, size_t *line_size)
{
    /* Embedded '\0' chars are removed. */

    struct index idx = { 0 };
    struct buf col = { 0 };
    char *p, *p_stop, ch;
    int state; /* State machine state. */
    size_t k;

    if (init_index(&idx))
        debug(goto error);

    if (init_buf(&col))
        debug(goto error);

    state = START;

    p = csv;
    p_stop = csv + csv_size;

    while (p != p_stop) {
        ch = *p++;
        if (process_quotes && ch == quote && state == START) {
            state = QUOTED;
            /*
             * Truncate any space or tab characters before the start of the
             * quoted field.
             */
            col.i = 0;
        } else if (ch == quote && state == QUOTED) {
            /* Look forward for escaped quote. */
            if (p != p_stop && *p == quote) {
                /* Store. */
                if (quote != '\0' && insert_buf(&col, quote))
                    debug(goto error);

                ++p; /* Advance. */
            } else {
                state = END;
            }
        } else if (state != QUOTED && ch == line_sep) {
            break;
        } else if (state != QUOTED && ch == delim) {
            if (insert_buf(&col, '\0'))
                debug(goto error);

            /* Do afterwards so that realloc doesn't change the pointers. */
            if (insert_index(&idx, col.a)) {
                free(col.a);
                col.a = NULL;
                debug(goto error);
            }
            col.a = NULL;

            if (init_buf(&col))
                debug(goto error);

            state = START;
        } else {
            if (ch != ' ' && ch != '\t') {
                if (state == END) {
                    fprintf(stderr,
                        "CSV split: ERROR: Non-space or non-tab after closed "
                        "quoted field\n");
                    printf("ERROR: %s\n", p);
                    debug(goto error);
                }

                if (state == START)
                    state = UNQUOTED;
            }

            /* Trim space and tab characters after a closed quoted field. */
            if (ch != '\0' && !(state == END && (ch == ' ' || ch == '\t'))
                && insert_buf(&col, ch))
                debug(goto error);
        }
    }
    if (insert_buf(&col, '\0'))
        debug(goto error);

    /* Do afterwards so that realloc doesn't change the pointers. */
    if (insert_index(&idx, col.a)) {
        free(col.a);
        col.a = NULL;
        debug(goto error);
    }
    col.a = NULL;

    if (state == QUOTED) {
        fprintf(stderr, "CSV split: ERROR: Unclosed quoted field\n");
        debug(goto error);
    }

    *num_cols = idx.i;
    *line_size = p - csv;
    return idx.a;

error:

    if (col.a != NULL) {
        free(col.a);
        col.a = NULL;
    }

    if (idx.a != NULL) {
        for (k = 0; k < idx.i; ++k)
            if (idx.a[k] != NULL) {
                free(idx.a[k]);
                idx.a[k] = NULL;
            }

        free(idx.a);
        idx.a = NULL;
    }

    debug(return NULL);
}

/*
int detect_delim(const char *csv, size_t csv_size, char quote, int process_quotes, char line_sep, char *delim) {
{

}
*/
