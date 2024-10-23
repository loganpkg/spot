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



char *jump_traj = "dddddddd__d__d__d____d____i__i__i__iiiiI";
char *fall_traj = "____i__i__i__iiiiI";
char *tornado_traj = "___d___d___dL";

/* Object in game */
struct obj {
    unsigned int y;             /* Vertical position */
    unsigned int x;             /* Horizontal position */
    char *y_traj;               /* Vertical trajectory */
    size_t y_ti;                /* Index in the vertical trajectory */
    char *x_traj;               /* Horizontal trajectory */
    size_t x_ti;                /* Index in the horizontal trajectory */
    char type;                  /* 'c' = cloud, 't' = tornado, 'm' = man */
    struct obj *prev;           /* Link to previous node */
    struct obj *next;           /* Link to next node */
};

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

            /* Silence compiler warning about unused variable */
            if (x > c_x)
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

struct obj *init_obj(void)
{
    struct obj *b;
    if ((b = calloc(1, sizeof(struct obj))) == NULL)
        mreturn(NULL);

    return b;
}

void free_obj_list(struct obj *b)
{
    struct obj *t;
    while (b != NULL) {
        t = b->next;
        free(b);
        b = t;
    }
}


#define CLOUD_HEIGHT 5
#define CLOUD_WIDTH 12


int spawn_obj(struct obj **head, int obj_h, int obj_w, int h, int w)
{
    struct obj *b;

    if (head == NULL || !h || !w)
        mreturn(GEN_ERROR);

    if ((b = init_obj()) == NULL)
        mreturn(GEN_ERROR);

    if (random_num(h - obj_h, &b->y))
        mreturn(GEN_ERROR);

    if (random_num(w - obj_w, &b->x))
        mreturn(GEN_ERROR);

    /* Link in as the new head */
    if (*head != NULL) {
        (*head)->prev = b;
        b->next = *head;
    }
    *head = b;
    return 0;
}


int process_trajectory(char **traj, size_t *index, unsigned int *coordin)
{
    if (*traj != NULL) {
        switch (*(*traj + *index)) {
        case 'd':
            if (!*coordin)
                return GEN_ERROR;

            --*coordin;
            ++*index;
            break;
        case 'i':
            ++*coordin;
            ++*index;
            break;
        case 'I':
            /* Terminal velocity. Do not advance in trajectory. */
            ++*coordin;
            break;
        case 'L':
            /* Loop back to the start of the trajectory */
            *index = 0;
            break;
        case '\0':
            /* End of trajectory */
            *traj = NULL;
            *index = 0;
            break;
        default:
            ++*index;           /* Eat char */
            break;
        }
    }
    return 0;
}


void print_obj_list(struct obj **head, char type, int *on_tornado)
{
    char *cloud_str = "  .~~~~~~.\n"
        " (        )\n" "(          )\n" " (        )\n" "  `~~~~~~`";

    char *tornado_str = "\\##########/\n"
        " \\########/\n"
        "  \\######/\n" "   \\####/\n" "    \\##/\n" "     \\/";

    char *man = " o\n" "<|>\n" "/\\";
    char *man_vert = " o\n" " V\n" " |";

    char *obj_str;
    struct obj *b, *t;
    int remove;

    b = *head;

    if (type == 't') {
        obj_str = tornado_str;
    } else if (type == 'm') {
        if (b->x % 2)
            obj_str = man_vert;
        else
            obj_str = man;
    } else {
        obj_str = cloud_str;    /* Default */
    }

    while (b != NULL) {
        remove = 0;
        if (print_object(b->y, b->x, obj_str, on_tornado)) {
            remove = 1;
        } else {
            /* Move */
            if (process_trajectory(&b->y_traj, &b->y_ti, &b->y))
                remove = 1;

            if (process_trajectory(&b->x_traj, &b->x_ti, &b->x))
                remove = 1;
        }

        if (remove) {
            /* Remove from list */
            if (b->prev != NULL)
                b->prev->next = b->next;

            if (b->next != NULL)
                b->next->prev = b->prev;

            /* Replace the head if it was the head */
            if (b == *head)
                *head = b->next;

            t = b;
            b = b->next;

            free(t);
        } else {
            b = b->next;
        }
    }
}

