/*
 * Copyright (c) 2026 Logan Ryan McLintock. All rights reserved.
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

/* Cross-platform version of asprintf. */

#ifdef __linux__
#define _GNU_SOURCE
#endif

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

int asprintf(char **ret_str, const char *format_str, ...)
{
#ifdef _WIN32
    int len;
    size_t s;
    char *str = NULL;
#else
    int r;
#endif

    va_list v_args;
    va_start(v_args, format_str);

#ifdef _WIN32

    if ((len = _vscprintf(format_str, v_args)) < 0)
        goto error;

    if ((unsigned int) len >= SIZE_MAX)
        goto error;

    s = (size_t) len + 1;

    if ((str = calloc(s, 1)) == NULL)
        goto error;

    if (vsprintf_s(str, s, format_str, v_args) < 0)
        goto error;

    va_end(v_args);

    *ret_str = str;

    return len;

error:
    va_end(v_args);
    if (str != NULL)
        free(str);

    return -1;

#else

    r = vasprintf(ret_str, format_str, v_args);

    va_end(v_args);

    return r;

#endif
}
