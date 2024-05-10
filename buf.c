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

/* Buffer module */

#include "toucanlib.h"


#define READ_BLOCK_SIZE BUFSIZ


struct ibuf *init_ibuf(size_t s)
{
    struct ibuf *b;

    if ((b = malloc(sizeof(struct ibuf))) == NULL) {
        fprintf(stderr, "%s:%d: Error\n", __FILE__, __LINE__);
        return NULL;
    }

    if ((b->a = malloc(s)) == NULL) {
        fprintf(stderr, "%s:%d: Error\n", __FILE__, __LINE__);
        return NULL;
    }

    b->i = 0;
    b->s = s;
    return b;
}

struct obuf *init_obuf(size_t s)
{
    return (struct obuf *) init_ibuf(s);
}

void free_ibuf(struct ibuf *b)
{
    if (b != NULL) {
        free(b->a);
        free(b);
    }
}

void free_obuf(struct obuf *b)
{
    free_ibuf((struct ibuf *) b);
}

static int grow_ibuf(struct ibuf *b, size_t will_use)
{
    char *t;
    size_t new_s;

    if (aof(b->s, will_use, SIZE_MAX)) {
        fprintf(stderr, "%s:%d: Error\n", __FILE__, __LINE__);
        return ERR;
    }

    new_s = b->s + will_use;

    if (mof(new_s, 2, SIZE_MAX)) {
        fprintf(stderr, "%s:%d: Error\n", __FILE__, __LINE__);
        return ERR;
    }

    new_s *= 2;

    if ((t = realloc(b->a, new_s)) == NULL) {
        fprintf(stderr, "%s:%d: Error\n", __FILE__, __LINE__);
        return ERR;
    }

    b->a = t;
    b->s = new_s;
    return 0;
}

static int grow_obuf(struct obuf *b, size_t will_use)
{
    return grow_ibuf((struct ibuf *) b, will_use);
}

int unget_ch(struct ibuf *b, char ch)
{
    if (b->i == b->s && grow_ibuf(b, 1)) {
        fprintf(stderr, "%s:%d: Error\n", __FILE__, __LINE__);
        return ERR;
    }

    *(b->a + b->i) = ch;
    ++b->i;
    return 0;
}

int put_ch(struct obuf *b, char ch)
{
    return unget_ch((struct ibuf *) b, ch);
}

int unget_str(struct ibuf *b, const char *str)
{
    size_t len, j;
    char *p;

    len = strlen(str);

    if (!len)
        return 0;

    if (len > b->s - b->i && grow_ibuf(b, len)) {
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

    if (fs > b->s - b->i && grow_ibuf(b, fs)) {
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
        if (b->i == b->s && grow_obuf(b, 1)) {
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
    if (mem_len > b->s - b->i && grow_obuf(b, mem_len)) {
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

    if (fs > b->s - b->i && grow_obuf(b, fs)) {
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
        if (READ_BLOCK_SIZE > b->s - b->i && grow_obuf(b, READ_BLOCK_SIZE)) {
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
}

struct lbuf *init_lbuf(size_t n)
{
    struct lbuf *b;

    if ((b = malloc(sizeof(struct lbuf))) == NULL) {
        fprintf(stderr, "%s:%d: Error\n", __FILE__, __LINE__);
        return NULL;
    }

    if (mof(n, sizeof(long), SIZE_MAX)) {
        fprintf(stderr, "%s:%d: Error\n", __FILE__, __LINE__);
        return NULL;
    }

    if ((b->a = malloc(n * sizeof(long))) == NULL) {
        fprintf(stderr, "%s:%d: Error\n", __FILE__, __LINE__);
        return NULL;
    }

    b->i = 0;
    b->n = n;
    return b;
}

void free_lbuf(struct lbuf *b)
{
    if (b != NULL) {
        free(b->a);
        free(b);
    }
}

static int grow_lbuf(struct lbuf *b, size_t will_use)
{
    long *t;
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

    if (mof(new_n, sizeof(long), SIZE_MAX)) {
        fprintf(stderr, "%s:%d: Error\n", __FILE__, __LINE__);
        return ERR;
    }

    if ((t = realloc(b->a, new_n * sizeof(long))) == NULL) {
        fprintf(stderr, "%s:%d: Error\n", __FILE__, __LINE__);
        return ERR;
    }

    b->a = t;
    b->n = new_n;
    return 0;
}


int add_l(struct lbuf *b, long x)
{
    if (b->i == b->n && grow_lbuf(b, 1)) {
        fprintf(stderr, "%s:%d: Error\n", __FILE__, __LINE__);
        return ERR;
    }

    *(b->a + b->i) = x;
    ++b->i;
    return 0;
}


struct pbuf *init_pbuf(size_t n)
{
    struct pbuf *b;

    if ((b = malloc(sizeof(struct pbuf))) == NULL) {
        fprintf(stderr, "%s:%d: Error\n", __FILE__, __LINE__);
        return NULL;
    }

    if (mof(n, sizeof(void *), SIZE_MAX)) {
        fprintf(stderr, "%s:%d: Error\n", __FILE__, __LINE__);
        return NULL;
    }

    if ((b->a = malloc(n * sizeof(void *))) == NULL) {
        fprintf(stderr, "%s:%d: Error\n", __FILE__, __LINE__);
        return NULL;
    }

    b->i = 0;
    b->n = n;
    return b;
}

void free_pbuf(struct pbuf *b)
{
    if (b != NULL) {
        free(b->a);
        free(b);
    }
}

static int grow_pbuf(struct pbuf *b, size_t will_use)
{
    void **t;
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

    if (mof(new_n, sizeof(void *), SIZE_MAX)) {
        fprintf(stderr, "%s:%d: Error\n", __FILE__, __LINE__);
        return ERR;
    }

    if ((t = realloc(b->a, new_n * sizeof(void *))) == NULL) {
        fprintf(stderr, "%s:%d: Error\n", __FILE__, __LINE__);
        return ERR;
    }

    b->a = t;
    b->n = new_n;
    return 0;
}

int add_p(struct pbuf *b, void *ptr)
{
    if (b->i == b->n && grow_pbuf(b, 1)) {
        fprintf(stderr, "%s:%d: Error\n", __FILE__, __LINE__);
        return ERR;
    }

    *(b->a + b->i) = ptr;
    ++b->i;
    return 0;
}
