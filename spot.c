/*
 * Copyright (c) 2023 Logan Ryan McLintock
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

#ifdef __linux__
/* For: snprintf */
#define _XOPEN_SOURCE 500
/* For: cfmakeraw */
#define _DEFAULT_SOURCE
#endif

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
/* For: _getch */
#include <conio.h>
/* For terminal functions */
#include <Windows.h>
#else
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>
#endif

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "gen.h"
#include "num.h"
#include "gb.h"

#ifdef _WIN32
#define DEFAULT_APP "\"\""
#define APP_PDF DEFAULT_APP
#define APP_JPG DEFAULT_APP
#define APP_MOV DEFAULT_APP
#define APP_MP4 DEFAULT_APP
#else
#define APP_PDF "mupdf"
#define APP_JPG "feh --fullscreen --start-at"
#define APP_MOV "vlc"
#define APP_MP4 "vlc"
#endif


#define INIT_GB_SIZE 512

#define LEFT_KEY  (UCHAR_MAX + 1)
#define RIGHT_KEY (UCHAR_MAX + 2)
#define UP_KEY    (UCHAR_MAX + 3)
#define DOWN_KEY  (UCHAR_MAX + 4)
#define DEL_KEY   (UCHAR_MAX + 5)
#define HOME_KEY  (UCHAR_MAX + 6)
#define END_KEY   (UCHAR_MAX + 7)

#define ESC 27

#define C(l) ((l) - 'a' + 1)

#define phy_move(pos) printf("\x1B[%lu;%luH", \
    (unsigned long) ((pos) / s->w + 1),       \
    (unsigned long) ((pos) % s->w + 1))

#define phy_hl_off() printf("\x1B[m")

/* Does not toggle */
#define phy_hl_on() printf("\x1B[7m")

#define phy_clear() printf("\x1B[2J\x1B[1;1H")

struct screen {
#ifdef _WIN32
    HANDLE term_handle;
#endif
    size_t h;                   /* Physical screen heigth */
    size_t w;                   /* Physical screen width */
    unsigned char *vs_c;        /* Current virtual screen */
    unsigned char *vs_n;        /* Next virtual screen */
    size_t vs_s;                /* Size of each virtual screen */
    int clear;                  /* Clear physical screen */
    int centre;                 /* Draw cursor on the centre row */
};


int get_key(void)
{
    int x, y;

#ifdef _WIN32
    while (1) {
        x = _getch();
        if (x == 0) {
            y = _getch();
            if (y == 3)
                return 0;       /* Ctrl 2 */
        } else if (x == 224) {
            y = _getch();
            switch (y) {
            case 'K':
                return LEFT_KEY;
            case 'M':
                return RIGHT_KEY;
            case 'H':
                return UP_KEY;
            case 'P':
                return DOWN_KEY;
            case 'S':
                return DEL_KEY;
            case 'G':
                return HOME_KEY;
            case 'O':
                return END_KEY;
            }
        } else {
            return x;
        }
    }

#else

    int z, w;

    while (1) {
        x = getchar();
        if (x == ESC) {
            y = getchar();
            if (y == '[') {
                z = getchar();
                if (!isdigit(z)) {
                    switch (z) {
                    case 'D':
                        return LEFT_KEY;
                    case 'C':
                        return RIGHT_KEY;
                    case 'A':
                        return UP_KEY;
                    case 'B':
                        return DOWN_KEY;
                    case 'H':
                        return HOME_KEY;
                    case 'F':
                        return END_KEY;
                    }
                } else {
                    if (z == '3' && getchar() == '~') {
                        return DEL_KEY;
                    } else {
                        /* Eat to end */
                        while ((w = getchar()) != '~'
                               && (isdigit(w) || w == ';'));
                    }
                }
            } else if (y == 'O') {
                getchar();      /* Eat */
            } else {
                if (ungetc(y, stdin) == EOF)
                    return EOF;

                return ESC;
            }
        } else {
            return x;
        }
    }

#endif
}

