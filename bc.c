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
