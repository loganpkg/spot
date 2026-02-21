/*
 * Copyright (c) 2023-2025 Logan Ryan McLintock. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef CURSES_H
#define CURSES_H

#ifndef _WIN32
/* For struct termios */
#include <termios.h>
#endif

/* For size_t */
#include <stddef.h>
#include <stdint.h>

/* Success is 0 or: */
#ifndef OK
#define OK 0
#endif

/* Only use with curses */
#define ERR EOF

struct screen {
#ifdef _WIN32
    HANDLE term_handle;
    DWORD term_orig;
    DWORD term_new;
#else
    struct termios term_orig;
    struct termios term_new;
#endif
    size_t h;            /* Physical screen heigth */
    size_t w;            /* Physical screen width */
    unsigned char *vs_c; /* Current virtual screen */
    unsigned char *vs_n; /* Next virtual screen */
    size_t vs_s;         /* Size of each virtual screen */
    size_t v_i;          /* Virtual cursor index (print location) */
    int v_hl;            /* Virtual highlight mode indicator */
    int non_blocking;    /* Non-blocking getch */
    size_t tabsize;      /* Number of spaces printed for a Tab */
    int clear;           /* Clear physical screen */
    int centre;          /* Draw cursor on the centre row */
    /* The buf module is not used in order to keep curses self-contained. */
    /* For raw characters: */
    unsigned char *a; /* Unread memory buffer */
    size_t i;         /* Next unused space in unread memory buffer */
    size_t n;         /* Allocated size of unread memory buffer */
    /* For cooked characters, includes special keyboard keys: */
    int *unget_buf;
    size_t b_i; /* Next unused element in unget_buf */
    size_t b_n; /* Allocated elements in unget_buf */
};

typedef struct screen WINDOW;
typedef char chtype;

/*
 * Only ASCII characters are supported, and the highest order bit is used
 * for the attributes component.
 */
#define A_CHARTEXT   0x7F
#define A_ATTRIBUTES 0x80

/* Highlighting is the only attribute that is implemented */
#define A_STANDOUT 0x80

#ifdef IN_CURSES_LIB
/* In library, so external variable */
extern WINDOW *stdscr;
#else
/* In application source, so global variable */
WINDOW *stdscr;
#endif

typedef unsigned char bool;

#ifndef FALSE
#define FALSE 0
#endif

#ifndef TRUE
#define TRUE (!FALSE)
#endif

#define C(l) ((l) - 'a' + 1)

/* Set to Ctrl-H */

#define KEY_LEFT  (UCHAR_MAX + 1)
#define KEY_RIGHT (UCHAR_MAX + 2)
#define KEY_UP    (UCHAR_MAX + 3)
#define KEY_DOWN  (UCHAR_MAX + 4)
#define KEY_DC    (UCHAR_MAX + 5)
#define KEY_HOME  (UCHAR_MAX + 6)
#define KEY_END   (UCHAR_MAX + 7)

/* Not set by this implementation */
#define KEY_BACKSPACE (UCHAR_MAX + 8)

#define TABSIZE (stdscr->tabsize)

/* void getmaxyx(WINDOW *win, int y, int x); */
#define getmaxyx(win, y, x)                                                   \
    do {                                                                      \
        (y) = (win)->h;                                                       \
        (x) = (win)->w;                                                       \
    } while (0)

/* void getyx(WINDOW *win, int y, int x); */
#define getyx(win, y, x)                                                      \
    do {                                                                      \
        (y) = (win)->v_i / (win)->w;                                          \
        (x) = (win)->v_i % (win)->w;                                          \
    } while (0)

/* Function declarations */
WINDOW *initscr(void);
int endwin(void);
int set_tabsize(size_t size);
int ungetch(int ch);
int getch(void);
int erase(void);
int clear(void);
int refresh(void);
int addch(unsigned char ch);
int addnstr(const char *str, int n);
int move(int y, int x);
chtype inch(void);
int clrtoeol(void);
int standend(void);
int standout(void);
int raw(void);
int noecho(void);
int keypad(WINDOW *win, bool bf);
int nodelay(WINDOW *win, bool bf);
int curs_set(int visibility);

#endif
