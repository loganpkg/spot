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

/* freq: Character frequency in a file */

#include "toucanlib.h"


int main(int argc, char **argv)
{
    int ret = 1;
    unsigned char *p = NULL, u;
    size_t fs, i, y;
    size_t freq[UCHAR_MAX + 1] = { 0 };
    int j;

    if (argc != 2) {
        fprintf(stderr, "Usage: freq file\n");
        return 1;
    }

    if (sane_io())
        return 1;

    if ((p = mmap_file_ro(*(argv + 1), &fs)) == NULL)
        return 1;

    for (i = 0; i < fs; ++i) {
        u = *(p + i);

        /* Overflow check */
        if (freq[u] == SIZE_MAX)
            goto clean_up;

        ++freq[u];
    }

    /* Print results */
    for (j = 0; j < UCHAR_MAX + 1; ++j) {
        if ((y = freq[j])) {
            if (isgraph(j))
                printf("%c %lu\n", j, (unsigned long) y);
            else
                printf("%02X %lu\n", j, (unsigned long) y);
        }
    }

    ret = 0;

  clean_up:
    if (p != NULL && un_mmap(p, fs))
        ret = 1;

    mreturn(ret);
}
