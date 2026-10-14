/*
 * Copyright (c) 2025 Logan Ryan McLintock. All rights reserved.
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

#include <debug.h>
#include <gap_buf.h>

#define INIT_NUM_ELEMENTS 10

int main(void)
{
    Gap_buf gb;

    if ((gb = gb_init(INIT_NUM_ELEMENTS)) == NULL)
        debug(goto error);

    gb_debug_print(gb);

    printf("Insert some characters:\n");

    if (gb_insert_ch(gb, 'e'))
        debug(goto error);

    if (gb_insert_ch(gb, 'l'))
        debug(goto error);

    if (gb_insert_ch(gb, 'e'))
        debug(goto error);

    if (gb_insert_ch(gb, 'p'))
        debug(goto error);

    if (gb_insert_ch(gb, 'h'))
        debug(goto error);

    if (gb_insert_ch(gb, 'a'))
        debug(goto error);

    if (gb_insert_ch(gb, 'n'))
        debug(goto error);

    if (gb_insert_ch(gb, 't'))
        debug(goto error);

    gb_debug_print(gb);

    printf("Move left 4 times:\n");

    if (gb_left_ch(gb))
        debug(goto error);

    if (gb_left_ch(gb))
        debug(goto error);

    if (gb_left_ch(gb))
        debug(goto error);

    if (gb_left_ch(gb))
        debug(goto error);

    gb_debug_print(gb);

    printf("Delete two characters:\n");

    if (gb_delete_ch(gb))
        debug(goto error);

    if (gb_delete_ch(gb))
        debug(goto error);

    gb_debug_print(gb);

    printf("Undo once:\n");

    if (gb_undo(gb))
        debug(goto error);

    gb_debug_print(gb);

    printf("Redo once:\n");

    if (gb_redo(gb))
        debug(goto error);

    gb_debug_print(gb);

    printf("Insert a file:\n");

    if (gb_insert_file(gb, "README.md"))
        debug(goto error);

    gb_debug_print(gb);

    printf("Undo:\n");

    if (gb_undo(gb))
        debug(goto error);

    gb_debug_print(gb);

    printf("Redo:\n");

    if (gb_redo(gb))
        debug(goto error);

    gb_debug_print(gb);

    gb_free(gb);
    return 0;

error:
    gb_free(gb);
    debug(return 1);
}
