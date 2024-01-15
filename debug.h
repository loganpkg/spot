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

/* Debug module */

#ifndef DEBUG_H
#define DEBUG_H

/* To stop empty translation unit error */
typedef int debug_dummy;

/* Comment out to turn off debugging */
#define DEBUG

/* Used to not trigger the debugging */
#define NULL_OK NULL


#ifdef DEBUG
#define mreturn(ret) do {                                                    \
    if (!strcmp(#ret, "1") || !strcmp(#ret, "NULL") || !strcmp(#ret, "ERR")) \
        fprintf(stderr, "%s:%d: mreturn: %s\n", __FILE__, __LINE__, #ret);   \
    return ret;                                                              \
} while (0)
#else
#define mreturn(ret) return ret
#endif

#ifdef DEBUG
#define mgoto(lab) do {                                                  \
    if (!strcmp(#lab, "clean_up") || !strcmp(#lab, "error"))             \
        fprintf(stderr, "%s:%d: mgoto: %s\n", __FILE__, __LINE__, #lab); \
    goto lab;                                                            \
} while (0)
#else
#define mgoto(lab) goto lab
#endif

#endif
