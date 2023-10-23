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

/*
 * bc -- Basic calculator.
 *
 * Jesus said to him, "Do you believe because you see me?
 * How happy are those who believe without seeing me!"
 *                                      John 20:29 GNT
 */


#include <stdio.h>
#include <string.h>

#include "debug.h"
#include "gen.h"
#include "eval.h"

#define INIT_BUF_SIZE 100

int main(void)
{
    int r, e = 0;
    struct ibuf *input = NULL;
    int math_error = 0;
    long x;

    if (sane_io())
        mreturn(1);

    if ((input = init_ibuf(INIT_BUF_SIZE)) == NULL)
        mreturn(1);

    while (1) {
        r = eval(input, 1, &math_error, &x);
        if (r)
            break;

        if (math_error) {
            e = 1;
            fprintf(stderr, "bc: Math error\n");
        } else {
            printf("%ld\n", x);
        }

        math_error = 0;
    }

    free_ibuf(input);

    if (r == ERR || e)
        mreturn(1);

    mreturn(0);
}
