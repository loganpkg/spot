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

#ifdef _WIN32
#include <io.h>
#include <fcntl.h>
#endif

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* #include "gen.h" */
#include "debug.h"
#include "buf.h"
#include "num.h"

#define INIT_CONCAT_BUF 512


int sane_io(void)
{
#ifdef _WIN32
    if (_setmode(_fileno(stdin), _O_BINARY) == -1)
        return 1;

    if (_setmode(_fileno(stdout), _O_BINARY) == -1)
        return 1;

    if (_setmode(_fileno(stderr), _O_BINARY) == -1)
        return 1;
#endif
    return 0;
}

char *concat(const char *str, ...)
{
    va_list v;
    struct obuf *b;
    const char *p;
    char *res;

    if (str == NULL)
        mreturn(NULL);

    if ((b = init_obuf(INIT_CONCAT_BUF)) == NULL)
        mreturn(NULL);

    p = str;

    va_start(v, str);

    do {
        if (put_str(b, p)) {
            free_obuf(b);
            mreturn(NULL);
        }
        if ((p = va_arg(v, const char *)) == NULL)
            break;
    } while (1);

    va_end(v);

    if (put_ch(b, '\0')) {
        free_obuf(b);
        mreturn(NULL);
    }

    res = b->a;
    free(b);                    /* Free struct only, not memory inside */

    mreturn(res);
}
