/*
 * Copyright (c) 2024 Logan Ryan McLintock
 *
 * Permission to use, copy, modify, and/or distribute this software for any
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

/* freq: Character frequency in a file */

#include "toucanlib.h"


int main(int argc, char **argv)
{
    int ret = 1;
    void *mem;
    unsigned char *p = NULL, u;
    size_t fs, i, y;
    size_t freq[UCHAR_MAX + 1];
    int j;

    if (argc != 2) {
        fprintf(stderr, "Usage: freq file\n");
        return 1;
    }

    if (binary_io())
        return 1;

    if (mmap_file_ro(*(argv + 1), &mem, &fs))
        return 1;

    /* Empty file. Nothing to do. */
    if (mem == NULL)
        return 0;

    p = mem;

    /* Initialise */
    for (j = 0; j < UCHAR_MAX + 1; ++j)
        freq[j] = 0;

    for (i = 0; i < fs; ++i) {
        u = *(p + i);

        /* Overflow check */
        if (freq[u] == SIZE_MAX)
            mgoto(clean_up);

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

    return ret;
}
