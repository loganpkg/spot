<[/*
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

/* Buffer module */

#include "toucanlib.h"

#define READ_BLOCK_SIZE BUFSIZ

]>divert(1)dnl
<[#ifndef BUF_STRUCTS_H
#define BUF_STRUCTS_H

#include <stddef.h>

]>divert(0)dnl
define(common_funcs,
<[divert(1)dnl
struct $1buf {
    $2 *a;                      /* Memory */
    size_t i;                   /* Write index */
    size_t n;                   /* Allocated number of elements */
};

divert(0)dnl
struct $1buf *init_$1buf(size_t n)
{
    struct $1buf *b;

    if ((b = malloc(sizeof(struct $1buf))) == NULL) {
        fprintf(stderr, "%s:%d: Error\n", __FILE__, __LINE__);
        return NULL;
    }

    if (mof(n, sizeof($2), SIZE_MAX)) {
        fprintf(stderr, "%s:%d: Error\n", __FILE__, __LINE__);
        return NULL;
    }

    if ((b->a = malloc(n * sizeof($2))) == NULL) {
        fprintf(stderr, "%s:%d: Error\n", __FILE__, __LINE__);
        return NULL;
    }

    b->i = 0;
    b->n = n;
    return b;
}

void free_$1buf(struct $1buf *b)
{
    if (b != NULL) {
        free(b->a);
        free(b);
    }
}

static int grow_$1buf(struct $1buf *b, size_t will_use)
{
    $2 *t;
    size_t new_n;

    if (aof(b->n, will_use, SIZE_MAX)) {
        fprintf(stderr, "%s:%d: Error\n", __FILE__, __LINE__);
        return ERR;
    }

    new_n = b->n + will_use;

    if (mof(new_n, 2, SIZE_MAX)) {
        fprintf(stderr, "%s:%d: Error\n", __FILE__, __LINE__);
        return ERR;
    }

    new_n *= 2;

    if (mof(new_n, sizeof($2), SIZE_MAX)) {
        fprintf(stderr, "%s:%d: Error\n", __FILE__, __LINE__);
        return ERR;
    }

    if ((t = realloc(b->a, new_n * sizeof($2))) == NULL) {
        fprintf(stderr, "%s:%d: Error\n", __FILE__, __LINE__);
        return ERR;
    }

    b->a = t;
    b->n = new_n;
    return 0;
}

int add_$1(struct $1buf *b, $2 x)
{
    if (b->i == b->n && grow_$1buf(b, 1)) {
        fprintf(stderr, "%s:%d: Error\n", __FILE__, __LINE__);
        return ERR;
    }

    *(b->a + b->i) = x;
    ++b->i;
    return 0;
}
]>)
common_funcs(i, char)dnl
common_funcs(o, char)dnl
common_funcs(l, long)dnl
common_funcs(s, size_t)dnl
common_funcs(p, void *)dnl
divert(1)dnl
<[#endif
]>divert(0)dnl
writediv(1, buf_structs.h)dnl

