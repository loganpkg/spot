/*
 * Copyright (c) 2023, 2024 Logan Ryan McLintock
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
 * Curses module.
 *
 * "Do not be worried and upset," Jesus told them.
 * "Believe in God and believe also in me. ..."
 *                                  John 14:1 GNT
 */


#include "toucanlib.h"


struct screen *init_screen(void)
{
    struct screen *s = NULL;

    if ((s = calloc(1, sizeof(struct screen))) == NULL)
        return NULL;

    /*
     * The memory for the virtual screens will be allocated upon the first call
     * to erase_screen.
     */
    s->vs_c = NULL;
    s->vs_n = NULL;
    s->vs_s = 0;
    s->clear = 1;
    s->centre = 0;

    if (sane_io())
        goto clean_up;

    /* Setup terminal */
#ifdef _WIN32
    if ((s->term_handle =
         GetStdHandle(STD_OUTPUT_HANDLE)) == INVALID_HANDLE_VALUE)
        goto clean_up;

    if (!GetConsoleMode(s->term_handle, &s->term_orig))
        goto clean_up;

    s->term_new =
        s->term_orig | ENABLE_PROCESSED_OUTPUT |
        ENABLE_VIRTUAL_TERMINAL_PROCESSING;

    if (!SetConsoleMode(s->term_handle, s->term_new))
        goto clean_up;

#else

    if (tcgetattr(STDIN_FILENO, &s->term_orig))
        goto clean_up;

    s->term_new = s->term_orig;

    cfmakeraw(&s->term_new);

    if (tcsetattr(STDIN_FILENO, TCSANOW, &s->term_new))
        goto clean_up;
#endif

    return s;

  clean_up:
    free(s);
    return NULL;
}


int free_screen(struct screen *s)
{
    int ret = 0;
    phy_hl_off();
    phy_clear();
#ifdef _WIN32
    if (!SetConsoleMode(s->term_handle, s->term_orig))
        ret = ERR;
#else
    if (tcsetattr(STDIN_FILENO, TCSANOW, &s->term_orig))
        ret = ERR;
#endif

    free(s->vs_c);
    free(s->vs_n);
    free(s);

    return ret;
}


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


static int get_phy_screen_size(struct screen *s)
{
#ifdef _WIN32
    CONSOLE_SCREEN_BUFFER_INFO si;

    if (!GetConsoleScreenBufferInfo(s->term_handle, &si))
        return ERR;

    s->h = si.srWindow.Bottom - si.srWindow.Top + 1;
    s->w = si.srWindow.Right - si.srWindow.Left + 1;
#else
    struct winsize ws;

    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1)
        return ERR;

    s->h = ws.ws_row;
    s->w = ws.ws_col;
#endif
    return 0;
}


int erase_screen(struct screen *s)
{
    size_t new_s_s;
    unsigned char *t;

    if (get_phy_screen_size(s))
        return ERR;

    if (mof(s->h, s->w, SIZE_MAX))
        return ERR;

    new_s_s = s->h * s->w;      /* New screen size */

    if (!new_s_s)               /* No screen size */
        return ERR;

    if (s->clear || new_s_s != s->vs_s) {
        if (new_s_s != s->vs_s) {
            /*
             * Allocates memory the first time, as s->vs_s is initially zero,
             * and s->vs_c and s->vs_n are initially NULL.
             */
            if ((t = realloc(s->vs_c, new_s_s)) == NULL)
                return ERR;

            s->vs_c = t;
            if ((t = realloc(s->vs_n, new_s_s)) == NULL)
                return ERR;

            s->vs_n = t;
            s->vs_s = new_s_s;
        }
        memset(s->vs_c, ' ', s->vs_s);
        phy_hl_off();
        phy_clear();
        s->clear = 0;
    }
    memset(s->vs_n, ' ', s->vs_s);
    s->v_i = 0;

    return 0;
}


void refresh_screen(struct screen *s)
{
    size_t k;
    unsigned char *t, ch;

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
    phy_move(s->v_i);

    /* Swap */
    t = s->vs_c;
    s->vs_c = s->vs_n;
    s->vs_n = t;
}


int print_ch(struct screen *s, unsigned char ch)
{
    unsigned char new_ch;
    size_t tws;                 /* Tab write size */

    /* Off screen */
    if (s->v_i >= s->vs_s)
        return ERR;

    if (ch == '\n') {
        *((s)->vs_n + (s)->v_i) = (s)->v_hl ? ' ' | '\x80' : ' ';
        ++s->v_i;
        if (s->v_i % s->w)
            s->v_i = (s->v_i / s->w + 1) * s->w;
    } else {
        if (ch == '\t') {
            tws =
                s->vs_s - s->v_i > TAB_SIZE ? TAB_SIZE : s->vs_s - s->v_i;
            memset(s->vs_n + s->v_i, s->v_hl ? ' ' | '\x80' : ' ', tws);
            s->v_i += tws;
        } else {
            if (ch == '\0')
                new_ch = '~';
            else if (!isprint(ch))
                new_ch = '?';
            else
                new_ch = ch;

            *(s->vs_n + s->v_i) = s->v_hl ? new_ch | '\x80' : new_ch;
            ++s->v_i;
        }
    }

    return 0;
}

int print_object(struct screen *s, size_t y, size_t x, const char *object)
{
    /* Does not move the cursor. Does not do highlighting. */
    char ch;

    if (y >= s->h || x >= s->w)
        return 1;

    s->v_i = y * s->w + x;
    while (1) {
        ch = *object;
        if (ch == '\0')
            break;

        if (print_ch(s, ch))
            return ERR;

        if (ch == '\n')
            s->v_i += x;        /* Indent */

        ++object;
    }
    return 0;
}
