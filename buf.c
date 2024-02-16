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


struct ibuf *init_ibuf(size_t s)
{
    struct ibuf *b;

    if ((b = malloc(sizeof(struct ibuf))) == NULL)
        mreturn(NULL);

    if ((b->a = malloc(s)) == NULL)
        mreturn(NULL);

    b->i = 0;
    b->s = s;
    mreturn(b);
}

struct obuf *init_obuf(size_t s)
{
    mreturn((struct obuf *) init_ibuf(s));
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

    if (aof(b->s, will_use, SIZE_MAX))
        mreturn(1);

    new_s = b->s + will_use;

    if (mof(new_s, 2, SIZE_MAX))
        mreturn(1);

    new_s *= 2;

    if ((t = realloc(b->a, new_s)) == NULL)
        mreturn(1);

    b->a = t;
    b->s = new_s;
    mreturn(0);
}

static int grow_obuf(struct obuf *b, size_t will_use)
{
    mreturn(grow_ibuf((struct ibuf *) b, will_use));
}

int unget_ch(struct ibuf *b, char ch)
{
    if (b->i == b->s && grow_ibuf(b, 1))
        mreturn(1);

    *(b->a + b->i) = ch;
    ++b->i;
    mreturn(0);
}

int put_ch(struct obuf *b, char ch)
{
    mreturn(unget_ch((struct ibuf *) b, ch));
}

int unget_str(struct ibuf *b, const char *str)
{
    size_t len, j;
    char *p;

    len = strlen(str);

    if (!len)
        mreturn(0);

    if (len > b->s - b->i && grow_ibuf(b, len))
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
    mreturn(0);
}

int unget_file(struct ibuf *b, const char *fn)
{
    int ret = 1;
    FILE *fp = NULL;
    long fs_l;
    size_t fs, j;
    char *p;
    int x;

    if (fn == NULL || *fn == '\0')
        mreturn(1);

    if ((fp = fopen(fn, "rb")) == NULL)
        mgoto(clean_up);

    if (fseek(fp, 0L, SEEK_END))
        mgoto(clean_up);

    if ((fs_l = ftell(fp)) == -1 || fs_l < 0)
        mgoto(clean_up);

    if (fseek(fp, 0L, SEEK_SET))
        mgoto(clean_up);

    if (!fs_l)
        mgoto(done);

    fs = (size_t) fs_l;

    if (fs > b->s - b->i && grow_ibuf(b, fs))
        mgoto(clean_up);

    p = b->a + b->i + fs - 1;
    j = fs;
    while (j) {
        if ((x = getc(fp)) == EOF)
            mgoto(clean_up);

        *p = x;
        --p;
        --j;
    }
    if (ferror(fp))
        mgoto(clean_up);

    b->i += fs;

  done:
    ret = 0;
  clean_up:
    if (fp != NULL && fclose(fp))
        ret = 1;

    mreturn(ret);
}

int get_ch(struct ibuf *input, char *ch, int read_stdin)
{
    int x;

    if (input->i) {
        --input->i;
        *ch = *(input->a + input->i);
        mreturn(0);
    }
    if (!read_stdin)
        mreturn(EOF);

    if ((x = getchar()) == EOF) {
        if (feof(stdin) && !ferror(stdin))
            mreturn(EOF);
        else
            mreturn(ERR);
    }
    *ch = x;
    mreturn(0);
}

int get_word(struct ibuf *input, struct obuf *token, int read_stdin)
{
    int r;
    char ch, type;

    do {
        if ((r = get_ch(input, &ch, read_stdin)) != 0)
            mreturn(r);
    } while (ch == '\0' || ch == '\r'); /* Discard these chars */

    token->i = 0;

    if (put_ch(token, ch))
        mreturn(ERR);

    if (isdigit(ch))
        type = 'd';             /* Decimal number */
    else if (isalpha(ch) || ch == '_')  /* First char cannot be a digit */
        type = 'w';             /* Word */
    else
        mgoto(end);             /* Send a single char */

    while (1) {
        do {
            r = get_ch(input, &ch, read_stdin);
            if (r == ERR)
                mreturn(ERR);
            else if (r == EOF)  /* Ignore, as not the first char */
                mgoto(end);
        } while (ch == '\0' || ch == '\r');

        if ((type == 'd' && isdigit(ch))
            || (type == 'w' && (isalnum(ch) || ch == '_'))) {
            /* More of the same type. Words can include digits here. */
            if (put_ch(token, ch))
                mreturn(ERR);
        } else {
            if (unget_ch(input, ch))
                mreturn(ERR);

            mgoto(end);
        }
    }

  end:
    if (put_ch(token, '\0'))    /* Terminate string */
        mreturn(ERR);

    mreturn(0);
}

int eat_whitespace(struct ibuf *input, int read_stdin)
{
    int r;
    char ch;

    while (1) {
        r = get_ch(input, &ch, read_stdin);
        if (r == ERR)
            mreturn(1);
        else if (r == EOF)
            break;

        if (!(isspace(ch) || ch == '\0')) {
            if (unget_ch(input, ch))
                mreturn(1);

            break;
        }
    }
    mreturn(0);
}

