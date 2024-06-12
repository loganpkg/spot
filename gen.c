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

/* Generic module */

#include "toucanlib.h"


#define INIT_CONCAT_BUF 512


int sane_io(void)
{
#ifdef _WIN32
    if (_setmode(_fileno(stdin), _O_BINARY) == -1)
        return ERR;

    if (_setmode(_fileno(stdout), _O_BINARY) == -1)
        return ERR;

    if (_setmode(_fileno(stderr), _O_BINARY) == -1)
        return ERR;
#endif
    return 0;
}

char *concat(const char *str, ...)
{
    va_list v;
    struct obuf *b;
    const char *p;
    char *res;

    if (str == NULL)
        mreturn(NULL);

    if ((b = init_obuf(INIT_CONCAT_BUF)) == NULL)
        mreturn(NULL);

    p = str;

    va_start(v, str);

    while (1) {
        if (put_str(b, p)) {
            free_obuf(b);
            mreturn(NULL);
        }
        if ((p = va_arg(v, const char *)) == NULL)
            break;
    }

    va_end(v);

    if (put_ch(b, '\0')) {
        free_obuf(b);
        mreturn(NULL);
    }

    res = b->a;
    free(b);                    /* Free struct only, not memory inside */

    return res;
}

void *quick_search(const void *mem, size_t mem_len, const void *find,
                   size_t find_len)
{
    /*
     * Sunday's Quick Search algoritm.
     * Exact match.
     */
    size_t b[UCHAR_MAX + 1];
    unsigned char *p, *p_last, *x, *q, *q_stop;
    size_t i;

    if (find_len > mem_len)
        return NULL;

    for (i = 0; i < UCHAR_MAX + 1; ++i)
        b[i] = find_len + 1;

    q = (unsigned char *) find;
    q_stop = q + find_len;      /* Exclusive */

    for (i = 0; i < find_len; ++i)
        b[q[i]] = find_len - i;

    p = (unsigned char *) mem;
    p_last = p + mem_len - find_len;    /* Inclusive */

    while (p <= p_last) {
        x = p;
        q = (unsigned char *) find;
        while (1) {
            if (q == q_stop)
                return p;       /* Full match */

            if (*x++ != *q++)
                break;
        }

        p += b[p[find_len]];
    }
    return NULL;
}

FILE *fopen_w(const char *fn, int append)
{
    /* Creates missing directories and opens a file for writing */
    FILE *fp;
    char *p, *q, ch;
    char *mode = append ? "ab" : "wb";

    errno = 0;
    fp = fopen(fn, mode);
    if (fp != NULL)
        return fp;

    if (fp == NULL && errno != ENOENT)
        mreturn(NULL);

    /* Try to make missing directories */
    if ((p = strdup(fn)) == NULL)
        mreturn(NULL);

    q = p;

    while ((ch = *q) != '\0') {
        if (ch == '/' || ch == '\\') {
            *q = '\0';
            errno = 0;
            if (mkdir(p) && errno != EEXIST) {
                free(p);
                mreturn(NULL);
            }
            *q = ch;
        }
        ++q;
    }

    free(p);

    return fopen(fn, mode);
}
