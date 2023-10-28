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

/* File system related module */

#ifdef __linux__
/* For: strdup */
#define _XOPEN_SOURCE 500
#endif

#ifdef _WIN32
#include <direct.h>
#else
#include <sys/stat.h>
#endif

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "fs.h"
#include "debug.h"


FILE *fopen_w(const char *fn)
{
    /* Creates missing directories and opens a file for writing */
    FILE *fp;
    char *p, *q, ch;

    errno = 0;
    fp = fopen(fn, "wb");
    if (fp != NULL)
        mreturn(fp);

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

    mreturn(fopen(fn, "wb"));
}