int delete_to_nl(struct ibuf *input, int read_stdin)
{
    /* Delete to (and including) the next newline character */
    int r;
    char ch;

    while (1) {
        r = get_ch(input, &ch, read_stdin);
        if (r == ERR)
            mreturn(1);
        else if (r == EOF)
            break;

        if (ch == '\n')
            break;
    }
    mreturn(0);
}

int put_str(struct obuf *b, const char *str)
{
    size_t i_backup = b->i;
    char ch;

    while ((ch = *str++) != '\0') {
        if (b->i == b->s && grow_obuf(b, 1)) {
            b->i = i_backup;    /* Restore */
            mreturn(1);
        }

        *(b->a + b->i++) = ch;
    }

    mreturn(0);
}

int put_mem(struct obuf *b, const char *mem, size_t mem_len)
{
    if (mem_len > b->s - b->i && grow_obuf(b, mem_len))
        mreturn(1);

    memcpy(b->a + b->i, mem, mem_len);
    b->i += mem_len;
    mreturn(0);
}

int put_obuf(struct obuf *b, struct obuf *t)
{
    /* Empties t onto the end of b */
    if (put_mem(b, t->a, t->i))
        mreturn(1);

    t->i = 0;
    mreturn(0);
}

int put_file(struct obuf *b, const char *fn)
{
    int ret = 1;
    FILE *fp = NULL;
    long fs_l;
    size_t fs;

    if (fn == NULL || *fn == '\0')
        mreturn(1);

    if ((fp = fopen(fn, "rb")) == NULL)
        mgoto(clean_up);

    if (fseek(fp, 0L, SEEK_END))
        mgoto(clean_up);

    if ((fs_l = ftell(fp)) == -1 || fs_l < 0)
        mgoto(clean_up);

    if (fseek(fp, 0L, SEEK_SET))
        mgoto(clean_up);

    if (!fs_l)
        mgoto(done);

    fs = (size_t) fs_l;

    if (fs > b->s - b->i && grow_obuf(b, fs))
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

    mreturn(ret);
}

int write_obuf(struct obuf *b, const char *fn)
{
    /* Empties b to file fn */
    FILE *fp;

    if (fn == NULL || *fn == '\0')
        mreturn(1);

    if ((fp = fopen_w(fn)) == NULL)
        mreturn(1);

    if (fwrite(b->a, 1, b->i, fp) != b->i) {
        fclose(fp);
        mreturn(1);
    }
    if (fclose(fp))
        mreturn(1);

    b->i = 0;
    mreturn(0);
}

int flush_obuf(struct obuf *b)
{
    if (!b->i)
        mreturn(0);

    if (fwrite(b->a, 1, b->i, stdout) != b->i)
        mreturn(1);

    if (fflush(stdout))
        mreturn(1);

    b->i = 0;
    mreturn(0);
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
    mreturn(str);
}

struct lbuf *init_lbuf(size_t n)
{
    struct lbuf *b;

    if ((b = malloc(sizeof(struct lbuf))) == NULL)
        mreturn(NULL);

    if (mof(n, sizeof(long), SIZE_MAX))
        mreturn(NULL);

    if ((b->a = malloc(n * sizeof(long))) == NULL)
        mreturn(NULL);

    b->i = 0;
    b->n = n;
    mreturn(b);
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

    if (aof(b->n, will_use, SIZE_MAX))
        mreturn(1);

    new_n = b->n + will_use;

    if (mof(new_n, 2, SIZE_MAX))
        mreturn(1);

    new_n *= 2;

    if (mof(new_n, sizeof(long), SIZE_MAX))
        mreturn(1);

    if ((t = realloc(b->a, new_n * sizeof(long))) == NULL)
        mreturn(1);

    b->a = t;
    b->n = new_n;
    mreturn(0);
}


int add_l(struct lbuf *b, long x)
{
    if (b->i == b->n && grow_lbuf(b, 1))
        mreturn(1);

    *(b->a + b->i) = x;
    ++b->i;
    mreturn(0);
}


struct pbuf *init_pbuf(size_t n)
{
    struct pbuf *b;

    if ((b = malloc(sizeof(struct pbuf))) == NULL)
        mreturn(NULL);

    if (mof(n, sizeof(void *), SIZE_MAX))
        mreturn(NULL);

    if ((b->a = malloc(n * sizeof(void *))) == NULL)
        mreturn(NULL);

    b->i = 0;
    b->n = n;
    mreturn(b);
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

    if (aof(b->n, will_use, SIZE_MAX))
        mreturn(1);

    new_n = b->n + will_use;

    if (mof(new_n, 2, SIZE_MAX))
        mreturn(1);

    new_n *= 2;

    if (mof(new_n, sizeof(void *), SIZE_MAX))
        mreturn(1);

    if ((t = realloc(b->a, new_n * sizeof(void *))) == NULL)
        mreturn(1);

    b->a = t;
    b->n = new_n;
    mreturn(0);
}

int add_p(struct pbuf *b, void *ptr)
{
    if (b->i == b->n && grow_pbuf(b, 1))
        mreturn(1);

    *(b->a + b->i) = ptr;
    ++b->i;
    mreturn(0);
}
