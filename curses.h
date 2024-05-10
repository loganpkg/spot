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


#ifndef CURSES_H
#define CURSES_H


#ifndef _WIN32
/* For struct termios */
#include <termios.h>
#endif

/* For size_t */
#include <stddef.h>


struct screen {
#ifdef _WIN32
    HANDLE term_handle;
    DWORD term_orig;
    DWORD term_new;
#else
    struct termios term_orig;
    struct termios term_new;
#endif
    size_t h;                   /* Physical screen heigth */
    size_t w;                   /* Physical screen width */
    unsigned char *vs_c;        /* Current virtual screen */
    unsigned char *vs_n;        /* Next virtual screen */
    size_t vs_s;                /* Size of each virtual screen */
    size_t v_i;                 /* Virtual cursor index (print location) */
    int v_hl;                   /* Virtual highlight mode indicator */
    size_t tabsize;             /* Number of spaces printed for a Tab */
    int clear;                  /* Clear physical screen */
    int centre;                 /* Draw cursor on the centre row */
};


typedef struct screen WINDOW;


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

/* Success is 0 or: */
#define OK 0

#define ERR 1


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

#define set_tabsize(size) (stdscr->tabsize = (size))

#define TABSIZE (stdscr->tabsize)


/* void getmaxyx(WINDOW *win, int y, int x); */
#define getmaxyx(win, y, x) do { \
    (y) = (win)->h;              \
    (x) = (win)->w;              \
} while (0)


/* void getyx(WINDOW *win, int y, int x); */
#define getyx(win, y, x) do {    \
    (y) = (win)->v_i / (win)->w; \
    (x) = (win)->v_i % (win)->w; \
} while (0)


/* Function declarations */
WINDOW *initscr(void);
int endwin(void);
int getch(void);
int erase(void);
int clear(void);
void refresh(void);
int addch(unsigned char ch);
int addnstr(const char *str, int n);
int move(int y, int x);
int clrtoeol(void);
int standend(void);
int standout(void);
int raw(void);
int noecho(void);
int keypad(WINDOW * win, bool bf);

#endif
