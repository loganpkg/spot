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

#ifndef GEN_H
#define GEN_H

/* For: size_t */
#include <stddef.h>

/* For: FILE */
#include <stdio.h>

/* To stop empty translation unit error */
typedef int gen_dummy;

/* EOF cannot be 1, so OK */
#define ERR 1

#define NUM_BUF_SIZE 32

#ifdef _WIN32
#define popen _popen
#define pclose _pclose
#endif

#ifndef _WIN32
#define mkdir(dir) mkdir(dir, S_IRWXU)
#endif

/* Function declarations */
int sane_io(void);
char *concat(const char *str, ...);
void *quick_search(const void *mem, size_t mem_len, const void *find,
                   size_t find_len);
FILE *fopen_w(const char *fn);

#endif