int get_screen_size(struct screen *s)
{
#ifdef _WIN32
    CONSOLE_SCREEN_BUFFER_INFO si;

    if (!GetConsoleScreenBufferInfo(s->term_handle, &si))
        return 1;

    s->h = si.srWindow.Bottom - si.srWindow.Top + 1;
    s->w = si.srWindow.Right - si.srWindow.Left + 1;
#else
    struct winsize ws;

    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1)
        return 1;

    s->h = ws.ws_row;
    s->w = ws.ws_col;
#endif
    return 0;
}

#define print_ch(b) do {                                  \
    ch = *((b)->a + i);                                   \
    if (ch == '\n') {                                     \
        *(s->vs_n + j) = v_hl ? ' ' | '\x80' : ' ';       \
        ++j;                                              \
        if (j % s->w)                                     \
            j = (j / s->w + 1) * s->w;                    \
    } else {                                              \
        if (ch == '\0')                                   \
            new_ch = '~';                                 \
        else if (ch == '\t')                              \
            new_ch = '>';                                 \
        else if (!isprint(ch))                            \
            new_ch = '?';                                 \
        else                                              \
            new_ch = ch;                                  \
                                                          \
        *(s->vs_n + j) = v_hl ? new_ch | '\x80' : new_ch; \
        ++j;                                              \
    }                                                     \
    ++i;                                                  \
} while (0)

