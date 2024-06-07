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


#ifdef __linux__
/* For: cfmakeraw */
#ifndef _DEFAULT_SOURCE
#define _DEFAULT_SOURCE
#endif
#endif

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#endif

#ifdef _WIN32
#include <Windows.h>
#include <conio.h>
#include <io.h>
#include <fcntl.h>
#else
#include <sys/ioctl.h>
#include <unistd.h>
#endif

#include <ctype.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


#define IN_CURSES_LIB
#include "curses.h"
#undef IN_CURSES_LIB


#define DEFAULT_TABSIZE 8
#define ESC 27


/* Unsigned overflow tests */
/* Addition */
#define aof(a, b, max_val) ((a) > (max_val) - (b))

/* Multiplication */
#define mof(a, b, max_val) ((a) && (b) > (max_val) / (a))


#define phy_move(pos) printf("\x1B[%lu;%luH", \
    (unsigned long) ((pos) / stdscr->w + 1),  \
    (unsigned long) ((pos) % stdscr->w + 1))

#define phy_hl_off() printf("\x1B[m")

/* Does not toggle */
#define phy_hl_on() printf("\x1B[7m")

#define phy_clear() printf("\x1B[2J\x1B[1;1H")


WINDOW *initscr(void)
{
    /* Already initialised, so fail */
    if (stdscr != NULL)
        return NULL;

    if ((stdscr = calloc(1, sizeof(WINDOW))) == NULL)
        return NULL;

    /*
     * The memory for the virtual screens will be allocated upon the first call
     * to erase_screen.
     */
    stdscr->vs_c = NULL;
    stdscr->vs_n = NULL;
    stdscr->vs_s = 0;
    stdscr->tabsize = DEFAULT_TABSIZE;
    stdscr->clear = 1;
    stdscr->centre = 0;

    /* Setup terminal */
#ifdef _WIN32
    if (_setmode(_fileno(stdin), _O_BINARY) == -1)
        mgoto(clean_up);

    if (_setmode(_fileno(stdout), _O_BINARY) == -1)
        mgoto(clean_up);

    if (_setmode(_fileno(stderr), _O_BINARY) == -1)
        mgoto(clean_up);

    if ((stdscr->term_handle =
         GetStdHandle(STD_OUTPUT_HANDLE)) == INVALID_HANDLE_VALUE)
        mgoto(clean_up);

    if (!GetConsoleMode(stdscr->term_handle, &stdscr->term_orig))
        mgoto(clean_up);

    stdscr->term_new =
        stdscr->term_orig | ENABLE_PROCESSED_OUTPUT |
        ENABLE_VIRTUAL_TERMINAL_PROCESSING;

    if (!SetConsoleMode(stdscr->term_handle, stdscr->term_new))
        mgoto(clean_up);

#else

    if (tcgetattr(STDIN_FILENO, &stdscr->term_orig))
        mgoto(clean_up);

    stdscr->term_new = stdscr->term_orig;

    cfmakeraw(&stdscr->term_new);

    if (tcsetattr(STDIN_FILENO, TCSANOW, &stdscr->term_new))
        mgoto(clean_up);
#endif

    return stdscr;

  clean_up:
    free(stdscr);
    return NULL;
}


int endwin(void)
{
    int ret = OK;
    phy_hl_off();
    phy_clear();
#ifdef _WIN32
    if (!SetConsoleMode(stdscr->term_handle, stdscr->term_orig))
        ret = ERR;
#else
    if (tcsetattr(STDIN_FILENO, TCSANOW, &stdscr->term_orig))
        ret = ERR;
#endif

    free(stdscr->vs_c);
    free(stdscr->vs_n);
    free(stdscr);

    return ret;
}


