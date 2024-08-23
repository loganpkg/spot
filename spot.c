/*
 * Copyright (c) 2023 Logan Ryan McLintock
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

/*
 * spot: text editor.
 * Dedicated to my son who was only a 4mm "spot" in his first ultrasound.
 *
 * Jesus answered, "Those who drink this water will get thirsty again,
 * but those who drink the water that I will give them will never be thirsty
 * again. The water that I will give them will become in them a spring which
 * will provide them with life-giving water and give them eternal life."
 *                                                          John 4:13-14 GNT
 */


#include "toucanlib.h"
#include <curses.h>


#define INIT_GB_SIZE 512

#define ESC 27

#define clean_ch(ch) (isprint(ch) || (ch) == '\t' || (ch) == '\n' ? (ch) \
    : ((ch) == '\0' ? '~' : '?'))


int draw(struct gb *b, struct gb *cl, int cl_active, char **sb,
         size_t *sb_s, int *centre, int *clr, int rv, int es_set, int es)
{
    size_t up, target_up, ts, i;
    int have_centred = 0;
    unsigned char ch;
    char num_str[NUM_BUF_SIZE];
    int r, len;
    size_t h;                   /* Screen height */
    size_t w;                   /* Screen width */
    size_t s;                   /* Screen size */
    size_t y;                   /* Current vertical position on screen */
    size_t x;                   /* Current horizontal position on screen */
    size_t si;                  /* Current screen index */
    /* Final vertical cursor position on screen */
    size_t c_y;
    /* Final horizontal cursor position on screen */
    size_t c_x;
    char *tmp;


  start:
    if (*clr) {
        if (clear() == ERR)
            return ERROR;

        *clr = 0;

    } else {
        if (erase() == ERR)
            return ERROR;
    }

    getmaxyx(stdscr, h, w);
    s = h * w;                  /* Cannot overflow as already in memory */

    if (b->d > b->g || *centre) {
        /* Does not consider long lines that wrap */
        b->d = b->g;
        up = 0;
        target_up = h <= 4 ? 1 : (h - 1) / 2;
        while (b->d) {
            --b->d;
            if (*(b->a + b->d) == '\n')
                ++up;

            if (up == target_up) {
                ++b->d;
                break;
            }
        }
        *centre = 0;
        have_centred = 1;
    }

    /* Size of the text portion of the screen */
    ts = s - (h >= 2 ? w : 0) - (h >= 3 ? w : 0);

    standend();

    /* Region commenced before draw start */
    if (b->m_set && b->m < b->d)
        standout();

    /* Before the gap */
    i = b->d;
    getyx(stdscr, y, x);
    si = y * w + x;

    while (i < b->g && si < ts) {
        if (b->m_set && b->m == i)
            standout();

        ch = *((b)->a + i);
        addch(clean_ch(ch));
        ++i;

        getyx(stdscr, y, x);
        si = y * w + x;
    }

    if (si >= ts) {
        /* Off screen */
        if (!have_centred)
            *centre = 1;
        else
            b->d = b->g;
        goto start;
    }

    /* At the cursor */
    c_y = y;
    c_x = x;

    if (b->m_set) {
        if (b->m < b->c)
            standend();         /* End of region */
        else
            standout();         /* Start of region */
    }

    /* After the gap (from the cursor onwards) */
    i = b->c;
    getyx(stdscr, y, x);
    si = y * w + x;

    while (i <= b->e && si < ts) {
        if (b->m_set && b->m == i)
            standend();

        ch = *((b)->a + i);
        addch(clean_ch(ch));
        ++i;

        getyx(stdscr, y, x);
        si = y * w + x;
    }

    if (h >= 2) {
        /* Status bar */

        if (es_set) {
            r = snprintf(num_str, NUM_BUF_SIZE, "%d", es);
            if (r < 0 || r >= NUM_BUF_SIZE)
                return ERROR;
        } else {
            *num_str = '\0';
        }

        move(h - 2, 0);

        if (*sb_s < w) {
            if ((tmp = realloc(*sb, w + 1)) == NULL)
                return ERROR;

            *sb = tmp;
            *sb_s = w + 1;
        }

        len = snprintf(*sb, *sb_s, "%c%c %s (%lu,%lu) %02X %s",
                       rv ? '!' : ' ', b->mod ? '*' : ' ', b->fn,
                       (unsigned long) b->r, (unsigned long) b->col,
                       cl_active ? *(cl->a + cl->c) : *(b->a + b->c),
                       num_str);
        if (len < 0)
            return ERROR;

        if ((size_t) len < *sb_s) {
            memset(*sb + len, ' ', w - len);
            *(*sb + w) = '\0';
        }

        standout();

        if (addnstr(*sb, w) == ERR)
            return ERROR;

        standend();
    }

    if (h >= 3) {
        /* Command line */
      cl_start:
        standend();

        if (cl->m_set && cl->m < cl->d)
            standout();

        /* Start of last line in virtual screen */
        move(h - 1, 0);

        /* Erase virtual line */
        clrtoeol();

        if (cl->d > cl->g)
            cl->d = cl->g;

        /* Before the gap */
        i = cl->d;
        getyx(stdscr, y, x);
        si = y * w + x;

        while (i < cl->g && si < s) {
            if (cl->m_set && cl->m == i)
                standout();

            ch = *((cl)->a + i);
            addch(clean_ch(ch));
            ++i;

            getyx(stdscr, y, x);
            si = y * w + x;
        }

        if (si >= s) {
            /* Off screen */
            cl->d = cl->g;      /* Draw from cursor */
            goto cl_start;
        }

        /* At the cursor */
        if (cl_active) {
            c_y = y;
            c_x = x;
        }

        if (cl->m_set) {
            if (cl->m < cl->c)
                standend();     /* End of region */
            else
                standout();     /* Start of region */
        }

        /* After the gap (from the cursor onwards) */
        i = cl->c;
        while (i <= cl->e && si < s) {
            if (cl->m_set && cl->m == i)
                standend();

            ch = *((cl)->a + i);
            addch(clean_ch(ch));
            ++i;

            getyx(stdscr, y, x);
            si = y * w + x;
        }
    }

    move(c_y, c_x);
    refresh();

    return 0;
}


