/*
 * Copyright (c) 2023 Logan Ryan McLintock
 *
 * Permission to use, copy, modify, and distribute this software for any
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
