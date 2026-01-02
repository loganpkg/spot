/*
 * Copyright (c) 2023-2025 Logan Ryan McLintock. All rights reserved.
 *
 * The contents of this file are subject to the
 * Common Development and Distribution License (CDDL) version 1.1
 * (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License in
 * the LICENSE file included with this software or at
 * https://opensource.org/license/cddl-1-1
 *
 * NOTICE PURSUANT TO SECTION 4.2 OF THE LICENSE:
 * This software is prohibited from being distributed or otherwise made
 * available under any subsequent version of the License.
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See the
 * License for the specific language governing rights and limitations
 * under the License.
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
