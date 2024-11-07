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

/* These can be adjusted to make the game easier or harder */
#define INIT_DRONE_HEALTH 20
#define INIT_ZOMBIE_HEALTH 10
#define INIT_BIRD_HEALTH 10
#define INIT_MAN_HEALTH 60

#define LAST_LEVEL 3

#define FALL_THROUGH_DAMAGE 4

/* Changes the speed of the game */
#define MILLISECONDS_PER_INTERVAL 50

/* Spawn probabilities */
#define CLOUD_PROB 30
#define GROUND_CLOUD_PROB 30
#define TORNADO_PROB 10
#define DRONE_PROB 10
#define BIRD_PROB 10
#define ZOMBIE_PROB 10
#define COIN_PROB 10
/* Out of */
#define PROB_MAX_INCLUSIVE 999


/* Object dimensions */
#define CLOUD_HEIGHT 5
#define CLOUD_WIDTH 12
#define TORNADO_HEIGHT 6
#define TORNADO_WIDTH 12
#define DRONE_HEIGHT 3
#define DRONE_WIDTH 9
#define DRONE_TORCH_X_OFFSET 7
#define BIRD_HEIGHT 3
#define BIRD_WIDTH 3
#define ZOMBIE_HEIGHT 3
#define ZOMBIE_WIDTH 4
#define MAN_HEIGHT 3
/* The width of the man varies */
#define MAN_MAX_WIDTH 3
#define COIN_HEIGHT 1
#define COIN_WIDTH 1
#define GAME_OVER_STR_WIDTH 56


/* Trajectories */

/* Simulates gravity */
char *jump_traj = "dddddddd__d__d__d____d____i__i__i__iiiiI";
char *fall_traj = "____i__i__i__iiiiI";

char *tornado_traj = "___d___d___dL";
char *bird_traj = "ddddL";
char *zombie_traj = "___i___i___iL";

char *drone_y_traj = "___i___i___i___i___iL";
char *drone_x_traj = "___i___i___i___i___iL";


