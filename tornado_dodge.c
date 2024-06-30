/*
 * Copyright (c) 2024 Logan Ryan McLintock
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