<[int unget_ch(struct ibuf *b, char ch)
{
    return add_i(b, ch);
}

int put_ch(struct obuf *b, char ch)
{
    return add_o(b, ch);
}

int unget_str(struct ibuf *b, const char *str)
{
    size_t len, j;
    char *p;

    len = strlen(str);

    if (!len)
        return 0;

    if (len > b->n - b->i && grow_ibuf(b, len)) {
        fprintf(stderr, "%s:%d: Error\n", __FILE__, __LINE__);
        return ERR;
    }

    p = b->a + b->i + len - 1;
    j = len;
    while (j) {
        *p = *str;
        --p;
        ++str;
        --j;
    }
    b->i += len;
    return 0;
}

int unget_file(struct ibuf *b, const char *fn)
{
    int ret = ERR;
    FILE *fp = NULL;
    size_t fs, j;
    char *p;
    int x;

    if (fn == NULL || *fn == '\0') {
        fprintf(stderr, "%s:%d: Error\n", __FILE__, __LINE__);
        return ERR;
    }

    if ((fp = fopen(fn, "rb")) == NULL) {
        ret = ERR;
        fprintf(stderr, "%s:%d: Error\n", __FILE__, __LINE__);
        goto clean_up;
    }

    if (get_file_size(fn, &fs)) {
        ret = ERR;
        fprintf(stderr, "%s:%d: Error\n", __FILE__, __LINE__);
        goto clean_up;
    }

    if (!fs) {
        ret = ERR;
        fprintf(stderr, "%s:%d: Error\n", __FILE__, __LINE__);
        goto done;
    }

    if (fs > b->n - b->i && grow_ibuf(b, fs)) {
        ret = ERR;
        fprintf(stderr, "%s:%d: Error\n", __FILE__, __LINE__);
        goto clean_up;
    }

    p = b->a + b->i + fs - 1;
    j = fs;
    while (j) {
        if ((x = getc(fp)) == EOF) {
            ret = ERR;
            fprintf(stderr, "%s:%d: Error\n", __FILE__, __LINE__);
            goto clean_up;
        }

        *p = x;
        --p;
        --j;
    }
    if (ferror(fp)) {
        ret = ERR;
        fprintf(stderr, "%s:%d: Error\n", __FILE__, __LINE__);
        goto clean_up;
    }

    b->i += fs;

  done:
    ret = 0;
  clean_up:
    if (fp != NULL && fclose(fp))
        ret = ERR;

    return ret;
}

int get_ch(struct ibuf *input, char *ch, int read_stdin)
{
    int x;

    if (input->i) {
        --input->i;
        *ch = *(input->a + input->i);
        return 0;
    }
    if (!read_stdin)
        return EOF;

    if ((x = getchar()) == EOF) {
        if (feof(stdin) && !ferror(stdin))
            return EOF;
        else {
            fprintf(stderr, "%s:%d: Error\n", __FILE__, __LINE__);
            return ERR;
        }
    }
    *ch = x;
    return 0;
}

int eat_str_if_match(struct ibuf *input, const char *str, int read_stdin)
{
    /*
     * Checks for str at the start of input and eats it if there is a match.
     * Cannot check the buffer directly as stdin may not have been read yet.
     * Returns MATCH, NO_MATCH, or ERR.
     * (EOF is considered as NO_MATCH).
     */
    int r;
    char x, ch;
    size_t i;

    i = 0;
    while (1) {
        x = *(str + i);
        if (x == '\0')
            break;

        r = get_ch(input, &ch, read_stdin);
        if (r == ERR) {
            fprintf(stderr, "%s:%d: Error\n", __FILE__, __LINE__);
            return ERR;
        } else if (r == EOF)
            goto no_match;

        if (x != ch) {
            if (unget_ch(input, ch)) {
                fprintf(stderr, "%s:%d: Error\n", __FILE__, __LINE__);
                return ERR;
            }

            goto no_match;
        }

        ++i;
    }

    return MATCH;

  no_match:
    /* Return the read characters */
    while (i) {
        if (unget_ch(input, *(str + i - 1))) {
            fprintf(stderr, "%s:%d: Error\n", __FILE__, __LINE__);
            return ERR;
        }

        --i;
    }

    return NO_MATCH;
}

int get_word(struct ibuf *input, struct obuf *token, int read_stdin)
{
    int r;
    char ch, type;

    do {
        if ((r = get_ch(input, &ch, read_stdin)) != 0)
            return r;
    } while (ch == '\0' || ch == '\r'); /* Discard these chars */

    token->i = 0;

    if (put_ch(token, ch)) {
        fprintf(stderr, "%s:%d: Error\n", __FILE__, __LINE__);
        return ERR;
    }

    if (isdigit(ch))
        type = 'd';             /* Decimal number */
    else if (isalpha(ch) || ch == '_')  /* First char cannot be a digit */
        type = 'w';             /* Word */
    else
        goto end;               /* Send a single char */

    while (1) {
        do {
            r = get_ch(input, &ch, read_stdin);
            if (r == ERR) {
                fprintf(stderr, "%s:%d: Error\n", __FILE__, __LINE__);
                return ERR;
            } else if (r == EOF)        /* Ignore, as not the first char */
                goto end;
        } while (ch == '\0' || ch == '\r');

        if ((type == 'd' && isdigit(ch))
            || (type == 'w' && (isalnum(ch) || ch == '_'))) {
            /* More of the same type. Words can include digits here. */
            if (put_ch(token, ch)) {
                fprintf(stderr, "%s:%d: Error\n", __FILE__, __LINE__);
                return ERR;
            }
        } else {
            if (unget_ch(input, ch)) {
                fprintf(stderr, "%s:%d: Error\n", __FILE__, __LINE__);
                return ERR;
            }

            goto end;
        }
    }

  end:
    if (put_ch(token, '\0')) {  /* Terminate string */
        fprintf(stderr, "%s:%d: Error\n", __FILE__, __LINE__);
        return ERR;
    }

    return 0;
}

int eat_whitespace(struct ibuf *input, int read_stdin)
{
    int r;
    char ch;

    while (1) {
        r = get_ch(input, &ch, read_stdin);
        if (r == ERR) {
            fprintf(stderr, "%s:%d: Error\n", __FILE__, __LINE__);
            return ERR;
        } else if (r == EOF)
            break;

        if (!(isspace(ch) || ch == '\0')) {
            if (unget_ch(input, ch)) {
                fprintf(stderr, "%s:%d: Error\n", __FILE__, __LINE__);
                return ERR;
            }

            break;
        }
    }
    return 0;
}

int delete_to_nl(struct ibuf *input, int read_stdin)
{
    /* Delete to (and including) the next newline character */
    int r;
    char ch;

    while (1) {
        r = get_ch(input, &ch, read_stdin);
        if (r == ERR) {
            fprintf(stderr, "%s:%d: Error\n", __FILE__, __LINE__);
            return ERR;
        } else if (r == EOF)
            break;

        if (ch == '\n')
            break;
    }
    return 0;
}

int put_str(struct obuf *b, const char *str)
{
    size_t i_backup = b->i;
    char ch;

    while ((ch = *str++) != '\0') {
        if (b->i == b->n && grow_obuf(b, 1)) {
            b->i = i_backup;    /* Restore */
            fprintf(stderr, "%s:%d: Error\n", __FILE__, __LINE__);
            return ERR;
        }

        *(b->a + b->i++) = ch;
    }

    return 0;
}

int put_mem(struct obuf *b, const char *mem, size_t mem_len)
{
    if (mem_len > b->n - b->i && grow_obuf(b, mem_len)) {
        fprintf(stderr, "%s:%d: Error\n", __FILE__, __LINE__);
        return ERR;
    }

    memcpy(b->a + b->i, mem, mem_len);
    b->i += mem_len;
    return 0;
}

int put_obuf(struct obuf *b, struct obuf *t)
{
    /* Empties t onto the end of b */
    if (put_mem(b, t->a, t->i)) {
        fprintf(stderr, "%s:%d: Error\n", __FILE__, __LINE__);
        return ERR;
    }

    t->i = 0;
    return 0;
}

int put_file(struct obuf *b, const char *fn)
{
    int ret = ERR;
    FILE *fp = NULL;
    size_t fs;

    if (fn == NULL || *fn == '\0') {
        fprintf(stderr, "%s:%d: Error\n", __FILE__, __LINE__);
        return ERR;
    }

    if ((fp = fopen(fn, "rb")) == NULL) {
        ret = ERR;
        fprintf(stderr, "%s:%d: Error\n", __FILE__, __LINE__);
        goto clean_up;
    }

    if (get_file_size(fn, &fs)) {
        ret = ERR;
        fprintf(stderr, "%s:%d: Error\n", __FILE__, __LINE__);
        goto clean_up;
    }

    if (!fs) {
        ret = ERR;
        fprintf(stderr, "%s:%d: Error\n", __FILE__, __LINE__);
        goto done;
    }

    if (fs > b->n - b->i && grow_obuf(b, fs)) {
        ret = ERR;
        fprintf(stderr, "%s:%d: Error\n", __FILE__, __LINE__);
        goto clean_up;
    }

    if (fread(b->a + b->i, 1, fs, fp) != fs) {
        ret = ERR;
        fprintf(stderr, "%s:%d: Error\n", __FILE__, __LINE__);
        goto clean_up;
    }

    b->i += fs;

  done:
    ret = 0;
  clean_up:
    if (fp != NULL)
        if (fclose(fp))
            ret = ERR;

    return ret;
}

int put_stream(struct obuf *b, FILE * fp)
{
    size_t i_backup, rs;

    if (fp == NULL) {
        fprintf(stderr, "%s:%d: Error\n", __FILE__, __LINE__);
        return ERR;
    }

    i_backup = b->i;

    while (1) {
        if (READ_BLOCK_SIZE > b->n - b->i && grow_obuf(b, READ_BLOCK_SIZE)) {
            fprintf(stderr, "%s:%d: Error\n", __FILE__, __LINE__);
            goto error;
        }

        rs = fread(b->a + b->i, 1, READ_BLOCK_SIZE, fp);
        b->i += rs;

        if (rs != READ_BLOCK_SIZE) {
            if (feof(fp) && !ferror(fp)) {
                break;
            } else {
                fprintf(stderr, "%s:%d: Error\n", __FILE__, __LINE__);
                goto error;
            }
        }
    }
    return 0;

  error:
    b->i = i_backup;
    return ERR;
}

int write_obuf(struct obuf *b, const char *fn)
{
    /* Empties b to file fn */
    FILE *fp;

    if (fn == NULL || *fn == '\0') {
        fprintf(stderr, "%s:%d: Error\n", __FILE__, __LINE__);
        return ERR;
    }

    if ((fp = fopen_w(fn)) == NULL) {
        fprintf(stderr, "%s:%d: Error\n", __FILE__, __LINE__);
        return ERR;
    }

    if (fwrite(b->a, 1, b->i, fp) != b->i) {
        fclose(fp);
        fprintf(stderr, "%s:%d: Error\n", __FILE__, __LINE__);
        return ERR;
    }
    if (fclose(fp)) {
        fprintf(stderr, "%s:%d: Error\n", __FILE__, __LINE__);
        return ERR;
    }

    b->i = 0;
    return 0;
}

int flush_obuf(struct obuf *b)
{
    if (!b->i)
        return 0;

    if (fwrite(b->a, 1, b->i, stdout) != b->i) {
        fprintf(stderr, "%s:%d: Error\n", __FILE__, __LINE__);
        return ERR;
    }

    if (fflush(stdout)) {
        fprintf(stderr, "%s:%d: Error\n", __FILE__, __LINE__);
        return ERR;
    }

    b->i = 0;
    return 0;
}

char *obuf_to_str(struct obuf **b)
{
    char *str;
    if (put_ch(*b, '\0')) {
        fprintf(stderr, "%s:%d: Error\n", __FILE__, __LINE__);
        return NULL;
    }

    /* Success */
    str = (*b)->a;
    free(*b);
    *b = NULL;
    return str;
}]>
