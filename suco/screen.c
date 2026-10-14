/*
 * Copyright (c) 2025 Logan Ryan McLintock. All rights reserved.
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

#if defined(__linux__) && !defined(_DEFAULT_SOURCE)
#define _DEFAULT_SOURCE
#endif

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#else
#include <sys/ioctl.h>
#endif

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "alias.h"
#include "debug.h"
#include "int.h"
#include "screen.h"

/* ANSI escape sequences: */
#define es_clear()        printf("\x1B[2J")
#define es_reset()        printf("\x1B[0m")
#define es_reverse_on()   printf("\x1B[7m")
#define es_reverse_off()  printf("\x1B[27m")
#define es_blinking_off() printf("\x1B[25m")
#define es_hide_cursor()  printf("\x1B[?25l")
#define es_show_cursor()  printf("\x1B[?25h")

/*
 * Our y and x start from 0, so need to add 1 for the ANSI escape sequence.
 * This macro evaluates y and x multiple times.
 */
#define es_move(sc, y, x)                                                     \
    do {                                                                      \
        printf("\x1B[%" lu ";%" lu "H", (size_t) (y) + 1, (size_t) (x) + 1);  \
        (sc)->s_y = (y);                                                      \
        (sc)->s_x = (x);                                                      \
    } while (0)

/*
 * y and x coordinates start from 0.
 * y coordinates are vertical.
 * x coordinates are horizontal.
 */
struct screen {
    int fd;
    size_t h;      /* Screen height. */
    size_t w;      /* Screen width. */
    size_t area;   /* Screen area. */
    size_t y;      /* In memory cursor y coordinate. */
    size_t x;      /* In memory cursor x coordinate. */
    size_t s_y;    /* On displayed screen cursor y coordinate. */
    size_t s_x;    /* On displayed screen cursor x coordinate. */
    int highlight; /* Indicates if highlight mode is on. */
#ifdef _WIN32
    HANDLE console_handle;
    int mode_backup; /* Indicates if mode_orig has been saved. */
    /* Used to restore the original console settings. */
    DWORD mode_orig;
#endif
    /* Double buffering: */
    unsigned char *current_mem; /* Mirrors the displayed screen. */
    unsigned char *next_mem;    /* Used to prepare for the next display. */
};

static int hard_clear_display(Screen sc)
{
    es_reset();
    es_blinking_off();
    es_clear();
    es_move(sc, 0, 0);

    if (fflush(stdout))
        debug(return 1);

    return 0;
}

int clear_screen(Screen sc, int mode)
{
    size_t h, w, area;
    unsigned char *t;
#ifdef _WIN32
    CONSOLE_SCREEN_BUFFER_INFO console_info;
#else
    struct winsize ws;
#endif

    if (mode != HARD_CLEAR && mode != SOFT_CLEAR)
        debug(return 1);

#ifdef _WIN32
    if (!GetConsoleScreenBufferInfo(sc->console_handle, &console_info))
        debug(return 1);

    h = console_info.srWindow.Bottom - console_info.srWindow.Top + 1;
    w = console_info.srWindow.Right - console_info.srWindow.Left + 1;
#else
    if (ioctl(sc->fd, TIOCGWINSZ, &ws))
        debug(return 1);

    h = ws.ws_row;
    w = ws.ws_col;
#endif

    if (mult_overflow(h, w))
        debug(return 1);

    area = h * w;

    if (area > sc->area) {
        /*
         * Since the memory only gets increased, it is OK if one realloc
         * succeeds and one fails.
         */
        if ((t = realloc(sc->current_mem, area)) == NULL)
            debug(return 1);

        sc->current_mem = t;

        /* Only clear the extended memory. */
        if (mode != HARD_CLEAR)
            memset(sc->current_mem + sc->area, ' ', area - sc->area);

        if ((t = realloc(sc->next_mem, area)) == NULL)
            debug(return 1);

        sc->next_mem = t;
    }

    if (sc->current_mem == NULL || sc->next_mem == NULL)
        debug(return 1);

    /* Only update area once memmory has been allocated. */

    sc->h = h;
    sc->w = w;
    sc->area = area;

    if (mode == HARD_CLEAR) {
        if (hard_clear_display(sc))
            debug(return 1);

        memset(sc->current_mem, ' ', sc->area);
    }

    memset(sc->next_mem, ' ', sc->area);

    sc->y = 0;
    sc->x = 0;

    return 0;
}

