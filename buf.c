/*
 * Copyright (c) 2023-2025 Logan Ryan McLintock. All rights reserved.
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

/* Buffer module */

#include "toucanlib.h"

#define READ_BLOCK_SIZE BUFSIZ
#define INIT_BUF_SIZE   512

/* ###################################################################### */

struct ibuf *init_ibuf(size_t n)
{
    struct ibuf *b = NULL;

    if ((b = calloc(1, sizeof(struct ibuf))) == NULL)
        mgoto(error);

    if ((b->a = calloc(n, 1)) == NULL)
        mgoto(error);

    b->i = 0;
    b->n = n;
    return b;

error:
    free_ibuf(b);
    return NULL;
}

int free_ibuf(struct ibuf *b)
{
    int ret = 0;
    struct ibuf *t;
    while (b != NULL) {
        t = b->next;

        if (b->nm != NULL)
            free(b->nm);

        if (b->fp != NULL && b->fp != stdin)
            if (fclose(b->fp))
                ret = 1; /* Continue */

        free(b->a);
        free(b);
        b = t;
    }

    return ret;
}

static int grow_ibuf(struct ibuf *b, size_t will_use)
{
    char *t;
    size_t new_n;

    if (will_use == 0)
        return 0;

    if (aof(b->n, will_use, SIZE_MAX))
        mreturn(1);

    new_n = b->n + will_use;

    if (mof(new_n, 2, SIZE_MAX))
        mreturn(1);

    new_n *= 2;

    if (new_n == 0)
        return 0;

    if ((t = realloc(b->a, new_n)) == NULL)
        mreturn(1);

    b->a = t;
    b->n = new_n;
    return 0;
}

int unget_ch(struct ibuf *b, char ch)
{
    if (b->i == b->n && grow_ibuf(b, 1))
        mreturn(1);

    *(b->a + b->i) = ch;
    ++b->i;
    return 0;
}