int main(void)
{
    int ret = GEN_ERROR;
    int x;

    struct obj *cloud_head = NULL;
    struct obj *tornado_head = NULL;
    struct obj *man = NULL;

#define TORNADO_HEIGHT 6
#define TORNADO_WIDTH 12


    /*
     * figlet -f standard 'GAME OVER' \
     *     | sed -E -e 's_\\_\\\\_g' -e 's_^_"_' -e 's_$_\\n"_'
     */
    char *game_over =
        "  ____    _    __  __ _____    _____     _______ ____  \n"
        " / ___|  / \\  |  \\/  | ____|  / _ \\ \\   / / ____|  _ \\ \n"
        "| |  _  / _ \\ | |\\/| |  _|   | | | \\ \\ / /|  _| | |_) |\n"
        "| |_| |/ ___ \\| |  | | |___  | |_| |\\ V / | |___|  _ < \n"
        " \\____/_/   \\_\\_|  |_|_____|  \\___/  \\_/  |_____|_| \\_\\\n"
        "                                                       \n";

    int h, w;

    char ch, ch2;
    int move_up, move_left, move_right;
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

    spawn_obj(&cloud_head, CLOUD_HEIGHT, CLOUD_WIDTH, h, w);
    spawn_obj(&cloud_head, CLOUD_HEIGHT, CLOUD_WIDTH, h, w);
    spawn_obj(&cloud_head, CLOUD_HEIGHT, CLOUD_WIDTH, h, w);
    spawn_obj(&cloud_head, CLOUD_HEIGHT, CLOUD_WIDTH, h, w);
    spawn_obj(&cloud_head, CLOUD_HEIGHT, CLOUD_WIDTH, h, w);
    spawn_obj(&cloud_head, CLOUD_HEIGHT, CLOUD_WIDTH, h, w);
    spawn_obj(&cloud_head, CLOUD_HEIGHT, CLOUD_WIDTH, h, w);
    spawn_obj(&cloud_head, CLOUD_HEIGHT, CLOUD_WIDTH, h, w);
    spawn_obj(&cloud_head, CLOUD_HEIGHT, CLOUD_WIDTH, h, w);
    spawn_obj(&cloud_head, CLOUD_HEIGHT, CLOUD_WIDTH, h, w);

    spawn_obj(&tornado_head, TORNADO_HEIGHT, TORNADO_WIDTH, h, w);
    tornado_head->x_traj = tornado_traj;
    spawn_obj(&tornado_head, TORNADO_HEIGHT, TORNADO_WIDTH, h, w);
    tornado_head->x_traj = tornado_traj;
    spawn_obj(&tornado_head, TORNADO_HEIGHT, TORNADO_WIDTH, h, w);
    tornado_head->x_traj = tornado_traj;
    spawn_obj(&tornado_head, TORNADO_HEIGHT, TORNADO_WIDTH, h, w);
    tornado_head->x_traj = tornado_traj;


    if ((man = init_obj()) == NULL)
        mgoto(clean_up);

    man->y_traj = fall_traj;

    while (1) {
        if (!health) {
          game_over:
            if (print_object(0, 0, game_over, &on_tornado))
                mgoto(clean_up);

            move(0, 0);

            if (refresh())
                mgoto(clean_up);

            if (milli_sleep(2000))
                mgoto(clean_up);

            goto quit;
        }

        if (erase())
            mgoto(clean_up);

        getmaxyx(stdscr, h, w);

        for (i = 0; i < health; ++i)
            if (addch('*') == ERR)
                mgoto(clean_up);

        print_obj_list(&cloud_head, 'c', &on_tornado);

        print_obj_list(&tornado_head, 't', &on_tornado);

        on_tornado = 0;

        print_obj_list(&man, 'm', &on_tornado);

        if (man == NULL)
            goto game_over;

        if (on_tornado)
            --health;

        ch = '\0';
        if (man->x % 2) {
            /* Under back foot */
            if (move(man->y + 3, man->x))
                goto game_over;

            ch = inch() & A_CHARTEXT;
        }

        /* Under front foot, or feet together */
        if (move(man->y + 3, man->x + 1))
            goto game_over;

        ch2 = inch() & A_CHARTEXT;

        move(0, 0);

        if (refresh())
            mgoto(clean_up);

#define MILLISECONDS_PER_INTERVAL 50

        if (milli_sleep(MILLISECONDS_PER_INTERVAL))
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

        if (on_floor) {
            man->y_traj = NULL;
            man->y_ti = 0;
        } else {
            if (man->y_traj == NULL)
                man->y_traj = fall_traj;
        }

        move_up = 0;
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
                move_up = 1;
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
            ++man->x;
        else if (move_left)
            --man->x;

        if (move_up && on_floor)
            man->y_traj = jump_traj;

    }


  quit:

    ret = 0;

    free_obj_list(cloud_head);
    free_obj_list(tornado_head);
    free_obj_list(man);

  clean_up:
    if (endwin() == ERR)
        ret = GEN_ERROR;

    return ret;
}
