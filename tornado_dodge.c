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


int print_object(size_t y, size_t x, const char *object, int *on_tornado)
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
            if (v_ch == '\\' || v_ch == '#' || v_ch == '/')
                *on_tornado = 1;

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
    char *cloud = "  .~~~~~~.  \n"
        " (        ) \n"
        "(          )\n" " (        ) \n" "  `~~~~~~`  \n";

    char *tornado = "\\##########/\n"
        " \\########/\n"
        "  \\######/\n" "   \\####/\n" "    \\##/\n" "     \\/";


    /*
     * figlet -f standard 'GAME OVER' \
     * | sed -E -e 's_\\_\\\\_g' -e 's_^_"_' -e 's_$_\\n"_'
     */
    char *game_over =
        "  ____    _    __  __ _____    _____     _______ ____  \n"
        " / ___|  / \\  |  \\/  | ____|  / _ \\ \\   / / ____|  _ \\ \n"
        "| |  _  / _ \\ | |\\/| |  _|   | | | \\ \\ / /|  _| | |_) |\n"
        "| |_| |/ ___ \\| |  | | |___  | |_| |\\ V / | |___|  _ < \n"
        " \\____/_/   \\_\\_|  |_|_____|  \\___/  \\_/  |_____|_| \\_\\\n"
        "                                                       \n";

    size_t tornado_y = 6, tornado_x;
    int h, w;

    char ch, ch2;
    size_t up = 0;
    int move_left, move_right;
    int on_floor = 0;

    int i, health = 10, on_tornado;

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


    getmaxyx(stdscr, h, w);

    tornado_x = w - 20;

    while (1) {
        if (erase())
            mgoto(clean_up);

        getmaxyx(stdscr, h, w);

        if (!health) {
            if (print_object(0, 0, game_over, &on_tornado))
                mgoto(clean_up);

            move(0, 0);

            if (refresh())
                mgoto(clean_up);

            if (milli_sleep(2000))
                mgoto(clean_up);

            goto quit;
        }

        for (i = 0; i < health; ++i)
            if (addch('*') == ERR)
                mgoto(clean_up);

        if (print_object(cloud_y, cloud_x, cloud, &on_tornado))
            mgoto(clean_up);

        if (print_object(cloud2_y, cloud2_x, cloud, &on_tornado))
            mgoto(clean_up);

        if (print_object(tornado_y, tornado_x, tornado, &on_tornado))
            mgoto(clean_up);

        on_tornado = 0;

        if (man_x % 2) {
            if (print_object(man_y, man_x, man_vert, &on_tornado))
                mgoto(clean_up);
        } else {
            if (print_object(man_y, man_x, man, &on_tornado))
                mgoto(clean_up);
        }

        if (on_tornado)
            --health;


        ch = '\0';
        if (man_x % 2) {
            /* Under back foot */
            if (move(man_y + 3, man_x))
                mgoto(clean_up);

            ch = inch() & A_CHARTEXT;
        }

        /* Under front foot, or feet together */
        if (move(man_y + 3, man_x + 1))
            mgoto(clean_up);

        ch2 = inch() & A_CHARTEXT;

        move(0, 0);

        if (refresh())
            mgoto(clean_up);

        if (milli_sleep(100))
            mgoto(clean_up);

        on_floor = 0;

        if (ch != '\0') {
            switch (ch) {
            case '~':
            case '(':
            case ')':
            case '.':
            case '`':
                on_floor = 1;
                break;
            }
        }

        if (!on_floor)
            switch (ch2) {
            case '~':
            case '(':
            case ')':
            case '.':
            case '`':
                on_floor = 1;
                break;
            };

        if (up) {
            --man_y;
            --up;
        } else if (!on_floor) {
            /* Fall */
            ++man_y;
        }

        --tornado_x;


        move_left = 0;
        move_right = 0;

        while ((x = getch()) != ERR) {
            /*
             * Consume all keyboard input that accumulated while the process
             * was asleep.
             */
            switch (x) {
            case 'f':
            case KEY_RIGHT:
                move_left = 0;
                move_right = 1;
                break;
            case 'b':
            case KEY_LEFT:
                move_right = 0;
                move_left = 1;
                break;
            case KEY_UP:
                if (on_floor)
                    up += 6;
                break;
            case 'q':
                goto quit;
            case 'p':
                /* Pause/play toggle */
                while (1)
                    if (getch() == 'p')
                        break;
                    else if (getch() == 'q')
                        goto quit;
                break;
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