int getch(void)
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
                return KEY_LEFT;
            case 'M':
                return KEY_RIGHT;
            case 'H':
                return KEY_UP;
            case 'P':
                return KEY_DOWN;
            case 'S':
                return KEY_DC;
            case 'G':
                return KEY_HOME;
            case 'O':
                return KEY_END;
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
                        return KEY_LEFT;
                    case 'C':
                        return KEY_RIGHT;
                    case 'A':
                        return KEY_UP;
                    case 'B':
                        return KEY_DOWN;
                    case 'H':
                        return KEY_HOME;
                    case 'F':
                        return KEY_END;
                    }
                } else {
                    if (z == '3' && getchar() == '~') {
                        return KEY_DC;
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


static int get_phy_screen_size(void)
{
#ifdef _WIN32
    CONSOLE_SCREEN_BUFFER_INFO si;

    if (!GetConsoleScreenBufferInfo(stdscr->term_handle, &si))
        return ERR;

    stdscr->h = si.srWindow.Bottom - si.srWindow.Top + 1;
    stdscr->w = si.srWindow.Right - si.srWindow.Left + 1;
#else
    struct winsize ws;

    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1)
        return ERR;

    stdscr->h = ws.ws_row;
    stdscr->w = ws.ws_col;
#endif
    return OK;
}


static int erase_screen(int clear)
{
    size_t new_s_s;
    unsigned char *t;

    if (get_phy_screen_size() == ERR)
        return ERR;

    if (mof(stdscr->h, stdscr->w, SIZE_MAX))
        return ERR;

    new_s_s = stdscr->h * stdscr->w;    /* New screen size */

    if (!new_s_s)               /* No screen size */
        return ERR;

    if (clear || new_s_s != stdscr->vs_s) {
        if (new_s_s != stdscr->vs_s) {
            /*
             * Allocates memory the first time, as stdscr->vs_s is initially
             * zero, and stdscr->vs_c and stdscr->vs_n are initially NULL.
             */
            if ((t = realloc(stdscr->vs_c, new_s_s)) == NULL)
                return ERR;

            stdscr->vs_c = t;
            if ((t = realloc(stdscr->vs_n, new_s_s)) == NULL)
                return ERR;

            stdscr->vs_n = t;
            stdscr->vs_s = new_s_s;
        }
        memset(stdscr->vs_c, ' ', stdscr->vs_s);
        phy_hl_off();
        phy_clear();
    }
    memset(stdscr->vs_n, ' ', stdscr->vs_s);
    stdscr->v_i = 0;

    return OK;
}


int erase(void)
{
    return erase_screen(0);
}

int clear(void)
{
    return erase_screen(1);
}


void refresh(void)
{
    size_t k;
    unsigned char *t, ch;

    /* Diff */
    for (k = 0; k < stdscr->vs_s; ++k) {
        if ((ch = *(stdscr->vs_n + k)) != *(stdscr->vs_c + k)) {
            phy_move(k);
            if (ch & '\x80')
                phy_hl_on();
            else
                phy_hl_off();

            putchar(ch & '\x7F');
        }
    }
    phy_move(stdscr->v_i);

    /* Swap */
    t = stdscr->vs_c;
    stdscr->vs_c = stdscr->vs_n;
    stdscr->vs_n = t;
}


int addch(unsigned char ch)
{
    unsigned char new_ch;
    size_t tws;                 /* Tab write size */

    /* Off screen */
    if (stdscr->v_i >= stdscr->vs_s)
        return ERR;

    if (ch == '\n') {
        *(stdscr->vs_n + stdscr->v_i) = stdscr->v_hl ? ' ' | '\x80' : ' ';
        ++stdscr->v_i;
        if (stdscr->v_i % stdscr->w)
            stdscr->v_i = (stdscr->v_i / stdscr->w + 1) * stdscr->w;
    } else {
        if (ch == '\t') {
            tws =
                stdscr->vs_s - stdscr->v_i >
                TABSIZE ? TABSIZE : stdscr->vs_s - stdscr->v_i;
            memset(stdscr->vs_n + stdscr->v_i,
                   stdscr->v_hl ? ' ' | '\x80' : ' ', tws);
            stdscr->v_i += tws;
        } else {
            if (ch == '\0')
                new_ch = '~';
            else if (!isprint(ch))
                new_ch = '?';
            else
                new_ch = ch;

            *(stdscr->vs_n + stdscr->v_i) =
                stdscr->v_hl ? new_ch | '\x80' : new_ch;
            ++stdscr->v_i;
        }
    }

    return OK;
}


int addnstr(const char *str, int n)
{
    char ch;

    if (str == NULL)
        return ERR;

    if (!n)
        return OK;

    if (n > 0) {
        while ((ch = *str++) != '\0' && n--)
            if (addch(ch) == ERR)
                return ERR;
    } else {
        while ((ch = *str++) != '\0')
            if (addch(ch) == ERR)
                return ERR;
    }

    return OK;
}

int move(int y, int x)
{
    size_t new_v_i;

    if (mof(y, stdscr->w, SIZE_MAX))
        return ERR;

    new_v_i = y * stdscr->w;

    if (aof(new_v_i, x, SIZE_MAX))
        return ERR;

    new_v_i += x;

    /* Off screen */
    if (new_v_i >= stdscr->vs_s)
        return ERR;

    stdscr->v_i = new_v_i;
    return OK;
}


int clrtoeol(void)
{
    memset(stdscr->vs_n + stdscr->v_i, ' ',
           stdscr->w - stdscr->v_i % stdscr->w);
    return OK;
}


int standend(void)
{
    stdscr->v_hl = 0;
    return OK;
}

int standout(void)
{
    stdscr->v_hl = 1;
    return OK;
}


int raw(void)
{
    /* Preformed by initscr() in this implementation */
    return OK;
}


int noecho(void)
{
    /* Preformed by initscr() in this implementation */
    return OK;
}


int keypad(WINDOW * win, bool bf)
{
    /* Preformed by initscr() in this implementation */
    if (win == NULL)
        return ERR;

    if (bf != TRUE && bf != FALSE)
        return ERR;

    return OK;
}
