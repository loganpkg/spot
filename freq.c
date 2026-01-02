/*
 * Copyright (c) 2023-2025 Logan Ryan McLintock. All rights reserved.
 *
 * The contents of this file are subject to the
 * Common Development and Distribution License (CDDL) version 1.1
 * (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License in
 * the LICENSE file included with this software or at
 * https://opensource.org/license/cddl-1-1
 *
 * NOTICE PURSUANT TO SECTION 4.2 OF THE LICENSE:
 * This software is prohibited from being distributed or otherwise made
 * available under any subsequent version of the License.
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See the
 * License for the specific language governing rights and limitations
 * under the License.
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
