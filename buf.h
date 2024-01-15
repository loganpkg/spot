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

#ifndef BUF_H
#define BUF_H

#include <stddef.h>

#define buf_members                                           \
    char *a;                    /* Memory */                  \
    size_t i;                   /* Write index */             \
    size_t s                    /* Allocated size in bytes */

/*
 * Input buffer. Characters are stored in reverse order.
 * Operated on by get and unget functions.
 */
struct ibuf {
    buf_members;
};

/*
 * Ouput buffer. Characters are stored in normal order.
 * Operated on by put functions.
 */
struct obuf {
    buf_members;
};

#undef buf_members

/* Buffer of long integers */
struct lbuf {
    long *a;                    /* Memory */
    size_t i;                   /* Write index */
    size_t n;                   /* Number of elements, not bytes */
};

/* Buffer of pointers */
struct pbuf {
    void **a;                   /* Array of pointers */
    size_t i;                   /* Write index */
    size_t n;                   /* Number of elements, not bytes */
};

/* Function declarations */
struct ibuf *init_ibuf(size_t s);
struct obuf *init_obuf(size_t s);
void free_ibuf(struct ibuf *b);
void free_obuf(struct obuf *b);
int unget_ch(struct ibuf *b, char ch);
int put_ch(struct obuf *b, char ch);
int unget_str(struct ibuf *b, const char *str);
int unget_file(struct ibuf *b, const char *fn);
int get_ch(struct ibuf *input, char *ch, int read_stdin);
int get_word(struct ibuf *input, struct obuf *token, int read_stdin);
int eat_whitespace(struct ibuf *input, int read_stdin);
int delete_to_nl(struct ibuf *input, int read_stdin);
int put_str(struct obuf *b, const char *str);
int put_mem(struct obuf *b, const char *mem, size_t mem_len);
int put_obuf(struct obuf *b, struct obuf *t);
int put_file(struct obuf *b, const char *fn);
int write_obuf(struct obuf *b, const char *fn);
int flush_obuf(struct obuf *b);
struct lbuf *init_lbuf(size_t n);
void free_lbuf(struct lbuf *b);
int add_l(struct lbuf *b, long x);
struct pbuf *init_pbuf(size_t n);
void free_pbuf(struct pbuf *b);
int add_p(struct pbuf *b, void *ptr);

#endif
