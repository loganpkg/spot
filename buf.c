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

/* Buffer module */

#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "buf.h"
#include "debug.h"
#include "num.h"

struct buf *init_buf(size_t s)
{
    struct buf *b;

    if ((b = malloc(sizeof(struct buf))) == NULL)
        mreturn(NULL);

    if ((b->a = malloc(s)) == NULL)
        mreturn(NULL);

    b->i = 0;
    b->s = s;
    mreturn(b);
}

void free_buf(struct buf *b)
{
    if (b != NULL) {
        free(b->a);
        free(b);
    }
}

static int grow_buf(struct buf *b, size_t will_use)
{
    char *t;
    size_t new_s;

    if (aof(b->s, will_use))
        mreturn(1);

    new_s = b->s + will_use;

    if (mof(new_s, 2))
        mreturn(1);

    new_s *= 2;

    if ((t = realloc(b->a, new_s)) == NULL)
        mreturn(1);

    b->a = t;
    b->s = new_s;
    mreturn(0);
}

int put_ch(struct buf *b, char ch)
{
    if (b->i == b->s && grow_buf(b, 1))
        mreturn(1);

    *(b->a + b->i) = ch;
    ++b->i;
    mreturn(0);
}

int put_str(struct buf *b, const char *str)
{
    size_t len;

    len = strlen(str);
    if (!len)
        mreturn(0);

    if (len > b->s - b->i && grow_buf(b, len))
        mreturn(1);

    memcpy(b->a + b->i, str, len);
    b->i += len;
    mreturn(0);
}

int unget_str(struct buf *b, const char *str)
{
    size_t len, j;
    char *p;

    len = strlen(str);

    if (!len)
        mreturn(0);

    if (len > b->s - b->i && grow_buf(b, len))
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

int put_buf(struct buf *b, struct buf *t)
{
    /* Empties t onto the end of b */
    if (t->i > b->s - b->i && grow_buf(b, t->i))
        mreturn(1);

    memcpy(b->a + b->i, t->a, t->i);
    b->i += t->i;
    t->i = 0;
    mreturn(0);
}

int put_file(struct buf *b, const char *fn)
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

    if (fs > b->s - b->i && grow_buf(b, fs))
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

int unget_file(struct buf *b, const char *fn)
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

    if (fs > b->s - b->i && grow_buf(b, fs))
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

int write_buf(struct buf *b, const char *fn)
{
    /* Empties b to file fn */
    FILE *fp;

    if (fn == NULL || *fn == '\0')
        mreturn(1);

    if ((fp = fopen(fn, "wb")) == NULL)
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

int flush_buf(struct buf *b)
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

int get_ch(struct buf *input, char *ch, int read_stdin)
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

int get_word(struct buf *input, struct buf *token, int read_stdin)
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

int eat_whitespace(struct buf *input, int read_stdin)
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
