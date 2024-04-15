/*
 * Copyright (c) 2024 Logan Ryan McLintock
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

/* lsed: Regular expression stream editor */

#include "toucanlib.h"

#define print_usage fprintf(stderr, \
    "Usage: %s find replace -nls|-nli [file]\n", *argv)

int main(int argc, char **argv)
{
    int ret = ERR;
    struct obuf *b = NULL;
    void *p = NULL, *q;
    size_t fs = 0, q_s;
    char *res = NULL;
    size_t res_len;
    int nl_sen;

    if (argc != 4 && argc != 5) {
        print_usage;
        goto clean_up;
    }

    if (!strcmp(*(argv + 3), "-nls")) {
        nl_sen = 1;
    } else if (!strcmp(*(argv + 3), "-nli")) {
        nl_sen = 0;
    } else {
        print_usage;
        goto clean_up;
    }

    if (sane_io())
        goto clean_up;

    if (argc == 4) {
        if ((b = init_obuf(BUFSIZ)) == NULL) {
            fprintf(stderr, "%s:%d: Error\n", __FILE__, __LINE__);
            goto clean_up;
        }
        if (put_stream(b, stdin)) {
            fprintf(stderr, "%s:%d: Error\n", __FILE__, __LINE__);
            goto clean_up;
        }
        q = b->a;
        q_s = b->i;
    } else {
        if (mmap_file_ro(*(argv + 4), &p, &fs)) {
            fprintf(stderr, "%s:%d: Error\n", __FILE__, __LINE__);
            goto clean_up;
        }
        q = p;
        q_s = fs;
    }

    if (regex_replace(q, q_s,
                      *(argv + 1), *(argv + 2),
                      strlen(*(argv + 2)), nl_sen, &res, &res_len, 0)) {
        fprintf(stderr, "%s:%d: Error\n", __FILE__, __LINE__);
        goto clean_up;
    }

    if (fwrite(res, 1, res_len, stdout) != res_len) {
        fprintf(stderr, "%s:%d: Error\n", __FILE__, __LINE__);
        goto clean_up;
    }

    ret = 0;

  clean_up:
    free_obuf(b);
    if (un_mmap(p, fs))
        ret = ERR;

    free(res);

    return ret;
}