/* Object in game */
struct obj {
    unsigned int y;             /* Vertical position */
    unsigned int x;             /* Horizontal position */
    char *y_traj;               /* Vertical trajectory */
    size_t y_ti;                /* Index in the vertical trajectory */
    char *x_traj;               /* Horizontal trajectory */
    size_t x_ti;                /* Index in the horizontal trajectory */
    /*
     * 'c' = cloud, 't' = tornado, 'd' = drone, 'b' = bird, 'z' = zombie,
     * 'i' = coin, 'm' = man.
     */
    char type;
    int health;                 /* Health level. 0 is the lowest. */
    struct obj *prev;           /* Link to previous node */
    struct obj *next;           /* Link to next node */
};

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
    } else if (type == 'd') {
        obj_h = DRONE_HEIGHT;
        obj_w = DRONE_WIDTH;
    } else if (type == 'b') {
        obj_h = BIRD_HEIGHT;
        obj_w = BIRD_WIDTH;
    } else if (type == 'z') {
        obj_h = ZOMBIE_HEIGHT;
        obj_w = ZOMBIE_WIDTH;
    } else if (type == 'm') {
        obj_h = MAN_HEIGHT;
        obj_w = MAN_MAX_WIDTH;
    } else if (type == 'i') {
        obj_h = COIN_HEIGHT;
        obj_w = COIN_WIDTH;
    } else {
        mreturn(GEN_ERROR);
    }

    if ((b = init_obj()) == NULL)
        mreturn(GEN_ERROR);

    /* Vertical */
    if (type == 'g') {
        /* Ground cloud */
        b->y = h - obj_h;
    } else if (type == 'd' || type == 'z' || type == 'm') {
        /* Spawn at the top of the sreen */
        b->y = 0;
    } else {
        if (random_num(h - obj_h, &b->y))
            mreturn(GEN_ERROR);
    }

    /* Horizontal */
    if (type == 't' || type == 'b') {
        /* Spawn in at right-hand side */
        b->x = w - obj_w - 1;
    } else if (type == 'm') {
        /* Spawn in at left-hand side */
        b->x = 0;
    } else {
        if (random_num(w - obj_w - 1, &b->x))
            mreturn(GEN_ERROR);
    }

    /* Default trajectories */
    switch (type) {
    case 't':
        b->x_traj = tornado_traj;
        break;
    case 'd':
        b->y_traj = drone_y_traj;
        b->x_traj = drone_x_traj;
        break;
    case 'b':
        b->x_traj = bird_traj;
        break;
    case 'z':
        b->x_traj = zombie_traj;
        break;
    }

    /* Set health */
    switch (type) {
    case 'd':
        b->health = INIT_DRONE_HEALTH;
        break;
    case 'b':
        b->health = INIT_BIRD_HEALTH;
        break;
    case 'z':
        b->health = INIT_ZOMBIE_HEALTH;
        break;
    case 'm':
        b->health = INIT_MAN_HEALTH;
        break;
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

int print_object(size_t y, size_t x, const char *object, char type,
                 int *health)
{
    chtype z, v_hl;
    char ch, v_ch;
    size_t c_y, c_x, torch_y;

    if (move(y, x) == ERR)
        return GEN_ERROR;

    while ((ch = *object++) != '\0') {
        z = inch();
        v_ch = z & A_CHARTEXT;
        v_hl = (z & A_ATTRIBUTES) & A_STANDOUT;

        switch (ch) {
        case ' ':
            /* Keep virtual char unchanged (do not print the space) */
            if (v_hl)
                standout();

            if (addch(v_ch) == ERR)
                return GEN_ERROR;

            if (v_hl)
                standend();

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
            /*
             * Coins restore the man to full health.
             * Only the man can get the coins.
             */
            if (type == 'm' && v_ch == '$')
                *health = INIT_MAN_HEALTH;

            /*
             * Highlighting or objects besides clouds and the health indicator
             * reduce health.
             */
            if (*health && (v_hl || !
                            (v_ch == ' ' || v_ch == '*' || v_ch == '.'
                             || v_ch == '`' || v_ch == '~' || v_ch == '('
                             || v_ch == ')')))
                -- * health;

            if (v_hl)
                standout();

            if (addch(ch) == ERR)
                return GEN_ERROR;

            if (v_hl)
                standend();

            break;
        }
    }

    if (type == 'd') {
        /* Draw search light */
        getyx(stdscr, c_y, c_x);
        torch_y = c_y;
        standout();
        while (1) {
            v_ch = inch() & A_CHARTEXT;

            /* Keep virtual char unchanged (just highlight) */
            if (addch(v_ch) == ERR)
                return GEN_ERROR;

            getyx(stdscr, c_y, c_x);
            if (c_y != torch_y)
                break;
        }
        standend();
    }

    return OK;
}

void print_obj_list(struct obj **head, char type, int *new_screen)
{
    char *cloud_str = "  .~~~~~~.\n"
        " (        )\n" "(          )\n" " (        )\n" "  `~~~~~~`";

    char *tornado_str = "\\##########/\n"
        " \\########/\n"
        "  \\######/\n" "   \\####/\n" "    \\##/\n" "     \\/";

    char *drone_str = "x x   x x\n" "|_|___|_|\n" "   |_|";

    char *bird_str = " /\n" "<-K\n" " \\";

    char *zombie_str = "[:]\n" " |==\n" "/\\";
    char *zombie_vert_str = "[:]\n" " |==\n" " |";

    char *coin_str = "$";

    char *man_str = " o\n" "<|>\n" "/\\";
    char *man_vert_str = " o\n" " V\n" " |";

    char *obj_str;
    struct obj *b, *t;
    int remove;
    char ch, ch2;
    int off_bottom_of_screen, on_floor;

    b = *head;

    if (*head == NULL)
        return;

    switch (type) {
    case 'c':
        obj_str = cloud_str;
        break;
    case 't':
        obj_str = tornado_str;
        break;
    case 'd':
        obj_str = drone_str;
        break;
    case 'b':
        obj_str = bird_str;
        break;
    case 'z':
        if (b->x % 2)
            obj_str = zombie_vert_str;
        else
            obj_str = zombie_str;

        break;
    case 'i':
        obj_str = coin_str;
        break;
    case 'm':
        if (b->x % 2)
            obj_str = man_vert_str;
        else
            obj_str = man_str;

        break;
    default:
        return;
    }

    while (b != NULL) {
        remove = 0;
        if (print_object(b->y, b->x, obj_str, type, &b->health)) {
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

        if (!b->health
            && (type == 'd' || type == 'b' || type == 'z' || type == 'm'))
            remove = 1;

        if (!remove && (type == 'z' || type == 'm')) {
            /*
             * The feet of the man and zombies are in the same position
             * relative to the top right of the object. Hence, the same
             * code can be used to see if they are standing on a cloud.
             */
            off_bottom_of_screen = 0;
            ch = '\0';
            if (b->x % 2) {
                /* Under back foot */
                if (move(b->y + 3, b->x))
                    off_bottom_of_screen = 1;

                ch = inch() & A_CHARTEXT;
            }

            /* Under front foot, or feet together */
            if (!off_bottom_of_screen && move(b->y + 3, b->x + 1))
                off_bottom_of_screen = 1;

            if (off_bottom_of_screen) {
                /* Off the bottom of the screen */
                if (b->health > FALL_THROUGH_DAMAGE)
                    b->health -= FALL_THROUGH_DAMAGE;
                else
                    b->health = 0;

                /* Move to the top of the screen */
                b->y = 0;
                b->y_traj = fall_traj;
                b->y_ti = 0;
            }

            ch2 = inch() & A_CHARTEXT;

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
                b->y_traj = NULL;
                b->y_ti = 0;
            } else {
                if (b->y_traj == NULL) {
                    b->y_traj = fall_traj;
                    b->y_ti = 0;
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

int remove_used_coins(struct obj **coin_head)
{
    char ch;
    struct obj *b, *t;

    b = *coin_head;

    if (*coin_head == NULL)
        return 0;               /* Nothing to do */

    while (b != NULL) {
        if (move(b->y, b->x) == ERR)
            mreturn(GEN_ERROR);

        ch = inch() & A_CHARTEXT;

        if (ch != '$') {
            /* Remove from list */
            if (b->prev != NULL)
                b->prev->next = b->next;

            if (b->next != NULL)
                b->next->prev = b->prev;

            /* Replace the head if it was the head */
            if (b == *coin_head)
                *coin_head = b->next;

            t = b;
            b = b->next;

            free(t);
        } else {
            b = b->next;
        }
    }
    return 0;
}

int main(void)
{
    int ret = GEN_ERROR;
    int k;

    struct obj *cloud_head = NULL;
    struct obj *tornado_head = NULL;
    struct obj *drone_head = NULL;
    struct obj *bird_head = NULL;
    struct obj *zombie_head = NULL;
    struct obj *coin_head = NULL;
    struct obj *man = NULL;

    int tmp;
    int level = 1;

    int h, w;

    int move_up, move_left, move_right;
    int init_num_tornadoes, init_num_clouds;
    int i, new_screen;

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

    if (curs_set(0) == ERR)
        mgoto(clean_up);

    getmaxyx(stdscr, h, w);

    /* Check minimum screen size */
    if (h < TORNADO_HEIGHT || w < GAME_OVER_STR_WIDTH)
        mgoto(clean_up);

    /* Man */
    if (spawn_obj(&man, h, w, 'm'))
        mgoto(clean_up);

  set_up_screen:

    free_obj_list(cloud_head);
    cloud_head = NULL;
    free_obj_list(tornado_head);
    tornado_head = NULL;
    free_obj_list(drone_head);
    drone_head = NULL;
    free_obj_list(bird_head);
    bird_head = NULL;
    free_obj_list(zombie_head);
    zombie_head = NULL;
    free_obj_list(coin_head);
    coin_head = NULL;

    init_num_clouds = w / (CLOUD_WIDTH + 6);
    if (!init_num_clouds)
        init_num_clouds = 1;

    /* Spawn a cloud in the bottom left to make it easier */
    if (spawn_obj(&cloud_head, h, CLOUD_WIDTH + 1, 'c'))
        mgoto(clean_up);

    cloud_head->y = h - CLOUD_HEIGHT;


    /* Minus one because already spawned one cloud */
    for (i = 0; i < init_num_clouds - 1; ++i)
        if (spawn_obj(&cloud_head, h, w, 'c'))
            mgoto(clean_up);

    init_num_tornadoes = h / (TORNADO_HEIGHT + MAN_HEIGHT + 1);
    if (!init_num_tornadoes)
        init_num_tornadoes = 1;

    for (i = 0; i < init_num_tornadoes; ++i)
        if (spawn_obj(&tornado_head, h, w, 't'))
            mgoto(clean_up);

    while (1) {
        if (!man->health) {
          game_over:

            move(0, 0);

            if (clrtoeol())
                mgoto(clean_up);

            if (print_object(0, 0, game_over_str, '_', &tmp))
                mgoto(clean_up);

            if (refresh())
                mgoto(clean_up);

            if (milli_sleep(2000))
                mgoto(clean_up);

            goto quit;
        }

        if (erase())
            mgoto(clean_up);

        getmaxyx(stdscr, h, w);

        for (i = 0; i < man->health; ++i)
            if (addch('*') == ERR)
                mgoto(clean_up);

        print_obj_list(&cloud_head, 'c', &new_screen);
        print_obj_list(&tornado_head, 't', &new_screen);
        print_obj_list(&drone_head, 'd', &new_screen);
        print_obj_list(&bird_head, 'b', &new_screen);
        print_obj_list(&zombie_head, 'z', &new_screen);
        print_obj_list(&coin_head, 'i', &new_screen);

        new_screen = 0;

        print_obj_list(&man, 'm', &new_screen);

        if (man == NULL)
            goto game_over;

        if (new_screen) {
            new_screen = 0;
            if (level == LAST_LEVEL) {
                /* Win */
                if (print_object(0, 0, win_str, '_', &tmp))
                    mgoto(clean_up);

                if (refresh())
                    mgoto(clean_up);

                if (milli_sleep(2000))
                    mgoto(clean_up);

                goto quit;
            }

            ++level;
            goto set_up_screen;
        }

        /* Check for used coins, and remove them */
        if (remove_used_coins(&coin_head))
            mgoto(clean_up);

        if (refresh())
            mgoto(clean_up);

        if (milli_sleep(MILLISECONDS_PER_INTERVAL))
            mgoto(clean_up);


        move_up = 0;
        move_left = 0;
        move_right = 0;

        while ((k = getch()) != ERR) {
            /*
             * Consume all keyboard input that accumulated while the process
             * was asleep.
             */
            switch (k) {
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
            case 'Q':
                goto quit;
            case 'p':
            case 'P':
                /* Pause/play toggle */
                while (1)
                    if ((k = getch()) == 'p' || k == 'P')
                        break;
                    else if (k == 'q' || k == 'Q')
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

        if (move_up && man->y_traj == NULL) {
            man->y_traj = jump_traj;
            man->y_ti = 0;
        }


        /* Spawn new objects */
        if (random_num(PROB_MAX_INCLUSIVE, &roll))
            mgoto(clean_up);

        if (roll < CLOUD_PROB && spawn_obj(&cloud_head, h, w, 'c'))
            mgoto(clean_up);

        if (random_num(PROB_MAX_INCLUSIVE, &roll))
            mgoto(clean_up);

        if (roll < GROUND_CLOUD_PROB && spawn_obj(&cloud_head, h, w, 'g'))
            mgoto(clean_up);

        if (random_num(PROB_MAX_INCLUSIVE, &roll))
            mgoto(clean_up);

        if (roll < TORNADO_PROB && spawn_obj(&tornado_head, h, w, 't'))
            mgoto(clean_up);

        if (level == 1) {
            if (random_num(PROB_MAX_INCLUSIVE, &roll))
                mgoto(clean_up);

            if (roll < BIRD_PROB && spawn_obj(&bird_head, h, w, 'b'))
                mgoto(clean_up);
        }
        if (level == 2) {
            if (random_num(PROB_MAX_INCLUSIVE, &roll))
                mgoto(clean_up);

            if (roll < DRONE_PROB && spawn_obj(&drone_head, h, w, 'd'))
                mgoto(clean_up);
        }
        if (level == 3) {
            if (random_num(PROB_MAX_INCLUSIVE, &roll))
                mgoto(clean_up);

            if (roll < ZOMBIE_PROB && spawn_obj(&zombie_head, h, w, 'z'))
                mgoto(clean_up);
        }

        if (random_num(PROB_MAX_INCLUSIVE, &roll))
            mgoto(clean_up);

        if (roll < COIN_PROB && spawn_obj(&coin_head, h, w, 'i'))
            mgoto(clean_up);
    }


  quit:

    ret = 0;

    free_obj_list(cloud_head);
    free_obj_list(tornado_head);
    free_obj_list(drone_head);
    free_obj_list(bird_head);
    free_obj_list(zombie_head);
    free_obj_list(coin_head);
    free_obj_list(man);

  clean_up:
    if (endwin() == ERR)
        ret = GEN_ERROR;

    return ret;
}
