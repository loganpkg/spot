/*
 * Copyright (c) 2023, 2024 Logan Ryan McLintock
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

#define INIT_UNREAD_MEM_SIZE 64

#define CTRL_2 0

#define mgoto(lb) do {                                                  \
    fprintf(stderr, "[%s:%d]: Error: " #lb "\n", __FILE__, __LINE__);   \
    goto lb;                                                            \
} while (0)


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
    stdscr->tabsize = DEFAULT_TABSIZE;
    stdscr->clear = 1;

    if ((stdscr->a = calloc(INIT_UNREAD_MEM_SIZE, 1)) == NULL)
        mgoto(error);

    stdscr->n = INIT_UNREAD_MEM_SIZE;

    /* Setup terminal */
#ifdef _WIN32
    if (_setmode(_fileno(stdin), _O_BINARY) == -1)
        mgoto(error);

    if (_setmode(_fileno(stdout), _O_BINARY) == -1)
        mgoto(error);

    if (_setmode(_fileno(stderr), _O_BINARY) == -1)
        mgoto(error);

    if ((stdscr->term_handle =
         GetStdHandle(STD_OUTPUT_HANDLE)) == INVALID_HANDLE_VALUE)
        mgoto(error);

    if (!GetConsoleMode(stdscr->term_handle, &stdscr->term_orig))
        mgoto(error);

    stdscr->term_new =
        stdscr->term_orig | ENABLE_PROCESSED_OUTPUT |
        ENABLE_VIRTUAL_TERMINAL_PROCESSING;

    if (!SetConsoleMode(stdscr->term_handle, stdscr->term_new))
        mgoto(error);

#else

    if (tcgetattr(STDIN_FILENO, &stdscr->term_orig))
        mgoto(error);

    stdscr->term_new = stdscr->term_orig;

    cfmakeraw(&stdscr->term_new);

    /* Wait forever for 1 char */
    stdscr->term_new.c_cc[VMIN] = 1;
    stdscr->term_new.c_cc[VTIME] = 0;

    if (tcsetattr(STDIN_FILENO, TCSANOW, &stdscr->term_new))
        mgoto(error);
#endif

    return stdscr;

  error:
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
    free(stdscr->a);
    free(stdscr);

    return ret;
}


int set_tabsize(size_t size)
{
    stdscr->tabsize = size;
    return OK;
}


#ifndef _WIN32
static int _kbhit(void)
{
    fd_set read_fd_set;
    struct timeval t_o = { 0 };

    FD_ZERO(&read_fd_set);
    FD_SET(STDIN_FILENO, &read_fd_set);
    if (select(STDIN_FILENO + 1, &read_fd_set, NULL, NULL, &t_o) == -1) {
        fprintf(stderr, "[%s:%d]: select: Error\n", __FILE__, __LINE__);
        return 0;               /* Error */
    }

    if (FD_ISSET(STDIN_FILENO, &read_fd_set))
        return 1;               /* Ready */

    return 0;                   /* No keyboard hit */
}
#endif


static int unread(unsigned char u)
{
    unsigned char *t;
    size_t new_n;

    if (stdscr->i == stdscr->n) {
        /* Grow buffer */
        if (aof(stdscr->n, 1, SIZE_MAX))
            return ERR;

        new_n = stdscr->n + 1;

        if (mof(new_n, 2, SIZE_MAX))
            return ERR;

        new_n *= 2;

        if ((t = realloc(stdscr->a, new_n)) == NULL)
            return ERR;

        stdscr->a = t;
        stdscr->n = new_n;
    }

    *(stdscr->a + stdscr->i) = u;
    ++stdscr->i;

    return OK;
}