int draw(struct gb *b, struct gb *cl, struct screen *s, int cl_active,
         int rv, int es_set, int es)
{
    size_t new_s_s, up, target_up, ts, i, j, cursor_pos, k;
    int v_hl, have_centred = 0;
    unsigned char *t, ch, new_ch, *sb;
    char num_str[NUM_BUF_SIZE];
    int r, len;

    if (get_screen_size(s))
        return 1;

    if (mof(s->h, s->w, SIZE_MAX))
        return 1;

    new_s_s = s->h * s->w;      /* New screen size */

    if (!new_s_s)
        return 0;

  start:

    if (s->clear || new_s_s != s->vs_s) {
        if (new_s_s != s->vs_s) {
            if ((t = realloc(s->vs_c, new_s_s)) == NULL)
                return 1;

            s->vs_c = t;
            if ((t = realloc(s->vs_n, new_s_s)) == NULL)
                return 1;

            s->vs_n = t;
            s->vs_s = new_s_s;
        }
        memset(s->vs_c, ' ', s->vs_s);
        phy_hl_off();
        phy_clear();
        s->clear = 0;
    }
    memset(s->vs_n, ' ', s->vs_s);


    if (b->d > b->g || s->centre) {
        /* Does not consider long lines that wrap */
        b->d = b->g;
        up = 0;
        target_up = s->h <= 4 ? 1 : (s->h - 1) / 2;
        while (b->d) {
            --b->d;
            if (*(b->a + b->d) == '\n')
                ++up;

            if (up == target_up) {
                ++b->d;
                break;
            }
        }
        s->centre = 0;
        have_centred = 1;
    }

    /* Size of the text portion of the screen */
    ts = s->vs_s - (s->h >= 2 ? s->w : 0) - (s->h >= 3 ? s->w : 0);

    v_hl = 0;

    /* Region commenced before draw start */
    if (b->m_set && b->m < b->d)
        v_hl = 1;

    /* Before the gap */
    i = b->d;
    j = 0;
    while (i < b->g && j < ts) {
        if (b->m_set && b->m == i)
            v_hl = 1;

        print_ch(b);
    }

    if (j == ts) {
        /* Off screen */
        if (!have_centred)
            s->centre = 1;
        else
            b->d = b->g;
        goto start;
    }

    /* At the cursor */
    cursor_pos = j;
    if (b->m_set) {
        if (b->m < b->c)
            v_hl = 0;           /* End of region */
        else
            v_hl = 1;           /* Start of region */
    }

    /* After the gap (from the cursor onwards) */
    i = b->c;
    while (i <= b->e && j < ts) {
        if (b->m_set && b->m == i)
            v_hl = 0;

        print_ch(b);
    }

    if (s->h >= 2) {
        /* Status bar */

        if (es_set) {
            r = snprintf(num_str, NUM_BUF_SIZE, "%d", es);
            if (r < 0 || r >= NUM_BUF_SIZE)
                return 1;
        } else {
            *num_str = '\0';
        }

        sb = s->vs_n + ((s->h - 2) * s->w);
        len = snprintf((char *) sb, s->w, "%c%c %s (%lu,%lu) %02X %s",
                       rv ? '!' : ' ', b->mod ? '*' : ' ', b->fn,
                       (unsigned long) b->r, (unsigned long) b->col,
                       cl_active ? *(cl->a + cl->c) : *(b->a + b->c),
                       num_str);
        if (len < 0)
            return 1;

        /* Overwrite \0 char */
        if ((size_t) len >= s->w)       /* Truncated */
            *(sb + s->w - 1) = ' ';
        else
            *(sb + len) = ' ';

        /* Virtually highlight */
        for (k = 0; k < s->w; ++k)
            *(sb + k) |= '\x80';
    }

    if (s->h >= 3) {
        /* Command line */
        if (cl->d > cl->g || cl->g - cl->d > cl->col
            || cl->g - cl->d >= s->w) {
            if (cl->col < s->w)
                cl->d = cl->g - cl->col;
            else
                cl->d = cl->g;
        }

        v_hl = 0;

        if (cl->m_set && cl->m < cl->d)
            v_hl = 1;

        /* Start of last line in virtual screen */
        j = (s->h - 1) * s->w;

        /* Before the gap */
        i = cl->d;
        while (i < cl->g && j < s->vs_s) {
            if (cl->m_set && cl->m == i)
                v_hl = 1;

            print_ch(cl);
        }

        /* At the cursor */
        if (cl_active)
            cursor_pos = j;

        if (cl->m_set) {
            if (cl->m < cl->c)
                v_hl = 0;       /* End of region */
            else
                v_hl = 1;       /* Start of region */
        }

        /* After the gap (from the cursor onwards) */
        i = cl->c;
        while (i <= cl->e && j < s->vs_s) {
            if (cl->m_set && cl->m == i)
                v_hl = 0;

            print_ch(cl);
        }
    }

    /* Diff */
    for (k = 0; k < s->vs_s; ++k) {
        if ((ch = *(s->vs_n + k)) != *(s->vs_c + k)) {
            phy_move(k);
            if (ch & '\x80')
                phy_hl_on();
            else
                phy_hl_off();

            putchar(ch & '\x7F');
        }
    }
    phy_move(cursor_pos);

    /* Swap */
    t = s->vs_c;
    s->vs_c = s->vs_n;
    s->vs_n = t;
    return 0;
}

#undef print_ch


#define z (cl_active ? cl : b)

