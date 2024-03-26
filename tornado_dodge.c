/*
 * Copyright (c) 2024 Logan Ryan McLintock
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

/* Tornado Dodge -- console video game */

#include "toucanlib.h"

int main(void)
{
    int ret = ERR;
    struct screen *s = NULL;
    int x;
    size_t man_y = 0, man_x = 0;

    char *man = " o \n" "<|>\n" "/\\ ";
    char *man_vert = " o \n" " V \n" " | ";

    if ((s = init_screen()) == NULL)
        return ERR;

    while (1) {
        if (erase_screen(s))
            goto clean_up;

        if (man_x % 2) {
            if (print_object(s, man_y, man_x, man_vert))
                goto clean_up;
        } else {
            if (print_object(s, man_y, man_x, man))
                goto clean_up;
        }

        refresh_screen(s);

        x = get_key();

        if (x == RIGHT_KEY)
            ++man_x;
        else if (x == 'q')
            break;
    }

    ret = 0;

  clean_up:
    if (free_screen(s))
        ret = ERR;

    return ret;
}