static int getch_raw(void)
{

#ifndef _WIN32
    int num, x, k;
    unsigned char t;
    size_t s_i, e_i;
#endif

  top:
    if (stdscr->i) {
        stdscr->i--;
        return *(stdscr->a + stdscr->i);
    }

    if (!stdscr->non_blocking)
#ifdef _WIN32
        return _getch();
#else
        return getchar();
#endif

    /* Non-blocking */

    /* No chars ready */
    if (!_kbhit())
        return ERR;


#ifdef _WIN32
    return _getch();
#else
    /* See how many chars are waiting */
    if (ioctl(STDIN_FILENO, FIONREAD, &num) == -1) {
        fprintf(stderr, "[%s:%d]: ioctl: Error\n", __FILE__, __LINE__);
        return ERR;
    }

    if (!num)
        return ERR;

    /*
     * Need to read all characters that are waiting in stdin.
     * Some keys send a multi-byte sequence, such as the arrow keys.
     * If only the first byte is read, then select() will report that
     * it is not ready until the next key is pressed, delaying the
     * reading of the rest of the bytes in the original key.
     */
    for (k = 0; k < num; ++k) {
        x = getchar();
        unread(x);
    }

    /* Need to reverse chars */
    s_i = stdscr->i - num;
    e_i = stdscr->i - 1;
    while (s_i < e_i) {
        t = *(stdscr->a + s_i);
        *(stdscr->a + s_i) = *(stdscr->a + e_i);
        *(stdscr->a + e_i) = t;
        ++s_i;
        --e_i;
    }

    goto top;
#endif
}


int getch(void)
{
#ifdef _WIN32
    int x, y;

  top:
    x = getch_raw();
    if (x == 0) {
        if ((y = getch_raw()) == ERR) {
            unread(x);
            return ERR;
        }
        if (y == 3)
            return CTRL_2;
        else
            goto top;           /* Eat */
    } else if (x == 224) {
        if ((y = getch_raw()) == ERR) {
            unread(x);
            return ERR;
        }
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
        default:
            goto top;           /* Eat */
        }
    }
    return x;

#else

#define get_sb_ch() do {                                \
    if (i == MAX_SEQ_LEN) {                              \
        fprintf(stderr, "[%s:%d]: getch: Error: "       \
            "Key sequence buffer is full\n",            \
            __FILE__, __LINE__);                        \
        goto top;                                       \
    }                                                   \
    if ((sb[i++] = getch_raw()) == ERR) {               \
        while (i)                                       \
            if (sb[--i] != ERR)                         \
                unread(sb[i]);                          \
                                                        \
        return ERR;                                     \
    }                                                   \
} while (0)

    /* Must be at least 4 */
#define MAX_SEQ_LEN 10

    int sb[MAX_SEQ_LEN];        /* Sequence buffer */
    size_t i = 0;               /* Index of next write in sb */

  top:

    i = 0;
    get_sb_ch();
    if (sb[0] == C('x')) {
        /*
         * Control X is often used as the prefix in a multi-key sequence.
         * So need to wait until the next char is ready, otherwise the
         * sequence will be separated.
         */
        get_sb_ch();
        unread(sb[1]);
        return sb[0];
    } else if (sb[0] == ESC) {
        get_sb_ch();
        if (sb[1] == '[') {
            get_sb_ch();
            switch (sb[2]) {
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
            case '3':
                get_sb_ch();
                if (sb[3] == '~')
                    return KEY_DC;

              eat:
                /* Eat to end of sequence */
                while (1) {
                    get_sb_ch();
                    /* All of sequence has been eaten */
                    if (!(isdigit(sb[i - 1]) || sb[i - 1] == ';'))
                        goto top;
                }
            default:
                goto eat;
            }
        } else {
            unread(sb[1]);
            return sb[0];
        }
    }
    return sb[0];
#endif
}

#undef get_sb_ch


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


int keypad(WINDOW *win, bool bf)
{
    /* Preformed by initscr() in this implementation */
    if (win == NULL)
        return ERR;

    if (bf != TRUE && bf != FALSE)
        return ERR;

    return OK;
}


int nodelay(WINDOW *win, bool bf)
{
    if (win == NULL)
        return ERR;

    if (bf != TRUE && bf != FALSE)
        return ERR;

    /*
       #ifndef _WIN32
       if ((!win->non_blocking && bf == TRUE)
       || (win->non_blocking && bf == FALSE)) {
       if ((flags = fcntl(STDIN_FILENO, F_GETFL)) == -1)
       return ERR;

       if (bf == TRUE)
       flags |= O_NONBLOCK;
       else
       flags &= ~O_NONBLOCK;

       if (fcntl(STDIN_FILENO, F_SETFL, flags) == -1)
       return ERR;
       }
       #endif
     */

    win->non_blocking = bf;

    return OK;
}
