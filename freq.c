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
