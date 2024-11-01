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

#define MILLISECONDS_PER_INTERVAL 50

#define PROB_MAX_INCLUSIVE 999
#define CLOUD_PROB 30
#define GROUND_CLOUD_PROB 30
#define TORNADO_PROB 10

#define CLOUD_HEIGHT 5
#define CLOUD_WIDTH 12
#define TORNADO_HEIGHT 6
#define TORNADO_WIDTH 12
#define MAN_HEIGHT 3

#define GAME_OVER_STR_WIDTH 56

#define INIT_HEALTH 60
#define LAST_LEVEL 2

    /* Trajectories */
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

int spawn_obj(struct obj **head, int h, int w, char type)
{
    struct obj *b;
    int obj_h, obj_w;

    if (head == NULL || !h || !w)
        mreturn(GEN_ERROR);

    if (type == 'c' || type == 'g') {
        obj_h = CLOUD_HEIGHT;
        obj_w = CLOUD_WIDTH;
    } else if (type == 't') {
        obj_h = TORNADO_HEIGHT;
        obj_w = TORNADO_WIDTH;
    } else {
        mreturn(GEN_ERROR);
    }

    if ((b = init_obj()) == NULL)
        mreturn(GEN_ERROR);

    if (type == 'g') {
        /* Ground cloud */
        b->y = h - obj_h;
    } else {
        if (random_num(h - obj_h, &b->y))
            mreturn(GEN_ERROR);
    }

    if (type == 't') {
        b->x = w - obj_w - 1;
    } else {
        if (random_num(w - obj_w - 1, &b->x))
            mreturn(GEN_ERROR);
    }

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


void print_obj_list(struct obj **head, char type, int *on_tornado,
                    int *new_screen)
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
            if (type == 'm') {
                /* Off right-hand side of screen. Start new screen. */
                b->x = 0;
                *new_screen = 1;
                return;
            } else {
                remove = 1;
            }
        } else {
            /* Move */
            if (process_trajectory(&b->y_traj, &b->y_ti, &b->y)) {
                /* Hit top of screen, so fall */
                if (type == 'm') {
                    b->y_traj = fall_traj;
                    b->y_ti = 0;
                } else {
                    remove = 1;
                }
            }

            if (process_trajectory(&b->x_traj, &b->x_ti, &b->x)) {
                /* Cannot move left off the screen */
                if (type == 'm') {
                    b->x_traj = NULL;
                    b->x_ti = 0;
                } else {
                    remove = 1;
                }
            }
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

    int health = INIT_HEALTH;
    int level = 1;

    int h, w;
    char ch, ch2;
    int move_up, move_left, move_right;
    int on_floor = 0;
    int init_num_tornadoes, init_num_clouds;
    int i, on_tornado, new_screen;

    unsigned int roll;

    /*
     * figlet -f standard 'GAME OVER' | head -n -1 \
     *     | sed -E -e 's_\\_\\\\_g' -e 's_^_"_' -e 's_$_\\n"_'
     */
    char *game_over_str =
        "  ____    _    __  __ _____    _____     _______ ____  \n"
        " / ___|  / \\  |  \\/  | ____|  / _ \\ \\   / / ____|  _ \\ \n"
        "| |  _  / _ \\ | |\\/| |  _|   | | | \\ \\ / /|  _| | |_) |\n"
        "| |_| |/ ___ \\| |  | | |___  | |_| |\\ V / | |___|  _ < \n"
        " \\____/_/   \\_\\_|  |_|_____|  \\___/  \\_/  |_____|_| \\_\\\n";

    /*
     * figlet -f standard 'YOU WIN!' | head -n -1 \
     *     | sed -E -e 's_\\_\\\\_g' -e 's_^_"_' -e 's_$_\\n"_'
     */
    char *win_str =
        "__   _____  _   _  __        _____ _   _ _ \n"
        "\\ \\ / / _ \\| | | | \\ \\      / /_ _| \\ | | |\n"
        " \\ V / | | | | | |  \\ \\ /\\ / / | ||  \\| | |\n"
        "  | || |_| | |_| |   \\ V  V /  | || |\\  |_|\n"
        "  |_| \\___/ \\___/     \\_/\\_/  |___|_| \\_(_)\n";


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

    /* Check minimum screen size */
    if (h < TORNADO_HEIGHT || w < GAME_OVER_STR_WIDTH)
        mgoto(clean_up);

    if ((man = init_obj()) == NULL)
        mgoto(clean_up);

    man->y_traj = fall_traj;
    man->y_ti = 0;

  set_up_screen:

    free_obj_list(cloud_head);
    cloud_head = NULL;
    free_obj_list(tornado_head);
    tornado_head = NULL;

    init_num_clouds = w / (CLOUD_WIDTH + 6);
    if (!init_num_clouds)
        init_num_clouds = 1;

    /* Spawn a cloud in the bottom left to make it easier */
    spawn_obj(&cloud_head, h, CLOUD_WIDTH + 1, 'c');
    cloud_head->y = h - CLOUD_HEIGHT;

    /* Minus one because already spawned one cloud */
    for (i = 0; i < init_num_clouds - 1; ++i)
        spawn_obj(&cloud_head, h, w, 'c');

    init_num_tornadoes = h / (TORNADO_HEIGHT + MAN_HEIGHT + 1);
    if (!init_num_tornadoes)
        init_num_tornadoes = 1;

    for (i = 0; i < init_num_tornadoes; ++i) {
        spawn_obj(&tornado_head, h, w, 't');
        tornado_head->x_traj = tornado_traj;
        tornado_head->x_ti = 0;
    }

    while (1) {
      top:
        if (!health) {
          game_over:

            move(0, 0);

            if (clrtoeol())
                mgoto(clean_up);

            if (print_object(0, 0, game_over_str, &on_tornado))
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

        print_obj_list(&cloud_head, 'c', &on_tornado, &new_screen);

        print_obj_list(&tornado_head, 't', &on_tornado, &new_screen);

        on_tornado = 0;
        new_screen = 0;

        print_obj_list(&man, 'm', &on_tornado, &new_screen);
        if (new_screen) {
            new_screen = 0;
            if (level == LAST_LEVEL) {
                /* Win */
                if (print_object(0, 0, win_str, &on_tornado))
                    mgoto(clean_up);

                move(0, 0);

                if (refresh())
                    mgoto(clean_up);

                if (milli_sleep(2000))
                    mgoto(clean_up);

                goto quit;
            }

            ++level;
            goto set_up_screen;
        }

        if (man == NULL)
            goto game_over;

        if (on_tornado)
            --health;

        ch = '\0';
        if (man->x % 2) {
            /* Under back foot */
            if (move(man->y + 3, man->x))
                goto off_bottom_of_screen;

            ch = inch() & A_CHARTEXT;
        }

        /* Under front foot, or feet together */
        if (move(man->y + 3, man->x + 1))
            goto off_bottom_of_screen;

        ch2 = inch() & A_CHARTEXT;

        move(0, 0);

        if (refresh())
            mgoto(clean_up);

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
            if (man->y_traj == NULL) {
                man->y_traj = fall_traj;
                man->y_ti = 0;
            }
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

        if (move_right) {
            ++man->x;
        } else if (move_left) {
            if (man->x)
                --man->x;
        }

        if (move_up && on_floor) {
            man->y_traj = jump_traj;
            man->y_ti = 0;
        }


        /* Spawn new objects */
        if (random_num(PROB_MAX_INCLUSIVE, &roll))
            mgoto(clean_up);

        if (roll < CLOUD_PROB)
            spawn_obj(&cloud_head, h, w, 'c');

        if (random_num(PROB_MAX_INCLUSIVE, &roll))
            mgoto(clean_up);

        if (roll < GROUND_CLOUD_PROB)
            spawn_obj(&cloud_head, h, w, 'g');

        if (random_num(PROB_MAX_INCLUSIVE, &roll))
            mgoto(clean_up);

        if (roll < TORNADO_PROB) {
            spawn_obj(&tornado_head, h, w, 't');
            tornado_head->x_traj = tornado_traj;
            tornado_head->x_ti = 0;
        }

        goto top;

      off_bottom_of_screen:
        /* Man off the bottom of the screen */
        if (health > 4)
            health -= 4;
        else
            health = 0;

        /* Move to the top */
        man->y = 0;
        man->y_traj = fall_traj;
        man->y_ti = 0;
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
