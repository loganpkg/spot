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
#include <curses.h>


int print_object(size_t y, size_t x, const char *object)
{
    char ch;
    size_t c_y, c_x;

    if (move(y, x) == ERR)
        return ERR;

    while ((ch = *object++) != '\0') {
        if (addch(ch) == ERR)
            return ERR;

        if (ch == '\n') {
            /* Indent */
            getyx(stdscr, c_y, c_x);
            if (c_x)
                return ERR;

            if (move(c_y, x) == ERR)
                return ERR;
        }
    }

    return OK;
}


int main(void)
{
    int ret = ERR;
    int x;
    size_t man_y = 0, man_x = 0;

    char *man = " o \n" "<|>\n" "/\\ ";
    char *man_vert = " o \n" " V \n" " | ";

    if (initscr() == NULL)
        mgoto(clean_up);

    if (raw() == ERR)
        mgoto(clean_up);

    if (noecho() == ERR)
        mgoto(clean_up);

    if (keypad(stdscr, TRUE) == ERR)
        mgoto(clean_up);

    if (set_tabsize(8) == ERR)
        mgoto(clean_up);

    while (1) {
        if (erase())
            mgoto(clean_up);

        if (man_x % 2) {
            if (print_object(man_y, man_x, man_vert))
                mgoto(clean_up);
        } else {
            if (print_object(man_y, man_x, man))
                mgoto(clean_up);
        }

        refresh();

        x = getch();

        if (x == KEY_RIGHT)
            ++man_x;
        else if (x == 'q')
            break;
    }

    ret = 0;

  clean_up:
    if (endwin() == ERR)
        ret = ERR;

    return ret;
}