int free_screen(Screen sc)
{
    int r = 0;

    if (sc != NULL) {
        if (hard_clear_display(sc))
            debug(r = 1);

#ifdef _WIN32
        if (sc->mode_backup
            && !SetConsoleMode(sc->console_handle, sc->mode_orig))
            debug(r = 1);
#endif

        free(sc->current_mem);
        free(sc->next_mem);
        free(sc);
    }

    return r;
}

Screen init_screen(void)
{
#ifdef _WIN32
    DWORD mode;
#endif
    Screen sc = NULL;

    if ((sc = calloc(1, sizeof(struct screen))) == NULL)
        debug(goto error);

    /* Do not assume NULL is zero. */
    sc->current_mem = NULL;
    sc->next_mem = NULL;

    if ((sc->fd = fileno(stdout)) == -1)
        debug(goto error);

#ifdef _WIN32
    sc->console_handle = GetStdHandle(STD_OUTPUT_HANDLE);
    if (sc->console_handle == INVALID_HANDLE_VALUE
        || sc->console_handle == NULL)
        debug(goto error);

    if (!GetConsoleMode(sc->console_handle, &mode))
        debug(goto error);

    sc->mode_orig = mode;
    sc->mode_backup = 1;

    /* Set ANSI mode. */
    mode |= ENABLE_PROCESSED_OUTPUT | ENABLE_VIRTUAL_TERMINAL_PROCESSING;

    if (!SetConsoleMode(sc->console_handle, mode))
        debug(goto error);

#endif

    if (clear_screen(sc, HARD_CLEAR))
        debug(goto error);

    return sc;

error:
    free_screen(sc);
    return NULL;
}

/*
 * As only printable chars are added to the memory,
 * bit 7 is used as a highlight indicator.
 */
#define add_ch(ch)                                                            \
    do {                                                                      \
        /* Non-first char out of bounds. */                                   \
        if (sc->y >= y_origin + sub_h || sc->x >= x_origin + sub_w)           \
            return 0;                                                         \
                                                                              \
        sc->next_mem[sc->y * sc->w + sc->x]                                   \
            = sc->highlight ? (ch) | 1 << 7 : (ch);                           \
        if (++sc->x == x_origin + sub_w) {                                    \
            ++sc->y;                                                          \
            sc->x = x_origin;                                                 \
        }                                                                     \
    } while (0)

int soft_clear_sub_screen(
    Screen sc, size_t y_origin, size_t x_origin, size_t sub_h, size_t sub_w)
{
    size_t row_i;

    /* Validate sub screen. */
    if (y_origin + sub_h > sc->h || x_origin + sub_w > sc->w)
        debug(return 1);

    for (row_i = y_origin; row_i < y_origin + sub_h; ++row_i)
        memset(sc->next_mem + row_i * sc->w + x_origin, ' ', sub_w);

    return 0;
}