#define z (cl_active ? cl : b)

int main(int argc, char **argv)
{
    int ret = 0;                /* Return value of the text editor */
    int running = 1;            /* Indicates if the text editor is running */
    int rv = 0;                 /* Return value of last internal command */
    int es_set = 0;
    int es = 0;                 /* Exit status of last shell command */
    char *sb = NULL;            /* Memory for status bar */
    size_t sb_s = 0;            /* Size of status bar memory */
    int centre = 0;             /* Vertically centre cursor on the screen */
    int clr = 0;                /* Redraw physical screen */
    int i, x, y;
    struct gb *b = NULL;        /* Text buffers linked together */
    struct gb *p = NULL;        /* Paste buffer */
    struct gb *cl = NULL;       /* Command line buffer */
    struct gb *se = NULL;       /* Search buffer */
    struct gb *tmp = NULL;      /* Temporary buffer */
    struct gb *t;               /* For switching gap buffers */
    char search_type = ' ';     /* s = Exact search, z = Regex search */
    int cl_active = 0;          /* Cursor is in the command line */
    char op = ' ';              /* The cl operation which is in progress */


    if (initscr() == NULL)
        mgoto(clean_up);

    if (raw() == ERR)
        mgoto(clean_up);

    if (noecho() == ERR)
        mgoto(clean_up);

    if (keypad(stdscr, TRUE) == ERR)
        mgoto(clean_up);

    if (nodelay(stdscr, FALSE) == ERR)
        mgoto(clean_up);

    if (set_tabsize(8) == ERR)
        mgoto(clean_up);

    if (argc > 1) {
        for (i = 1; i < argc; ++i)
            if (new_gb(&b, *(argv + i), INIT_GB_SIZE))
                mgoto(clean_up);

        while (b->prev)
            b = b->prev;
    } else {
        /* No args */
        if (new_gb(&b, NULL, INIT_GB_SIZE))
            mgoto(clean_up);
    }

    if ((p = init_gb(INIT_GB_SIZE)) == NULL)
        mgoto(clean_up);

    if ((cl = init_gb(INIT_GB_SIZE)) == NULL)
        mgoto(clean_up);

    if ((se = init_gb(INIT_GB_SIZE)) == NULL)
        mgoto(clean_up);

    if ((tmp = init_gb(INIT_GB_SIZE)) == NULL)
        mgoto(clean_up);

    while (running) {
        if (draw
            (b, cl, cl_active, &sb, &sb_s, &centre, &clr, rv, es_set, es))
            mgoto(clean_up);

        rv = 0;
        es_set = 0;
        es = 0;

        x = getch();

        switch (x) {
        case C('b'):
        case KEY_LEFT:
            rv = left_ch(z);
            break;
        case C('f'):
        case KEY_RIGHT:
            rv = right_ch(z);
            break;
        case C('p'):
        case KEY_UP:
            rv = up_line(z);
            break;
        case C('n'):
        case KEY_DOWN:
            rv = down_line(z);
            break;
        case C('d'):
        case KEY_DC:
            rv = delete_ch(z);
            break;
        case C('h'):
        case 127:
        case KEY_BACKSPACE:
            rv = backspace_ch(z);
            break;
        case C('a'):
        case KEY_HOME:
            start_of_line(z);
            break;
        case C('e'):
        case KEY_END:
            end_of_line(z);
            break;
        case 0:
            set_mark(z);
            break;
        case C('g'):
            if (z->m_set) {
                z->m_set = 0;
                z->m = 0;
            } else if (cl_active) {
                delete_gb(cl);
                cl_active = 0;
            }
            break;
        case C('l'):
            centre = 1;
            clr = 1;
            break;
        case C('w'):
            rv = copy_region(z, p, 1);
            break;
        case C('y'):
            rv = paste(z, p);
            break;
        case C('k'):
            rv = cut_to_eol(z, p);
            break;
        case C('t'):
            trim_clean(z);
            break;
        case C('s'):
            delete_gb(cl);
            cl_active = 1;
            op = 's';
            break;
        case C('z'):
            delete_gb(cl);
            cl_active = 1;
            op = 'z';
            break;
        case C('r'):
            delete_gb(cl);
            cl_active = 1;
            op = 'r';
            break;
        case C('u'):
            delete_gb(cl);
            cl_active = 1;
            op = 'u';
            break;
        case C('q'):
            delete_gb(cl);
            cl_active = 1;
            op = 'q';
            break;
        case ESC:
            y = getch();
            switch (y) {
            case '2':
            case '@':
                set_mark(z);
                break;
            case 'b':
                left_word(z);
                break;
            case 'f':
                right_word(z, ' ');
                break;
            case 'l':
                /* Lowercase word */
                right_word(z, 'L');
                break;
            case 'u':
                /* Uppercase word */
                right_word(z, 'U');
                break;
            case 'k':
                rv = cut_to_sol(z, p);
                break;
            case 'm':
                rv = match_bracket(z);
                break;
            case 'n':
                /* Repeat last search */
                if (search_type == 's')
                    rv = exact_forward_search(b, se);
                else if (search_type == 'z')
                    rv = regex_forward_search(b, se);
                else
                    rv = 1;

                break;
            case 'w':
                rv = copy_region(z, p, 0);
                break;
            case '!':
                remove_gb(&b);
                if (b == NULL)
                    running = 0;

                break;
            case '=':
                delete_gb(cl);
                if (b->fn != NULL && insert_str(cl, b->fn))
                    delete_gb(cl);

                cl_active = 1;
                op = '=';
                break;
            case '$':
                delete_gb(cl);
                cl_active = 1;
                op = '$';       /* insert_shell_cmd */
                break;
            case '`':
                rv = shell_line(z, tmp, &es);
                if (!rv)
                    es_set = 1;

                break;
            case '<':
                start_of_gb(z);
                break;
            case '>':
                end_of_gb(z);
                break;
            }
            break;
        case C('x'):
            y = getch();
            switch (y) {
            case C('x'):
                rv = swap_cursor_and_mark(z);
                break;
            case C('c'):
                running = 0;
                break;
            case C('s'):
                rv = save(b);   /* Cannot save the command line */
                break;
            case C('f'):
                delete_gb(cl);
                cl_active = 1;
                op = 'f';
                break;
            case 'i':
                delete_gb(cl);
                cl_active = 1;
                op = 'i';
                break;
            case KEY_LEFT:
                if (b->prev != NULL)
                    b = b->prev;

                break;
            case KEY_RIGHT:
                if (b->next != NULL)
                    b = b->next;

                break;
            }
            break;
        case '\r':
        case '\n':
            if (cl_active) {
                switch (op) {
                case 's':
                    /* Swap gap buffers */
                    t = se;
                    se = cl;
                    cl = t;
                    delete_gb(cl);
                    search_type = op;
                    rv = exact_forward_search(b, se);
                    break;
                case 'z':
                    /* Swap gap buffers */
                    t = se;
                    se = cl;
                    cl = t;
                    delete_gb(cl);
                    search_type = op;
                    rv = regex_forward_search(b, se);
                    break;
                case 'r':
                    rv = regex_replace_region(b, cl);
                    break;
                case '=':
                    start_of_gb(cl);
                    rv = rename_gb(b, (const char *) cl->a + cl->c);
                    break;
                case 'u':
                    rv = goto_row(b, cl);
                    break;
                case 'q':
                    rv = insert_hex(b, cl);
                    break;
                case 'f':
                    start_of_gb(cl);
                    rv = new_gb(&b, (const char *) cl->a + cl->c,
                                INIT_GB_SIZE);
                    break;
                case 'i':
                    start_of_gb(cl);
                    rv = insert_file(b, (const char *) cl->a + cl->c);
                    break;
                case '$':
                    start_of_gb(cl);
                    rv = insert_shell_cmd(b, (const char *) cl->a + cl->c,
                                          &es);
                    if (!rv)
                        es_set = 1;

                    break;
                }
                cl_active = 0;
                op = ' ';
            } else {
                rv = insert_ch(z, '\n');
            }
            break;
        default:
            if (isprint(x) || x == '\t')
                rv = insert_ch(z, x);

            break;
        }
    }


  clean_up:
    if (endwin() == ERR)
        ret = ERROR;

    free(sb);

    free_gb_list(b);
    free_gb(p);
    free_gb(cl);
    free_gb(se);
    free_gb(tmp);

    return ret;
}

#undef z
