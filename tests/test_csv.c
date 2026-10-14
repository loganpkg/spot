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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "csv.h"

int main(void)
{
    char *csv = "\"\",\",\",\"\"\"\",\"\"\"\"\"\",\"\n\",\n"
                "a\",b\"\",    c,d    ,    e    ,\n"
                "\"    f    \",    \"g\"    ,wow\n"
                ",    \"    h    \"    ,    \"cool,,,world\"";

    size_t len;
    char *p;
    size_t num_cols, line_size;
    char **fields;
    size_t k;

    len = strlen(csv);
    p = csv;

    printf("CSV:\n%s\n", csv);

    printf("len: %" lu "\n", len);

    while (len) {
        if ((fields = split_csv_line(
                 p, len, ',', '"', 1, '\n', &num_cols, &line_size))
            == NULL)
            debug(return 1);

        for (k = 0; k < num_cols; ++k) {
            printf("Column %" lu ": [%s]\n", k, fields[k]);
            if (fields[k] != NULL) {
                free(fields[k]);
                fields[k] = NULL;
            }
        }

        if (fields != NULL) {
            free(fields);
            fields = NULL;
        }

        printf("num_cols: %" lu "\n", num_cols);
        printf("line_size: %" lu "\n", line_size);

        len -= line_size;
        printf("len: %" lu "\n", len);

        p += line_size;
    }
    return 0;
}
