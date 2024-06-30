/*
 * Copyright (c) 2023 Logan Ryan McLintock
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

/*
 * bc -- Basic calculator.
 *
 * Jesus said to him, "Do you believe because you see me?
 * How happy are those who believe without seeing me!"
 *                                      John 20:29 GNT
 */


#include "toucanlib.h"


#define INIT_BUF_SIZE 100

int main(void)
{
    int ret = ERR;
    struct ibuf *input = NULL;
    long x;

    if (binary_io())
        mreturn(ERR);

    if (unget_stream(&input, stdin, "stdin"))
        mreturn(ERR);

    while (1) {
        ret = eval_ibuf(&input, &x, 0);

        if (!ret)
            printf("%ld\n", x);
        else if (ret == ERR || ret == EOF)
            break;
        else
            fprintf(stderr, "%s:%lu: Math error\n", input->nm, input->rn);
    }

    free_ibuf(input);

    if (ret == EOF)
        return 0;
    else
        return ret;
}