int unget_str(struct ibuf *b, const char *str)
{
    size_t len, j;
    char *p;

    len = strlen(str);

    if (!len)
        return 0;

    if (len > b->n - b->i && grow_ibuf(b, len))
        mreturn(1);

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

int unget_stream(struct ibuf **b, FILE *fp, const char *nm)
{
    /*
     * Creates a new struct head. *b can be NULL.
     * Upon error, fp is closed by caller.
     */
    struct ibuf *t = NULL;

    if ((t = init_ibuf(INIT_BUF_SIZE)) == NULL)
        mreturn(1);

    if ((t->nm = strdup(nm)) == NULL) {
        free_ibuf(t);
        mreturn(1);
    }

    /* Success */
    t->fp = fp;
    t->rn = 1;

    /* Link in front */
    t->next = *b;
    *b = t;

    return 0;
}

int unget_file(struct ibuf **b, const char *fn)
{
    FILE *fp = NULL;
    if ((fp = fopen(fn, "rb")) == NULL)
        mreturn(1);

    if (unget_stream(b, fp, fn)) {
        fclose(fp);
        mreturn(1);
    }

    return 0;
}

int append_stream(struct ibuf **b, FILE *fp, const char *nm)
{
    /* Links a new stream in at the tail of the list. *b can be NULL. */
    struct ibuf *t = NULL;
    struct ibuf *w = NULL;

    if ((t = init_ibuf(INIT_BUF_SIZE)) == NULL)
        mreturn(1);

    if ((t->nm = strdup(nm)) == NULL) {
        free_ibuf(t);
        mreturn(1);
    }

    /* Success */
    t->fp = fp;
    t->rn = 1;

    /* Link at end */
    if (*b != NULL) {
        w = *b;
        while (w->next != NULL) w = w->next;

        w->next = t;
    } else {
        *b = t;
    }

    return 0;
}

int append_file(struct ibuf **b, const char *fn)
{
    FILE *fp = NULL;
    if ((fp = fopen(fn, "rb")) == NULL)
        mreturn(1);

    if (append_stream(b, fp, fn)) {
        fclose(fp);
        mreturn(1);
    }

    return 0;
}

int get_ch(struct ibuf **input, char *ch)
{
    struct ibuf *t = NULL;
    int x;

top:
    if ((*input)->i) {
        --(*input)->i;
        *ch = *((*input)->a + (*input)->i);
        return 0;
    }

    if ((*input)->fp != NULL) {
        if ((x = getc((*input)->fp)) == EOF) {
            if (ferror((*input)->fp))
                mreturn(1);
            else if (feof((*input)->fp)) {
                if ((*input)->next != NULL) {
                    t = (*input)->next;
                    /* Isolate old head */
                    (*input)->next = NULL;
                    if (free_ibuf(*input))
                        mreturn(1);

                    /* Update head */
                    *input = t;
                    goto top;
                } else {
                    /*
                     * Need to close the stream, as some systems block
                     * waiting for input after the first EOF is read,
                     * instead of automatically repeating the EOF status
                     * on each subsequent read after the first EOF char.
                     */
                    if (fclose((*input)->fp))
                        mreturn(1);

                    (*input)->fp = NULL;
                    return EOF;
                }
            }
        } else {
            if ((*input)->incr_rn) {
                ++(*input)->rn;
                (*input)->incr_rn = 0;
            }

            if (x == '\n')
                (*input)->incr_rn = 1;

            *ch = x;
            return 0;
        }
    }

    return EOF;
}

int eat_whitespace(struct ibuf **input)
{
    int r;
    char ch;

    while (1) {
        r = get_ch(input, &ch);
        if (r == 1)
            mreturn(1);
        else if (r == EOF)
            break;

        if (!(isspace(ch) || ch == '\0')) {
            if (unget_ch(*input, ch))
                mreturn(1);

            break;
        }
    }
    return 0;
}

int delete_to_nl(struct ibuf **input)
{
    /* Delete to (and including) the next newline character */
    int r;
    char ch;

    while (1) {
        r = get_ch(input, &ch);
        if (r == 1)
            mreturn(1);
        else if (r == EOF)
            break;

        if (ch == '\n')
            break;
    }
    return 0;
}

int eat_str_if_match(struct ibuf **input, const char *str)
{
    /*
     * Checks for str at the start of input and eats it if there is a match.
     * Cannot check the buffer directly as stdin may not have been read yet.
     * Returns MATCH, NO_MATCH, or 1.
     * (EOF is considered as NO_MATCH).
     */
    int r;
    char x, ch;
    size_t i;

    if (str == NULL)
        return NO_MATCH;

    i = 0;
    while (1) {
        x = *(str + i);
        if (x == '\0')
            break;

        r = get_ch(input, &ch);
        if (r == 1)
            mreturn(1);
        else if (r == EOF)
            goto no_match;

        if (x != ch) {
            if (unget_ch(*input, ch))
                mreturn(1);

            goto no_match;
        }

        ++i;
    }

    return MATCH;

no_match:
    /* Return the read characters */
    while (i) {
        if (unget_ch(*input, *(str + i - 1)))
            mreturn(1);

        --i;
    }

    return NO_MATCH;
}

int get_word(struct ibuf **input, struct obuf *token, int interpret_hex)
{
    int r;
    char ch, type;
    int second_ch;

    token->i = 0;

    if ((r = get_ch(input, &ch)))
        return r;

    if (put_ch(token, ch))
        mreturn(1);

    if (isdigit(ch))
        type = 'd';                    /* Decimal (or octal) number */
    else if (isalpha(ch) || ch == '_') /* First char cannot be a digit */
        type = 'w'; /* Word (valid variable or macro name) */
    else
        goto end; /* Send a single char */

    second_ch = 1;
    while (1) {
        r = get_ch(input, &ch);
        if (r == 1)
            mreturn(1);
        else if (r == EOF) /* Ignore, as not the first char */
            goto end;

        if (interpret_hex && second_ch && type == 'd'
            && (ch == 'x' || ch == 'X'))
            type = 'h'; /* Hexadecimal number */

        /* More of the same type. Words can include digits here. */
        if ((type == 'd' && isdigit(ch))
            || (type == 'w' && (isalnum(ch) || ch == '_'))
            || (type == 'h' && (second_ch || isxdigit(ch)))) {
            if (put_ch(token, ch))
                mreturn(1);
        } else {
            if (unget_ch(*input, ch))
                mreturn(1);

            goto end;
        }

        second_ch = 0;
    }

end:
    if (put_ch(token, '\0')) { /* Terminate string */
        mreturn(1);
    }

    return 0;
}

/* ###################################################################### */

struct obuf *init_obuf(size_t n)
{
    struct obuf *b = NULL;

    if ((b = calloc(1, sizeof(struct obuf))) == NULL)
        mgoto(error);

    if ((b->a = calloc(n, 1)) == NULL)
        mgoto(error);

    b->i = 0;
    b->n = n;
    return b;

error:
    free_obuf(b);
    return NULL;
}

void free_obuf(struct obuf *b)
{
    if (b != NULL) {
        free(b->a);
        free(b);
    }
}

void free_obuf_exterior(struct obuf *b)
{
    free(b);
}

static int grow_obuf(struct obuf *b, size_t will_use)
{
    char *t;
    size_t new_n;

    if (will_use == 0)
        return 0;

    if (aof(b->n, will_use, SIZE_MAX))
        mreturn(1);

    new_n = b->n + will_use;

    if (mof(new_n, 2, SIZE_MAX))
        mreturn(1);

    new_n *= 2;

    if (new_n == 0)
        return 0;

    if ((t = realloc(b->a, new_n)) == NULL)
        mreturn(1);

    b->a = t;
    b->n = new_n;
    return 0;
}

int put_ch(struct obuf *b, char ch)
{
    if (b->i == b->n && grow_obuf(b, 1))
        mreturn(1);

    *(b->a + b->i) = ch;
    ++b->i;
    return 0;
}

int put_str(struct obuf *b, const char *str)
{
    size_t i_backup = b->i;
    char ch;

    while ((ch = *str++) != '\0') {
        if (b->i == b->n && grow_obuf(b, 1)) {
            b->i = i_backup; /* Restore */
            mreturn(1);
        }

        *(b->a + b->i++) = ch;
    }

    return 0;
}

int put_mem(struct obuf *b, const char *mem, size_t mem_len)
{
    if (mem_len > b->n - b->i && grow_obuf(b, mem_len))
        mreturn(1);

    memcpy(b->a + b->i, mem, mem_len);
    b->i += mem_len;
    return 0;
}

int put_obuf(struct obuf *b, struct obuf *t)
{
    /* Empties t onto the end of b */
    if (t->i == 0)
        return 0; /* Nothing to do. */

    if (put_mem(b, t->a, t->i))
        mreturn(1);

    t->i = 0;
    return 0;
}

int put_file(struct obuf *b, const char *fn)
{
    int ret = 1;
    FILE *fp = NULL;
    size_t fs;

    if (fn == NULL || *fn == '\0')
        mreturn(1);

    if ((fp = fopen(fn, "rb")) == NULL)
        mgoto(clean_up);

    if (get_file_size(fn, &fs))
        mgoto(clean_up);

    if (!fs)
        mgoto(done);

    if (fs > b->n - b->i && grow_obuf(b, fs))
        mgoto(clean_up);

    if (fread(b->a + b->i, 1, fs, fp) != fs)
        mgoto(clean_up);

    b->i += fs;

done:
    ret = 0;

clean_up:
    if (fp != NULL)
        if (fclose(fp))
            ret = 1;

    return ret;
}

int put_stream(struct obuf *b, FILE *fp)
{
    size_t i_backup, rs;

    if (fp == NULL)
        mreturn(1);

    i_backup = b->i;

    while (1) {
        if (READ_BLOCK_SIZE > b->n - b->i && grow_obuf(b, READ_BLOCK_SIZE))
            mgoto(error);

        rs = fread(b->a + b->i, 1, READ_BLOCK_SIZE, fp);
        b->i += rs;

        if (rs != READ_BLOCK_SIZE) {
            if (feof(fp) && !ferror(fp))
                break;
            else
                mgoto(error);
        }
    }
    return 0;

error:
    b->i = i_backup;
    return 1;
}

int write_obuf(struct obuf *b, const char *fn, int append)
{
    /* Empties b to file fn */
    FILE *fp;

    if (fn == NULL || *fn == '\0')
        mreturn(1);

    if ((fp = fopen_w(fn, append)) == NULL)
        mreturn(1);

    if (fwrite(b->a, 1, b->i, fp) != b->i) {
        fclose(fp);
        mreturn(1);
    }
    if (fclose(fp))
        mreturn(1);

    b->i = 0;
    return 0;
}

int flush_obuf(struct obuf *b, int tty_output)
{
    size_t i;
    char ch;

    if (!b->i)
        return 0;

    if (tty_output) {
        for (i = 0; i < b->i; ++i) {
            ch = *(b->a + i);

            if (isprint(ch) || ch == '\n')
                putchar(ch);
            else if (ch >= 1 && ch <= 26)
                printf("^%c", 'A' + ch - 1);
            else
                switch (ch) {
                case 0:
                    printf("^@");
                    break;
                case 27:
                    printf("^[");
                    break;
                case 28:
                    printf("^\\");
                    break;
                case 29:
                    printf("^]");
                    break;
                case 30:
                    printf("^^");
                    break;
                case 31:
                    printf("^_");
                    break;
                case 127:
                    printf("^?");
                    break;
                default:
                    printf("\\x%02X", (unsigned char) ch);
                    break;
                }
        }
    } else {
        if (fwrite(b->a, 1, b->i, stdout) != b->i)
            mreturn(1);
    }

    if (fflush(stdout))
        mreturn(1);

    b->i = 0;
    return 0;
}

char *obuf_to_str(struct obuf **b)
{
    char *str;
    if (put_ch(*b, '\0'))
        mreturn(NULL);

    /* Success */
    str = (*b)->a;
    free(*b);
    *b = NULL;
    return str;
}

/* ###################################################################### */

struct lbuf *init_lbuf(size_t n)
{
    struct lbuf *b = NULL;

    if ((b = calloc(1, sizeof(struct lbuf))) == NULL)
        mgoto(error);

    if (mof(n, sizeof(long), SIZE_MAX))
        mgoto(error);

    if ((b->a = calloc(n, sizeof(long))) == NULL)
        mgoto(error);

    b->i = 0;
    b->n = n;
    return b;

error:
    free_lbuf(b);
    return NULL;
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

    if (will_use == 0)
        return 0;

    if (aof(b->n, will_use, SIZE_MAX))
        mreturn(1);

    new_n = b->n + will_use;

    if (mof(new_n, 2, SIZE_MAX))
        mreturn(1);

    new_n *= 2;

    if (mof(new_n, sizeof(long), SIZE_MAX))
        mreturn(1);

    if (new_n == 0)
        return 0;

    if ((t = realloc(b->a, new_n * sizeof(long))) == NULL)
        mreturn(1);

    b->a = t;
    b->n = new_n;
    return 0;
}

int add_l(struct lbuf *b, long x)
{
    if (b->i == b->n && grow_lbuf(b, 1))
        mreturn(1);

    *(b->a + b->i) = x;
    ++b->i;
    return 0;
}

/* ###################################################################### */

struct sbuf *init_sbuf(size_t n)
{
    struct sbuf *b = NULL;

    if ((b = calloc(1, sizeof(struct sbuf))) == NULL)
        mgoto(error);

    if (mof(n, sizeof(size_t), SIZE_MAX))
        mgoto(error);

    if ((b->a = calloc(n, sizeof(size_t))) == NULL)
        mgoto(error);

    b->i = 0;
    b->n = n;
    return b;

error:
    free_sbuf(b);
    return NULL;
}

void free_sbuf(struct sbuf *b)
{
    if (b != NULL) {
        free(b->a);
        free(b);
    }
}

static int grow_sbuf(struct sbuf *b, size_t will_use)
{
    size_t *t;
    size_t new_n;

    if (will_use == 0)
        return 0;

    if (aof(b->n, will_use, SIZE_MAX))
        mreturn(1);

    new_n = b->n + will_use;

    if (mof(new_n, 2, SIZE_MAX))
        mreturn(1);

    new_n *= 2;

    if (mof(new_n, sizeof(size_t), SIZE_MAX))
        mreturn(1);

    if (new_n == 0)
        return 0;

    if ((t = realloc(b->a, new_n * sizeof(size_t))) == NULL)
        mreturn(1);

    b->a = t;
    b->n = new_n;
    return 0;
}

int add_s(struct sbuf *b, size_t x)
{
    if (b->i == b->n && grow_sbuf(b, 1))
        mreturn(1);

    *(b->a + b->i) = x;
    ++b->i;
    return 0;
}

/* ###################################################################### */

struct pbuf *init_pbuf(size_t n)
{
    struct pbuf *b = NULL;

    if ((b = calloc(1, sizeof(struct pbuf))) == NULL)
        mgoto(error);

    if (mof(n, sizeof(void *), SIZE_MAX))
        mgoto(error);

    if ((b->a = calloc(n, sizeof(void *))) == NULL)
        mgoto(error);

    b->i = 0;
    b->n = n;
    return b;

error:
    free_pbuf(b);
    return NULL;
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

    if (will_use == 0)
        return 0;

    if (aof(b->n, will_use, SIZE_MAX))
        mreturn(1);

    new_n = b->n + will_use;

    if (mof(new_n, 2, SIZE_MAX))
        mreturn(1);

    new_n *= 2;

    if (mof(new_n, sizeof(void *), SIZE_MAX))
        mreturn(1);

    if (new_n == 0)
        return 0;

    if ((t = realloc(b->a, new_n * sizeof(void *))) == NULL)
        mreturn(1);

    b->a = t;
    b->n = new_n;
    return 0;
}

int add_p(struct pbuf *b, void *x)
{
    if (b->i == b->n && grow_pbuf(b, 1))
        mreturn(1);

    *(b->a + b->i) = x;
    ++b->i;
    return 0;
}
