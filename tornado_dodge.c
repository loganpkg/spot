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
    char ch, v_ch;
    size_t c_y, c_x;

    if (move(y, x) == ERR)
        return GEN_ERROR;

    while ((ch = *object++) != '\0') {
        v_ch = inch() & A_CHARTEXT;
        switch (ch) {
        case ' ':
            /* Keep virtual char unchanged (do not print the space) */
            if (addch(v_ch) == ERR)
                return GEN_ERROR;

            break;
        case '\n':
            /* Indent */
            getyx(stdscr, c_y, c_x);
            if (move(c_y + 1, x) == ERR)
                return GEN_ERROR;

            break;
        default:
            if (addch(ch) == ERR)
                return GEN_ERROR;

            break;
        }
    }

    return OK;
}


int main(void)
{
    int ret = GEN_ERROR;
    int x;
    size_t man_y = 0, man_x = 0;
    size_t cloud_y = 6, cloud_x = 0;
    size_t cloud2_y = 16, cloud2_x = 8;

    char *man = " o \n" "<|>\n" "/\\ ";
    char *man_vert = " o \n" " V \n" " | ";
    char *cloud = "============\n";

    char *tornado = "\\##########/\n"
        " \\########/\n"
        "  \\######/\n" "   \\####/\n" "    \\##/\n" "     \\/";

    size_t tornado_y = 6, tornado_x;
    size_t y;

    char ch, ch2;
    size_t up = 0;
    int move_left, move_right;
    int on_floor = 0;

    if (initscr() == NULL)
        mgoto(clean_up);

    if (raw() == ERR)
        mgoto(clean_up);

    if (noecho() == ERR)
        mgoto(clean_up);

    if (keypad(stdscr, TRUE) == ERR)
        mgoto(clean_up);

    if (nodelay(stdscr, TRUE) == ERR)
        mgoto(clean_up);

    if (set_tabsize(8) == ERR)
        mgoto(clean_up);


    getmaxyx(stdscr, y, tornado_x);

    tornado_x -= 20;

    while (1) {
        if (erase())
            mgoto(clean_up);

        if (print_object(cloud_y, cloud_x, cloud))
            mgoto(clean_up);

        if (print_object(cloud2_y, cloud2_x, cloud))
            mgoto(clean_up);

        if (print_object(tornado_y, tornado_x, tornado))
            mgoto(clean_up);

        if (man_x % 2) {
            if (print_object(man_y, man_x, man_vert))
                mgoto(clean_up);
        } else {
            if (print_object(man_y, man_x, man))
                mgoto(clean_up);
        }

        if (milli_sleep(100))
            mgoto(clean_up);

        if (move(man_y + 3, man_x))
            mgoto(clean_up);

        ch = inch() & A_CHARTEXT;

        if (move(man_y + 3, man_x + 1))
            mgoto(clean_up);

        ch2 = inch() & A_CHARTEXT;

        if (ch == '=' || ch2 == '=')
            on_floor = 1;
        else
            on_floor = 0;

        if (up) {
            --man_y;
            --up;
        } else if (!on_floor) {
            /* Fall */
            ++man_y;
        }

        --tornado_x;

        move(0, 0);

        if (refresh())
            mgoto(clean_up);

        move_left = 0;
        move_right = 0;

        while ((x = getch()) != ERR) {
            /*
             * Consume all keyboard input that accumulated while the process
             * was asleep.
             */
            if (x == 'f' || x == KEY_RIGHT) {
                move_left = 0;
                move_right = 1;
            } else if (x == 'b' || x == KEY_LEFT) {
                move_right = 0;
                move_left = 1;
            } else if (x == KEY_UP) {
                if (on_floor)
                    up += 6;
            } else if (x == 'q' || (x == 24 && getch() == 3)) {
                goto quit;
            }
        }

        if (move_right)
            ++man_x;
        else if (move_left)
            --man_x;
    }


  quit:

    ret = 0;

  clean_up:
    if (endwin() == ERR)
        ret = GEN_ERROR;

    return ret;
}