int sub_screen_print_ch(Screen sc, size_t y_origin, size_t x_origin,
    size_t sub_h, size_t sub_w, char ch)
{
    /*
     * Some characters are displayed as multiple chars, such as tab.
     * The newline character is displayed as a sequence of spaces until
     * the edge is reached.
     * For these characters, so long as the first displayed character
     * is within bounds, no error will result, even if the rest is out
     * of bounds.
     */
    size_t j, y_old;

    /* Validate sub screen. */
    if (y_origin + sub_h > sc->h || x_origin + sub_w > sc->w)
        debug(return 1);

    /* Check if out of bounds. If not, first char will be printed. */
    if (sc->y >= y_origin + sub_h || sc->x >= x_origin + sub_w)
        return 1;

    if (isprint(ch)) {
        add_ch(ch);
    } else if (ch == '\t') {
        j = TAB_SIZE;
        while (j--) add_ch(' ');
    } else if (ch == '\n') {
        /* Clear to the end of the line. */
        y_old = sc->y;
        while (sc->y == y_old) add_ch(' ');
    } else if (iscntrl(ch)) {
        add_ch('^');
        /* Toggle bit 6 (the lowest bit is bit 0). */
        add_ch(ch ^ 1 << 6);
    } else {
        add_ch('?');
    }

    return 0;
}

#undef add_ch

int sub_screen_print_str(Screen sc, size_t y_origin, size_t x_origin,
    size_t sub_h, size_t sub_w, const char *str)
{
    char ch;

    if (str == NULL)
        debug(return 1);

    while ((ch = *str++))
        if (sub_screen_print_ch(sc, y_origin, x_origin, sub_h, sub_w, ch))
            debug(return 1);

    return 0;
}

int print_ch(Screen sc, char ch)
{
    return sub_screen_print_ch(sc, 0, 0, sc->h, sc->w, ch);
}

int print_str(Screen sc, const char *str)
{
    return sub_screen_print_str(sc, 0, 0, sc->h, sc->w, str);
}

int refresh_screen(Screen sc)
{
    size_t y, x; /* In memory. */
    size_t i;
    int s_rev = 0; /* Displayed screen reverse setting is off. */
    unsigned char u;

    es_hide_cursor();
    for (y = 0; y < sc->h; ++y)
        for (x = 0; x < sc->w; ++x) {
            i = y * sc->w + x;

            if ((u = sc->next_mem[i]) != sc->current_mem[i]) {
                /*
                 * Make both memory the same. This is better than switching
                 * memory, as it allows for consecutive refreshes without
                 * clearing in between.
                 */
                sc->current_mem[i] = u;

                /* Optimisation to avoid unneeded moves. */
                if (y != sc->s_y || x != sc->s_x)
                    es_move(sc, y, x);

                if (u & 1 << 7) {
                    /* Highlight. */
                    /* Clear bit 7 for printing on display. */
                    u &= ~(1 << 7);
                    if (!s_rev) {
                        /* Off, so turn on. */
                        es_reverse_on();
                        s_rev = 1;
                    }
                } else {
                    /* No highlight. */
                    if (s_rev) {
                        /* On, so turn off. */
                        es_reverse_off();
                        s_rev = 0;
                    }
                }

                /* Automatically advances the cursor on the display. */
                putchar(u);

                /* Track the displayed cursor's location. */
                if (++sc->s_x == sc->w) {
                    ++sc->s_y;
                    sc->s_x = 0;
                }
            }
        }

    /* Set the final displayed cursor location. */
    if (sc->y != sc->s_y || sc->x != sc->s_x)
        es_move(sc, sc->y, sc->x);

    /* Leave the reverse mode of the displayed screen off. */
    if (s_rev)
        es_reverse_off();

    es_show_cursor();
    if (fflush(stdout))
        debug(return 1);

    return 0;
}

int move(Screen sc, size_t y, size_t x)
{
    if (y >= sc->h || x >= sc->w)
        debug(return 1); /* Out of bounds. */

    sc->y = y;
    sc->x = x;
    return 0;
}

size_t get_screen_height(Screen sc)
{
    return sc->h;
}

size_t get_screen_width(Screen sc)
{
    return sc->w;
}

size_t get_y(Screen sc)
{
    return sc->y;
}

size_t get_x(Screen sc)
{
    return sc->x;
}

void highlight_on(Screen sc)
{
    sc->highlight = 1;
}

void highlight_off(Screen sc)
{
    sc->highlight = 0;
}
