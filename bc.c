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


#include "toucanlib.h"


#define INIT_BUF_SIZE 100

int main(void)
{
    int ret = ERR;
    struct ibuf *input = NULL;
    long x;

    if (sane_io()) {
        fprintf(stderr, "%s:%d: Error\n", __FILE__, __LINE__);
        return ERR;
    }

    if ((input = init_ibuf(INIT_BUF_SIZE)) == NULL) {
        fprintf(stderr, "%s:%d: Error\n", __FILE__, __LINE__);
        return ERR;
    }

    while (1) {
        ret = eval(input, 1, &x, 0);

        if (!ret)
            printf("%ld\n", x);
        else if (ret == ERR || ret == EOF)
            break;
        else
            fprintf(stderr, "bc: Math error\n");
    }

    free_ibuf(input);

    if (ret == EOF)
        return 0;
    else
        return ret;
}