int main(int argc, char **argv)
{
    int ret = 0;                /* Return value of the text editor */
    int running = 1;            /* Indicates if the text editor is running */
    int rv = 0;                 /* Return value of last internal command */
    int es_set = 0;
    int es = 0;                 /* Exit status of last shell command */
    int i, x, y;
    struct screen s;
    struct gb *b = NULL;        /* Text buffers linked together */
    struct gb *p = NULL;        /* Paste buffer */
    struct gb *cl = NULL;       /* Command line buffer */
    struct gb *sb = NULL;       /* Search buffer */
    struct gb *tmp = NULL;      /* Temporary buffer */
    struct gb *t;               /* For switching gap buffers */
    char search_type = ' ';     /* s = Exact search, z = Regex search */
    int cl_active = 0;          /* Cursor is in the command line */
    char op = ' ';              /* The cl operation which is in progress */

#ifdef _WIN32
    DWORD term_orig, term_new;
#else
    struct termios term_orig;
    struct termios term_new;
#endif

    /* Init some screen variables */
    s.vs_c = NULL;
    s.vs_n = NULL;
    s.vs_s = 0;
    s.clear = 1;
    s.centre = 0;

    if (sane_io())
        return 1;

/* Setup terminal */
#ifdef _WIN32
    if ((s.term_handle =
         GetStdHandle(STD_OUTPUT_HANDLE)) == INVALID_HANDLE_VALUE)
        return 1;

    if (!GetConsoleMode(s.term_handle, &term_orig))
        return 1;

    term_new =
        term_orig | ENABLE_PROCESSED_OUTPUT |
        ENABLE_VIRTUAL_TERMINAL_PROCESSING;

    if (!SetConsoleMode(s.term_handle, term_new))
        return 1;

#else

    if (tcgetattr(STDIN_FILENO, &term_orig))
        return 1;

    term_new = term_orig;

    cfmakeraw(&term_new);

    if (tcsetattr(STDIN_FILENO, TCSANOW, &term_new))
        return 1;

#endif

    if (argc > 1) {
        for (i = 1; i < argc; ++i) {
            if (new_gb(&b, *(argv + i), INIT_GB_SIZE))
                goto clean_up;
        }
        while (b->prev)
            b = b->prev;
    } else {
        /* No args */
        if (new_gb(&b, NULL, INIT_GB_SIZE))
            goto clean_up;
    }

    if ((p = init_gb(INIT_GB_SIZE)) == NULL)
        goto clean_up;

    if ((cl = init_gb(INIT_GB_SIZE)) == NULL)
        goto clean_up;

    if ((sb = init_gb(INIT_GB_SIZE)) == NULL)
        goto clean_up;

    if ((tmp = init_gb(INIT_GB_SIZE)) == NULL)
        goto clean_up;

    while (running) {
        if (draw(b, cl, &s, cl_active, rv, es_set, es))
            goto clean_up;

        rv = 0;
        es_set = 0;
        es = 0;

        x = get_key();

        switch (x) {
        case C('b'):
        case LEFT_KEY:
            rv = left_ch(z);
            break;
        case C('f'):
        case RIGHT_KEY:
            rv = right_ch(z);
            break;
        case C('p'):
        case UP_KEY:
            rv = up_line(z);
            break;
        case C('n'):
        case DOWN_KEY:
            rv = down_line(z);
            break;
        case C('d'):
        case DEL_KEY:
            rv = delete_ch(z);
            break;
        case C('h'):
        case 127:
            rv = backspace_ch(z);
            break;
        case C('a'):
        case HOME_KEY:
            start_of_line(z);
            break;
        case C('e'):
        case END_KEY:
            end_of_line(z);
            break;
        case 0:
            z->m_set = 1;
            z->m = z->c;
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
            s.centre = 1;
            s.clear = 1;
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
            y = get_key();
            switch (y) {
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
                    rv = exact_forward_search(b, sb);
                else if (search_type == 'z')
                    rv = regex_forward_search(b, sb);
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
            y = get_key();
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
            case LEFT_KEY:
                if (b->prev != NULL)
                    b = b->prev;

                break;
            case RIGHT_KEY:
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
                    t = sb;
                    sb = cl;
                    cl = t;
                    delete_gb(cl);
                    search_type = op;
                    rv = exact_forward_search(b, sb);
                    break;
                case 'z':
                    /* Swap gap buffers */
                    t = sb;
                    sb = cl;
                    cl = t;
                    delete_gb(cl);
                    search_type = op;
                    rv = regex_forward_search(b, sb);
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
    phy_hl_off();
    phy_clear();
#ifdef _WIN32
    if (!SetConsoleMode(s.term_handle, term_orig))
        ret = 1;
#else
    if (tcsetattr(STDIN_FILENO, TCSANOW, &term_orig))
        ret = 1;
#endif

    free(s.vs_c);
    free(s.vs_n);

    free_gb_list(b);
    free_gb(p);
    free_gb(cl);
    free_gb(sb);
    free_gb(tmp);

    return ret;
}

#undef z
