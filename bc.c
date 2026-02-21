/*
 * Copyright (c) 2023-2025 Logan Ryan McLintock. All rights reserved.
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
    int ret = 1;
    struct ibuf *input = NULL;
    long x;

    if (binary_io())
        mreturn(1);

    if (unget_stream(&input, stdin, "stdin"))
        mreturn(1);

    while (1) {
        ret = eval_ibuf(&input, &x, 0);

        if (!ret)
            printf("%ld\n", x);
        else if (ret == 1 || ret == EOF)
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
