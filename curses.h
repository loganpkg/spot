/*
 * Copyright (c) 2024, 2025 Logan Ryan McLintock. All rights reserved.
 *
 * The contents of this file are subject to the
 * Common Development and Distribution License (CDDL) version 1.1
 * (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License in
 * the LICENSE file included with this software or at
 * https://opensource.org/license/cddl-1-1
 *
 * NOTICE PURSUANT TO SECTION 4.2 OF THE LICENSE:
 * This software is prohibited from being distributed or otherwise made
 * available under any subsequent version of the License.
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See the
 * License for the specific language governing rights and limitations
 * under the License.
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
    size_t h;                   /* Physical screen heigth */
    size_t w;                   /* Physical screen width */
    unsigned char *vs_c;        /* Current virtual screen */
    unsigned char *vs_n;        /* Next virtual screen */
    size_t vs_s;                /* Size of each virtual screen */
    size_t v_i;                 /* Virtual cursor index (print location) */
    int v_hl;                   /* Virtual highlight mode indicator */
    int non_blocking;           /* Non-blocking getch */
    size_t tabsize;             /* Number of spaces printed for a Tab */
    int clear;                  /* Clear physical screen */
    int centre;                 /* Draw cursor on the centre row */
    /* The buf module is not used in order to keep curses self-contained. */
    /* For raw characters: */
    unsigned char *a;           /* Unread memory buffer */
    size_t i;                   /* Next unused space in unread memory buffer */
    size_t n;                   /* Allocated size of unread memory buffer */
    /* For cooked characters, includes special keyboard keys: */
    int *unget_buf;
    size_t b_i;                 /* Next unused element in unget_buf */
    size_t b_n;                 /* Allocated elements in unget_buf */
};


typedef struct screen WINDOW;
typedef char chtype;

/*
 * Only ASCII characters are supported, and the highest order bit is used
 * for the attributes component.
 */
#define A_CHARTEXT 0x7F
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
