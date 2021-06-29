/*
 * Copyright (c) 2021 Logan Ryan McLintock
 *
 * Permission to use, copy, modify, and distribute this software for any
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
 */

/*
 * README:
 * To compile simply run:
 * $ cc -ansi -g -O3 -Wall -Wextra -pedantic spot.c && mv a.out spot
 * or
 * > cl /Ot /Wall /wd4820 /wd4255 /wd4242 /wd4244 /wd4310 /wd4267 /wd4710 ^
 *   /wd4706 /wd5045 spot.c
 * and place the executable somewhere in your PATH.
 *
 * spot can optionally be compiled with a curses library by setting
 * USE_CURSES to 1, followed by:
 * $ cc -ansi -g -O3 -Wall -Wextra -pedantic spot.c -lncurses && mv a.out spot
 * or
 * > cd C:\Users\logan\Documents\PDCurses-3.9\PDCurses-3.9\wincon
 * > nmake -f Makefile.vc
 * > cd C:\Users\logan\Documents\spot
 * > cl /Ot /Wall /wd4820 /wd4668 /wd4267 /wd4242 /wd4244 /wd4710 /wd5045 ^
     /wd4706 spot.c pdcurses.lib User32.Lib AdvAPI32.Lib ^
 *   /I C:\Users\logan\Documents\PDCurses-3.9\PDCurses-3.9 ^
 *   /link /LIBPATH:C:\Users\logan\Documents\PDCurses-3.9\PDCurses-3.9\wincon
 * Please note that PDCurses does not handle terminal size changes.
 *
 * To use:
 * $ spot [file...]
 *
 * The keybindings are shown below the #include statements.
 */

/* Change this to 1 to compile with ncurses or PDCurses */
#define USE_CURSES 0

#ifdef __linux__
#define _GNU_SOURCE
#endif

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define _CRT_SECURE_NO_WARNINGS
#define _CRT_NONSTDC_NO_DEPRECATE
#endif

#include <sys/types.h>
#include <sys/stat.h>

#ifdef _WIN32
#include <conio.h>
#include <fcntl.h>
#include <io.h>
#if !USE_CURSES
#include <windows.h>
#endif
#else
#if !USE_CURSES
#include <sys/ioctl.h>
#include <termios.h>
#endif
#include <sys/wait.h>
#include <unistd.h>
#endif

#if USE_CURSES
#include <curses.h>
#else
#include <stdio.h>
#endif

#include <fcntl.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <limits.h>
#include <ctype.h>

#define HELP char *help[] = { \
"spot keybindings", \
"^ means the control key, RK is the right key, and LK is the left key.", \
"Commands with descriptions ending with * take an optional command", \
"multiplier prefix ^U n (where n is a positive number).", \
"^[ ?   Display keybindings in new gap buffer", \
"^b     Backward char (left)*", \
"^f     Forward char (right)*", \
"^p     Previous line (up)*", \
"^n     Next line (down)*", \
"^h     Backspace*", \
"^d     Delete*", \
"^[ f   Forward word*", \
"^[ b   Backward word*", \
"^[ u   Uppercase word*", \
"^[ l   Lowercase word*", \
"^q hh  Quote two digit hexadecimal value*", \
"^a     Start of line (home)", \
"^e     End of line", \
"^[ <   Start of gap buffer", \
"^[ >   End of gap buffer", \
"^[ m   Match bracket", \
"^l     Level cursor and redraw screen", \
"^2     Set the mark", \
"^g     Clear the mark or escape the command line", \
"^w     Wipe (cut) region", \
"^o     Wipe region appending on the paste gap buffer", \
"^[ w   Soft wipe (copy) region", \
"^[ o   Soft wipe region appending on the paste gap buffer", \
"^k     Kill (cut) to end of line", \
"^[ k   Kill (cut) to start of line", \
"^y     Yank (paste)", \
"^t     Trim trailing whitespace and clean", \
"^s     Search", \
"^[ n   Search without editing the command line", \
"^x i   Insert file at cursor", \
"^x ^F  Open file in new gap buffer", \
"^r     Rename gap buffer", \
"^x ^s  Save current gap buffer", \
"^x LK  Move left one gap buffer", \
"^x RK  Move right one gap buffer", \
"^[ !   Close current gap buffer without saving", \
"^x ^c  Close editor without saving any gap buffers", \
NULL \
}

/* Initial gap buffer size */
#define INIT_GAPBUF_SIZE BUFSIZ

/* size_t Addition OverFlow test */
#define AOF(a, b) ((a) > SIZE_MAX - (b))
/* size_t Multiplication OverFlow test */
#define MOF(a, b) ((a) && (b) > SIZE_MAX / (a))

/* Takes signed input */
#define ISASCII(x) ((x) >= 0 && (x) <= 127)

/* Converts a lowercase letter to the corresponding control value */
#define C(l) ((l) - 'a' + 1)

/* Control 2 (control spacebar or control @ may work too) */
#define C_2 0

/* Escape key */
#define ESC 27

/* Calculates the gap size */
#define GAPSIZE(b) ((size_t) (b->c - b->g))

/* Converts the cursor pointer to an index */
#define CURSOR_INDEX(b) ((size_t) (b->g - b->a))

/* Converts an index to a pointer */
#define INDEX_TO_POINTER(b, i) (b->a + b->i < b->g ? b->a + b->i \
    : b->c + b->i - (b->g - b->a))

/* Delete gap buffer */
#define DELETEGAPBUF(b) do {b->g = b->a; b->c = b->e; b->r = 1; b->d = 0; \
    b->m = 0; b->mr = 1; b->m_set = 0; b->mod = 1;} while (0)

/* Update settings when a gap buffer is modified */
#define SETMOD(b) do {b->m = 0; b->mr = 1; b->m_set = 0; b->mod = 1;} while (0)

/* No out of bounds or gap size checks are performed */
#define INSERTCH(b, x) do {*b->g++ = x; if (x == '\n') ++b->r;} while(0)
#define DELETECH(b) ++b->c
#define BACKSPACECH(b) if (*--b->g == '\n') --b->r
#define LEFTCH(b) do {*--b->c = *--b->g; if (*b->c == '\n') --b->r;} while(0)
#define RIGHTCH(b) do {if (*b->c == '\n') ++b->r; *b->g++ = *b->c++;} while (0)

/* gap buffer */
struct gapbuf {
    struct gapbuf *prev;        /* Previous gap buffer node */
    char *fn;                   /* Filename */
    char *a;                    /* Start of gap buffer */
    char *g;                    /* Start of gap */
    char *c;                    /* Cursor (to the right of the gap) */
    char *e;                    /* End of gap buffer */
    size_t r;                   /* Row number (starting from 1) */
    size_t sc;                  /* Sticky column number (starting from 0) */
    int sc_set;                 /* Sticky column is set */
    size_t d;                   /* Draw start index (ignores the gap) */
    size_t m;                   /* Mark index (ignores the gap) */
    size_t mr;                  /* Row number at the mark */
    int m_set;                  /* Mark is set */
    int mod;                    /* Gap buffer text has been modified */
    struct gapbuf *next;        /* Next gap buffer node */
};

#if USE_CURSES
#define PRINTCH(ch) ret = addch(ch)
#define MOVE_CURSOR(y, x) ret = move(y, x)
#define GET_CURSOR(y, x) getyx(stdscr, y, x)
#define GET_MAX(y, x) getmaxyx(stdscr, y, x)
#define CLEAR_DOWN() ret = clrtobot()
#define STANDOUT_TO_EOL() ret = chgat(width, A_STANDOUT, 0, NULL)
#else

/*
 * Double buffering terminal graphics
 */
#define ERR -1
#define OK 0

/* ANSI escape sequences */
#define PHY_CLEAR_SCREEN() printf("\033[2J\033[1;1H")
/* Index starts at one. Top left is (1, 1) */
#define PHY_MOVE_CURSOR(y, x) printf("\033[%lu;%luH", (unsigned long) (y), \
    (unsigned long) (x))
#define PHY_ATTR_OFF printf("\033[m")
#define PHY_INVERSE_VIDEO printf("\033[7m")

#define BUF_FREE_SIZE(b) (b->s - b->i)

struct buf {
    char *a;
    size_t i;
    size_t s;
};

#define INIT_BUF_SIZE 512

struct graph {
    char *ns;                   /* Next screen (virtual) */
    char *cs;                   /* Current screen (virtual) */
    size_t vms;                 /* Virtual memory size */
    size_t h;                   /* Screen height (real) */
    size_t w;                   /* Screen width (real) */
    size_t sa;                  /* Screen area (real) */
    size_t v;                   /* Virtual index */
    int hard;                   /* Clear the physical screen */
    int iv;                     /* Inverse video mode (virtual) */
    int phy_iv;                 /* Mirrors the physical inverse video mode */
    struct buf *input;          /* Keyboard input buffer */
#ifndef _WIN32
    struct termios t_orig;      /* Original terminal attributes */
#endif
};

typedef struct graph WINDOW;

/* Global */
WINDOW *stdscr = NULL;

/* Number of spaces used to display a tab (must be at least 1) */
#define TABSIZE 4

/* Index starts from zero. Top left is (0, 0). Sets ret. */
#define MOVE_CURSOR(y, x) do { \
    if ((y) < stdscr->h && (x) < stdscr->w) { \
        stdscr->v = (y) * stdscr->w + (x); \
        ret = OK; \
    } else { \
        ret = ERR; \
    } \
} while (0)

#define CLEAR_DOWN() do { \
    if (stdscr->v < stdscr->sa) { \
        memset(stdscr->ns + stdscr->v, ' ', stdscr->sa - stdscr->v); \
        ret = 0; \
    } else { \
        ret = 1; \
    } \
} while (0)

#define STANDOUT_TO_EOL() do { \
    if (stdscr->v < stdscr->sa) { \
        *(stdscr->ns + stdscr->v) \
            = (char) (*(stdscr->ns + stdscr->v) | 0x80); \
        ++stdscr->v; \
        while (stdscr->v < stdscr->sa && stdscr->v % stdscr->w) { \
            *(stdscr->ns + stdscr->v) \
                = (char) (*(stdscr->ns + stdscr->v) | 0x80); \
            ++stdscr->v; \
        } \
        ret = OK; \
    } else { \
        ret = ERR; \
    } \
} while (0)

#define GET_CURSOR_Y(y) y = stdscr->v / stdscr->w

#define GET_CURSOR_X(x) x = stdscr->v % stdscr->w

#define GET_CURSOR(y, x) do { \
    GET_CURSOR_Y(y); \
    GET_CURSOR_X(x); \
} while (0)

#define GET_MAX(y, x) do { \
    y = stdscr->h; \
    x = stdscr->w; \
} while (0)

/*
 * Prints a character to the virtual screen.
 * Evaluates ch more than once. Sets ret.
 */

/* Will not equal ERR, so there is no problem in testing against ERR */
#define standout() (stdscr->iv = 1)
#define standend() (stdscr->iv = 0)

/* If inverse video mode is on then set the highest bit on the char */
#define IVCH(ch) (stdscr->iv ? (char) ((ch) | 0x80) : (ch))

#define IVON(ch) ((ch) & 0x80)

#define PRINTCH(ch) do { \
    if (stdscr->v < stdscr->sa) { \
        if (isgraph(ch) || ch == ' ') { \
            *(stdscr->ns + stdscr->v++) = IVCH(ch); \
        } else if (ch == '\n') { \
            *(stdscr->ns + stdscr->v++) = IVCH(' '); \
            if (stdscr->v % stdscr->w) \
                stdscr->v = (stdscr->v / stdscr->w + 1) * stdscr->w; \
        } else if (ch == '\t') { \
            memset(stdscr->ns + stdscr->v, IVCH(' '), TABSIZE); \
            stdscr->v += TABSIZE; \
        } else { \
            *(stdscr->ns + stdscr->v++) = IVCH('?'); \
        } \
        ret = OK; \
    } else { \
        ret = ERR; \
    } \
} while (0)

int addnstr(char *str, int n)
{
    char ch;
    int ret;
    while (n-- && (ch = *str++)) {
        PRINTCH(ch);
        if (ret == ERR)
            return ERR;
    }
    return OK;
}

struct buf *init_buf(void)
{
    struct buf *b;
    if ((b = malloc(sizeof(struct buf))) == NULL)
        return NULL;
    if ((b->a = malloc(INIT_BUF_SIZE)) == NULL) {
        free(b);
        return NULL;
    }
    b->s = INIT_BUF_SIZE;
    b->i = 0;
    return b;
}

void free_buf(struct buf *b)
{
    if (b != NULL) {
        free(b->a);
        free(b);
    }
}

int grow_buf(struct buf *b, size_t will_use)
{
    char *t;
    size_t new_s;
    /* Gap is big enough, nothing to do */
    if (will_use <= BUF_FREE_SIZE(b))
        return 0;
    if (MOF(b->s, 2))
        return 1;
    new_s = b->s * 2;
    if (AOF(new_s, will_use))
        new_s += will_use;
    if ((t = realloc(b->a, new_s)) == NULL)
        return 1;
    b->a = t;
    b->s = new_s;
    return 0;
}

int get_screen_size(size_t * height, size_t * width)
{
    /* Gets the screen size */
#ifdef _WIN32
    HANDLE out;
    CONSOLE_SCREEN_BUFFER_INFO info;
    if ((out = GetStdHandle(STD_OUTPUT_HANDLE)) == INVALID_HANDLE_VALUE)
        return 1;
    if (!GetConsoleScreenBufferInfo(out, &info))
        return 1;
    *height = info.srWindow.Bottom - info.srWindow.Top + 1;
    *width = info.srWindow.Right - info.srWindow.Left + 1;
    return 0;
#else
    struct winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1)
        return 1;
    *height = ws.ws_row;
    *width = ws.ws_col;
    return 0;
#endif
}

int erase(void)
{
    size_t new_h, new_w, req_vms;
    char *tmp_ns, *tmp_cs;
    if (get_screen_size(&new_h, &new_w))
        return ERR;

    /* Reset virtual index */
    stdscr->v = 0;

    /* Clear hard or change in screen dimensions */
    if (stdscr->hard || new_h != stdscr->h || new_w != stdscr->w) {
        stdscr->h = new_h;
        stdscr->w = new_w;
        if (MOF(stdscr->h, stdscr->w))
            return ERR;
        stdscr->sa = stdscr->h * stdscr->w;
        /*
         * Add TABSIZE to the end of the virtual screen to
         * allow for characters to be printed off the screen.
         * Assumes that tab consumes the most screen space
         * out of all the characters.
         */
        if (AOF(stdscr->sa, TABSIZE))
            return ERR;
        req_vms = stdscr->sa + TABSIZE;
        /* Bigger screen */
        if (stdscr->vms < req_vms) {
            if ((tmp_ns = malloc(req_vms)) == NULL)
                return ERR;
            if ((tmp_cs = malloc(req_vms)) == NULL) {
                free(tmp_ns);
                return ERR;
            }
            free(stdscr->ns);
            stdscr->ns = tmp_ns;
            free(stdscr->cs);
            stdscr->cs = tmp_cs;
            stdscr->vms = req_vms;
        }
        /*
         * Clear the virtual current screen. No need to erase the
         * virtual screen beyond the physical screen size.
         */
        memset(stdscr->cs, ' ', stdscr->sa);
        PHY_ATTR_OFF;
        stdscr->phy_iv = 0;
        PHY_CLEAR_SCREEN();
        stdscr->hard = 0;
    }
    /* Clear the virtual next screen */
    memset(stdscr->ns, ' ', stdscr->sa);
    return OK;
}

int clear(void)
{
    stdscr->hard = 1;
    return erase();
}

int endwin(void)
{
    int ret = OK;
    /* Screen is not initialised */
    if (stdscr == NULL)
        return ERR;
    PHY_ATTR_OFF;
    PHY_CLEAR_SCREEN();
#ifndef _WIN32
    if (tcsetattr(STDIN_FILENO, TCSANOW, &stdscr->t_orig))
        ret = ERR;
#endif
    free(stdscr->ns);
    free(stdscr->cs);
    free_buf(stdscr->input);
    free(stdscr);
    return ret;
}

WINDOW *initscr(void)
{
#ifdef _WIN32
    HANDLE out;
    DWORD mode;
#else
    struct termios term_orig, term_new;
#endif

    /* Error, screen is already initialised */
    if (stdscr != NULL)
        return NULL;

#ifdef _WIN32
    /* Check input is from a terminal */
    if (!_isatty(_fileno(stdin)))
        return NULL;
    /* Turn on interpretation of VT100-like escape sequences */
    if ((out = GetStdHandle(STD_OUTPUT_HANDLE)) == INVALID_HANDLE_VALUE)
        return NULL;
    if (!GetConsoleMode(out, &mode))
        return NULL;
    if (!SetConsoleMode(out, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING))
        return NULL;
#else
    if (!isatty(STDIN_FILENO))
        return NULL;
    /* Change terminal input to raw and no echo */
    if (tcgetattr(STDIN_FILENO, &term_orig))
        return NULL;
    term_new = term_orig;
    cfmakeraw(&term_new);
    if (tcsetattr(STDIN_FILENO, TCSANOW, &term_new))
        return NULL;
#endif

    if ((stdscr = calloc(1, sizeof(WINDOW))) == NULL) {
#ifndef _WIN32
        tcsetattr(STDIN_FILENO, TCSANOW, &term_orig);
#endif
        return NULL;
    }

    if ((stdscr->input = init_buf()) == NULL) {
#ifndef _WIN32
        tcsetattr(STDIN_FILENO, TCSANOW, &term_orig);
#endif
        free(stdscr);
        stdscr = NULL;
        return NULL;
    }

    if (clear()) {
#ifndef _WIN32
        tcsetattr(STDIN_FILENO, TCSANOW, &term_orig);
#endif
        free_buf(stdscr->input);
        free(stdscr);
        stdscr = NULL;
        return NULL;
    }
#ifndef _WIN32
    stdscr->t_orig = term_orig;
#endif

    return stdscr;
}

void draw_diff(void)
{
    /* Physically draw the screen where the virtual screens differ */
    int in_pos = 0;             /* In position for printing */
    char ch;
    size_t i;
    for (i = 0; i < stdscr->sa; ++i) {
        if ((ch = *(stdscr->ns + i)) != *(stdscr->cs + i)) {
            if (!in_pos) {
                /* Top left corner is (1, 1) not (0, 0) so need to add one */
                PHY_MOVE_CURSOR(i / stdscr->w + 1, i % stdscr->w + 1);
                in_pos = 1;
            }
            /* Inverse video mode */
            if (IVON(ch) && !stdscr->phy_iv) {
                PHY_INVERSE_VIDEO;
                stdscr->phy_iv = 1;
            } else if (!IVON(ch) && stdscr->phy_iv) {
                PHY_ATTR_OFF;
                stdscr->phy_iv = 0;
            }
            putchar(ch & 0x7F);
        } else {
            in_pos = 0;
        }
    }
}

int refresh(void)
{
    char *t;
    draw_diff();
    /* Set physical cursor to the position of the virtual cursor */
    if (stdscr->v < stdscr->sa)
        PHY_MOVE_CURSOR(stdscr->v / stdscr->w + 1,
                        stdscr->v % stdscr->w + 1);
    else
        PHY_MOVE_CURSOR(stdscr->h, stdscr->w);
    /* Swap virtual screens */
    t = stdscr->cs;
    stdscr->cs = stdscr->ns;
    stdscr->ns = t;
    return OK;
}

#ifdef _WIN32
#define GETCH_RAW() _getch()
#else
#define GETCH_RAW() getchar()
#endif

#define GETCH() (stdscr->input->i ? *(stdscr->input->a + --stdscr->input->i) \
    : GETCH_RAW())

int ungetch(int ch)
{
    if (stdscr->input->i == stdscr->input->s)
        if (grow_buf(stdscr->input, 1))
            return EOF;
    return *(stdscr->input->a + stdscr->input->i++) = ch;
}

#define KEY_ENTER 343
#define KEY_DC 330
#define KEY_BACKSPACE 263
#define KEY_LEFT 260
#define KEY_RIGHT 261
#define KEY_UP 259
#define KEY_DOWN 258
#define KEY_HOME 262
#define KEY_END 360

int getch(void)
{
    /* Process multi-char keys */
#ifdef _WIN32
    int x;
    if ((x = GETCH()) != 0xE0)
        return x;
    switch (x = GETCH()) {
    case 'G':
        return KEY_HOME;
    case 'H':
        return KEY_UP;
    case 'K':
        return KEY_LEFT;
    case 'M':
        return KEY_RIGHT;
    case 'O':
        return KEY_END;
    case 'P':
        return KEY_DOWN;
    case 'S':
        return KEY_DC;
    default:
        if (ungetch(x) == EOF)
            return EOF;
        return 0xE0;
    }
#else
    int x, z;
    if ((x = GETCH()) != ESC)
        return x;
    if ((x = GETCH()) != '[') {
        if (ungetch(x) == EOF)
            return EOF;
        return ESC;
    }
    x = GETCH();
    if (x != 'A' && x != 'B' && x != 'C' && x != 'D' && x != 'F'
        && x != 'H' && x != '1' && x != '3' && x != '4') {
        if (ungetch(x) == EOF)
            return EOF;
        if (ungetch('[') == EOF)
            return EOF;
        return ESC;
    }
    switch (x) {
    case 'A':
        return KEY_UP;
    case 'B':
        return KEY_DOWN;
    case 'C':
        return KEY_RIGHT;
    case 'D':
        return KEY_LEFT;
    }
    if ((z = GETCH()) != '~') {
        if (ungetch(z) == EOF)
            return EOF;
        if (ungetch(x) == EOF)
            return EOF;
        if (ungetch('[') == EOF)
            return EOF;
        return ESC;
    }
    switch (x) {
    case '1':
        return KEY_HOME;
    case '3':
        return KEY_DC;
    case '4':
        return KEY_END;
    }
#endif
    return EOF;
}

#endif


struct gapbuf *init_gapbuf(void)
{
    /* Initialises a gap buffer */
    struct gapbuf *b;
    if ((b = malloc(sizeof(struct gapbuf))) == NULL)
        return NULL;
    if ((b->a = malloc(INIT_GAPBUF_SIZE)) == NULL)
        return NULL;
    b->e = b->a + INIT_GAPBUF_SIZE - 1;
    b->g = b->a;
    b->c = b->e;
    /* End of gap buffer char. Cannot be deleted. */
    *b->e = '\0';
    b->r = 1;
    b->sc = 0;
    b->sc_set = 0;
    b->d = 0;
    b->m = 0;
    b->mr = 1;
    b->m_set = 0;
    b->mod = 0;
    b->fn = NULL;
    b->prev = NULL;
    b->next = NULL;
    return b;
}

char *memmatch(char *big, size_t big_len, char *little, size_t little_len)
{
    /* Quick Search algorithm */
#define UCHAR_NUM (UCHAR_MAX + 1)
    unsigned char bad[UCHAR_NUM], *pattern, *q, *q_stop, *q_check;
    size_t to_match, j;
    if (!little_len)
        return big;
    if (little_len > big_len)
        return NULL;
    for (j = 0; j < UCHAR_NUM; ++j)
        bad[j] = little_len + 1;
    pattern = (unsigned char *) little;
    for (j = 0; j < little_len; ++j)
        bad[pattern[j]] = little_len - j;
    q = (unsigned char *) big;
    q_stop = (unsigned char *) big + big_len - little_len;
    while (q <= q_stop) {
        q_check = q;
        pattern = (unsigned char *) little;
        to_match = little_len;
        /* Compare pattern to text */
        while (to_match && *q_check++ == *pattern++)
            --to_match;
        /* Match found */
        if (!to_match)
            return (char *) q;
        /* Jump using the bad character table */
        q += bad[q[little_len]];
    }
    /* Not found */
    return NULL;
}

void free_gapbuf_list(struct gapbuf *b)
{
    /* Frees a gap buffer doubly linked list (can be a single gap buffer) */
    struct gapbuf *t = b, *next;
    if (b == NULL)
        return;
    /* Move to start of list */
    while (t->prev != NULL)
        t = t->prev;
    /* Move forward freeing each node */
    while (t != NULL) {
        next = t->next;
        free(t->fn);
        free(t->a);
        free(t);
        t = next;
    }
}

int grow_gap(struct gapbuf *b, size_t will_use)
{
    /* Grows the gap size of a gap buffer */
    char *t, *new_e;
    size_t gapbuf_size = b->e - b->a + 1;
    if (MOF(gapbuf_size, 2))
        return 1;
    gapbuf_size *= 2;
    if (AOF(gapbuf_size, will_use))
        return 1;
    gapbuf_size += will_use;
    if ((t = malloc(gapbuf_size)) == NULL)
        return 1;
    /* Copy text before the gap */
    memcpy(t, b->a, b->g - b->a);
    /* Copy text after the gap, excluding the end of gap buffer character */
    new_e = t + gapbuf_size - 1;
    memcpy(new_e - (b->e - b->c), b->c, b->e - b->c);
    /* Set end of gap buffer character */
    *new_e = '\0';
    /* Update pointers, indices do not need to be changed */
    b->g = t + (b->g - b->a);
    b->c = new_e - (b->e - b->c);
    b->e = new_e;
    /* Free old memory */
    free(b->a);
    b->a = t;
    return 0;
}

int insert_ch(struct gapbuf *b, char c, size_t mult)
{
    /* Inserts a char mult times into the gap buffer */
    if (GAPSIZE(b) < mult)
        if (grow_gap(b, mult))
            return 1;
    while (mult--)
        INSERTCH(b, c);
    SETMOD(b);
    return 0;
}

int delete_ch(struct gapbuf *b, size_t mult)
{
    /* Deletes mult chars in a gap buffer */
    if (mult > (size_t) (b->e - b->c))
        return 1;
    while (mult--)
        DELETECH(b);
    SETMOD(b);
    return 0;
}

int backspace_ch(struct gapbuf *b, size_t mult)
{
    /* Backspaces mult chars in a gap buffer */
    if (mult > CURSOR_INDEX(b))
        return 1;
    while (mult--)
        BACKSPACECH(b);
    SETMOD(b);
    return 0;
}

int left_ch(struct gapbuf *b, size_t mult)
{
    /* Move the cursor left mult positions */
    if (mult > CURSOR_INDEX(b))
        return 1;
    while (mult--)
        LEFTCH(b);
    return 0;
}

int right_ch(struct gapbuf *b, size_t mult)
{
    /* Move the cursor right mult positions */
    if (mult > (size_t) (b->e - b->c))
        return 1;
    while (mult--)
        RIGHTCH(b);
    return 0;
}

void start_of_gapbuf(struct gapbuf *b)
{
    while (b->a != b->g)
        LEFTCH(b);
}

void end_of_gapbuf(struct gapbuf *b)
{
    while (b->c != b->e)
        RIGHTCH(b);
}

void start_of_line(struct gapbuf *b)
{
    while (b->a != b->g && *(b->g - 1) != '\n')
        LEFTCH(b);
}

void end_of_line(struct gapbuf *b)
{
    while (b->c != b->e && *b->c != '\n')
        RIGHTCH(b);
}

size_t col_num(struct gapbuf *b)
{
    /* Returns the column number, which starts from zero */
    char *q = b->g;
    while (q != b->a && *(q - 1) != '\n')
        --q;
    return b->g - q;
}

int up_line(struct gapbuf *b, size_t mult)
{
    /* Moves the cursor up mult lines */
    size_t col;
    size_t target_row;
    if (b->r > mult)
        target_row = b->r - mult;
    else
        return 1;

    /* Get or set sticky column */
    if (b->sc_set) {
        col = b->sc;
    } else {
        col = col_num(b);
        b->sc = col;
        b->sc_set = 1;
    }

    while (b->g != b->a && b->r != target_row)
        LEFTCH(b);
    start_of_line(b);
    while (b->c != b->e && col && *b->c != '\n') {
        RIGHTCH(b);
        --col;
    }
    return 0;
}

int down_line(struct gapbuf *b, size_t mult)
{
    /* Moves the cursor down mult lines */
    size_t col;
    size_t target_row;
    char *c_backup = b->c;

    if (AOF(b->r, mult))
        return 1;

    target_row = b->r + mult;

    /* Get sticky column */
    if (b->sc_set)
        col = b->sc;
    else
        col = col_num(b);

    /* Try to go down */
    while (b->c != b->e && b->r != target_row)
        RIGHTCH(b);

    if (b->r != target_row) {
        /* Failed, go back */
        while (b->c != c_backup)
            LEFTCH(b);
        return 1;
    }

    /* Set the sticky column if not set (only set upon success) */
    if (!b->sc_set) {
        b->sc = col;
        b->sc_set = 1;
    }

    /* Try to move to the desired column */
    while (b->c != b->e && col && *b->c != '\n') {
        RIGHTCH(b);
        --col;
    }

    return 0;
}

void forward_word(struct gapbuf *b, int mode, size_t mult)
{
    /*
     * Moves forward up to mult words. If mode is 0 then no editing
     * occurs. If mode is 1 then words will be converted to uppercase,
     * and if mode is 3 then words will be converted to lowercase.
     */
    int mod = 0;
    while (b->c != b->e && mult--) {
        /* Eat leading non-alphanumeric characters */
        while (b->c != b->e && ISASCII(*b->c) && !isalnum(*b->c))
            RIGHTCH(b);

        /* Convert letters to uppercase while in the alphanumeric word */
        while (b->c != b->e && ISASCII(*b->c) && isalnum(*b->c)) {
            switch (mode) {
            case 0:
                break;
            case 1:
                if (islower(*b->c)) {
                    *b->c = 'A' + *b->c - 'a';
                    mod = 1;
                }
                break;
            case 2:
                if (isupper(*b->c)) {
                    *b->c = 'a' + *b->c - 'A';
                    mod = 1;
                }
                break;
            }
            RIGHTCH(b);
        }
    }
    if (mod)
        SETMOD(b);
}

void backward_word(struct gapbuf *b, size_t mult)
{
    /* Moves back a maximum of mult words */
    while (b->g != b->a && mult--) {
        /* Eat trailing non-alphanumeric characters */
        while (b->g != b->a && ISASCII(*(b->g - 1))
               && !isalnum(*(b->g - 1)))
            LEFTCH(b);
        /* Go to start of word */
        while (b->g != b->a && ISASCII(*(b->g - 1))
               && isalnum(*(b->g - 1))) {
            LEFTCH(b);
        }
    }
}

int insert_hex(struct gapbuf *b, size_t mult)
{
    /*
     * Inserts a character mult times that was typed as its two digit
     * hexadecimal value.
     */
    int x;
    char ch = 0;
    size_t i = 2;
    while (i--) {
        x = getch();
        if (ISASCII(x) && isxdigit(x)) {
            if (islower(x))
                ch = ch * 16 + x - 'a' + 10;
            else if (isupper(x))
                ch = ch * 16 + x - 'A' + 10;
            else
                ch = ch * 16 + x - '0';
        } else {
            return 1;
        }
    }
    if (insert_ch(b, ch, mult))
        return 1;
    return 0;
}

void trim_clean(struct gapbuf *b)
{
    /*
     * Trims trailing whitespace and deletes any character that is
     * not in {isgraph, ' ', '\t', '\n'}.
     */
    size_t r_backup = b->r;
    size_t col = col_num(b);
    int nl_found = 0;
    int at_eol = 0;
    int mod = 0;

    end_of_gapbuf(b);

    /* Delete to end of text, sparing the first newline character */
    while (b->g != b->a) {
        LEFTCH(b);
        if (ISASCII(*b->c) && isgraph(*b->c)) {
            break;
        } else if (*b->c == '\n' && !nl_found) {
            nl_found = 1;
        } else {
            DELETECH(b);
            mod = 1;
        }
    }

    /* Process text, triming trailing whitespace */
    while (b->g != b->a) {
        LEFTCH(b);
        if (*b->c == '\n') {
            at_eol = 1;
        } else if (ISASCII(*b->c) && isgraph(*b->c)) {
            /* Never delete a graph character */
            at_eol = 0;
        } else if (at_eol) {
            /* Delete any remaining character at the end of the line */
            DELETECH(b);
            mod = 1;
        } else if (*b->c != ' ' && *b->c != '\t' && *b->c != '\n') {
            /* Delete any remaining characters inside the line */
            DELETECH(b);
            mod = 1;
        }
    }

    if (mod)
        SETMOD(b);

    /* Attempt to move back to old position */
    while (b->c != b->e && b->r != r_backup)
        RIGHTCH(b);
    while (b->c != b->e && col && *b->c != '\n') {
        RIGHTCH(b);
        --col;
    }
}

void str_gapbuf(struct gapbuf *b)
{
    /* Prepares a gap buffer so that b->c can be used as a string */
    end_of_gapbuf(b);
    while (b->a != b->g) {
        LEFTCH(b);
        if (*b->c == '\0')
            DELETECH(b);
    }
}

void set_mark(struct gapbuf *b)
{
    b->m = CURSOR_INDEX(b);
    b->mr = b->r;
    b->m_set = 1;
}

void clear_mark(struct gapbuf *b)
{
    b->m = 0;
    b->mr = 1;
    b->m_set = 0;
}

int search(struct gapbuf *b, char *p, size_t n)
{
    /* Forward search gap buffer b for memory p (n chars long) */
    char *q;
    if (b->c == b->e)
        return 1;
    if ((q = memmatch(b->c + 1, b->e - b->c - 1, p, n)) == NULL)
        return 1;
    while (b->c != q)
        RIGHTCH(b);
    return 0;
}

int match_bracket(struct gapbuf *b)
{
    /* Moves the cursor to the corresponding nested bracket */
    int right;
    char orig = *b->c;
    char target;
    size_t depth;
    char *backup = b->c;

    switch (orig) {
    case '(':
        target = ')';
        right = 1;
        break;
    case '{':
        target = '}';
        right = 1;
        break;
    case '[':
        target = ']';
        right = 1;
        break;
    case '<':
        target = '>';
        right = 1;
        break;
    case ')':
        target = '(';
        right = 0;
        break;
    case '}':
        target = '{';
        right = 0;
        break;
    case ']':
        target = '[';
        right = 0;
        break;
    case '>':
        target = '<';
        right = 0;
        break;
    default:
        return 1;
    }

    depth = 1;
    if (right) {
        while (b->c != b->e) {
            RIGHTCH(b);
            if (*b->c == orig)
                ++depth;
            if (*b->c == target)
                if (!--depth)
                    return 0;
        }
        while (b->c != backup)
            LEFTCH(b);
    } else {
        while (b->a != b->g) {
            LEFTCH(b);
            if (*b->c == orig)
                ++depth;
            if (*b->c == target)
                if (!--depth)
                    return 0;
        }
        while (b->c != backup)
            RIGHTCH(b);
    }

    return 1;
}

int copy_region(struct gapbuf *b, struct gapbuf *p)
{
    /*
     * Copies the region from gap buffer b into gap buffer p.
     * The cursor is moved to the end of gap buffer p first.
     */
    char *m_pointer;
    size_t s;
    /* Region does not exist */
    if (!b->m_set)
        return 1;
    /* Region is empty */
    if (b->m == CURSOR_INDEX(b))
        return 1;
    /* Make sure that the cursor is at the end of p */
    end_of_gapbuf(p);
    m_pointer = INDEX_TO_POINTER(b, m);
    /* Mark before cursor */
    if (b->m < CURSOR_INDEX(b)) {
        s = b->g - m_pointer;
        if (s > GAPSIZE(p))
            if (grow_gap(p, s))
                return 1;
        /* Left of gap insert */
        memcpy(p->g, m_pointer, s);
        p->g += s;
        /* Adjust row number */
        p->r += b->r - b->mr;
    } else {
        /* Cursor before mark */
        s = m_pointer - b->c;
        if (s > GAPSIZE(p))
            if (grow_gap(p, s))
                return 1;
        /* Left of gap insert */
        memcpy(p->g, b->c, s);
        p->g += s;
        /* Adjust row number */
        p->r += b->mr - b->r;
    }
    SETMOD(p);
    return 0;
}

int delete_region(struct gapbuf *b)
{
    /* Deletes the region */
    /* Region does not exist */
    if (!b->m_set)
        return 1;
    /* Region is empty */
    if (b->m == CURSOR_INDEX(b))
        return 1;
    /* Mark before cursor */
    if (b->m < CURSOR_INDEX(b)) {
        b->g = b->a + b->m;
        /* Adjust for removed rows */
        b->r = b->mr;
    } else {
        /* Cursor before mark */
        b->c = INDEX_TO_POINTER(b, m);
    }
    SETMOD(b);
    return 0;
}

int cut_region(struct gapbuf *b, struct gapbuf *p)
{
    /*
     * Copies the region from gap buffer b into gap buffer p.
     * The cursor is moved to the end of gap buffer p first.
     */
    if (copy_region(b, p))
        return 1;
    if (delete_region(b))
        return 1;
    return 0;
}

int paste(struct gapbuf *b, struct gapbuf *p, size_t mult)
{
    /*
     * Pastes (inserts) gap buffer p into gap buffer b mult times.
     * Moves the cursor to the end of gap buffer p first.
     */
    size_t num = mult;
    size_t s, ts;
    char *q;
    end_of_gapbuf(p);
    s = p->g - p->a;
    if (!s)
        return 1;
    if (MOF(s, mult))
        return 1;
    ts = s * mult;
    if (ts > GAPSIZE(b))
        if (grow_gap(b, ts))
            return 1;
    q = b->g;
    while (num--) {
        /* Left of gap insert */
        memcpy(q, p->a, s);
        q += s;
    }
    b->g += ts;
    b->r += (p->r - 1) * mult;
    SETMOD(b);
    return 0;
}

int insert_help_line(struct gapbuf *b, char *str)
{
    char ch;
    while ((ch = *str++))
        if (insert_ch(b, ch, 1))
            return 1;
    if (insert_ch(b, '\n', 1))
        return 1;
    return 0;
}

int cut_to_eol(struct gapbuf *b, struct gapbuf *p)
{
    /* Cut to the end of the line */
    if (*b->c == '\n')
        return delete_ch(b, 1);
    set_mark(b);
    end_of_line(b);
    if (cut_region(b, p))
        return 1;
    return 0;
}

int cut_to_sol(struct gapbuf *b, struct gapbuf *p)
{
    /* Cut to the start of the line */
    set_mark(b);
    start_of_line(b);
    if (cut_region(b, p))
        return 1;
    return 0;
}

int filesize(char *fn, size_t * fs)
{
    /* Gets the filesize of a filename */
    struct stat st;
    if (stat(fn, &st))
        return 1;

#ifndef S_ISREG
#define S_ISREG(m) ((m & S_IFMT) == S_IFREG)
#endif

    if (!S_ISREG(st.st_mode))
        return 1;
    if (st.st_size < 0)
        return 1;
    *fs = st.st_size;
    return 0;
}

int insert_file(struct gapbuf *b, char *fn)
{
    /* Inserts a file into the righthand side of the gap */
    size_t fs;
    FILE *fp;
    if (filesize(fn, &fs))
        return 1;
    /* Nothing to do */
    if (!fs)
        return 0;
    if (fs > GAPSIZE(b))
        if (grow_gap(b, fs))
            return 1;
    if ((fp = fopen(fn, "r")) == NULL)
        return 1;
    /* Right of gap insert */
    if (fread(b->c - fs, 1, fs, fp) != fs) {
        fclose(fp);
        return 1;
    }
    b->c -= fs;
    SETMOD(b);
    return 0;
}

int write_file(struct gapbuf *b)
{
    /* Writes a gap buffer to file */
    char *tmp_fn;
    FILE *fp;
    size_t n, b_fn_len;

#ifndef _WIN32
    struct stat st;
#endif

    /* No filename */
    if (b->fn == NULL || !(b_fn_len = strlen(b->fn)))
        return 1;
    if (AOF(b_fn_len, 2))
        return 1;
    if ((tmp_fn = malloc(b_fn_len + 2)) == NULL)
        return 1;
    memcpy(tmp_fn, b->fn, b_fn_len);
    *(tmp_fn + b_fn_len) = '~';
    *(tmp_fn + b_fn_len + 1) = '\0';
    if ((fp = fopen(tmp_fn, "wb")) == NULL) {
        free(tmp_fn);
        return 1;
    }
    /* Before gap */
    n = b->g - b->a;
    if (fwrite(b->a, 1, n, fp) != n) {
        free(tmp_fn);
        fclose(fp);
        return 1;
    }
    /* After gap, excluding the last character */
    n = b->e - b->c;
    if (fwrite(b->c, 1, n, fp) != n) {
        free(tmp_fn);
        fclose(fp);
        return 1;
    }
    if (fclose(fp)) {
        free(tmp_fn);
        return 1;
    }
#ifndef _WIN32
    /* If original file exists, then apply its permissions to the new file */
    if (!stat(b->fn, &st) && S_ISREG(st.st_mode)
        && chmod(tmp_fn, st.st_mode & 0777)) {
        free(tmp_fn);
        return 1;
    }
#endif

#ifdef _WIN32
    /* rename does not overwrite files */
    errno = 0;
    if (remove(b->fn) && errno != ENOENT) {
        free(tmp_fn);
        return 1;
    }
#endif

    /* Atomic on POSIX systems */
    if (rename(tmp_fn, b->fn)) {
        free(tmp_fn);
        return 1;
    }

    free(tmp_fn);
    b->mod = 0;
    return 0;
}

int init_ncurses(void)
{
    /* Starts ncurses */
    if (initscr() == NULL)
        return 1;

#if USE_CURSES
    if (raw() == ERR) {
        endwin();
        return 1;
    }
    if (noecho() == ERR) {
        endwin();
        return 1;
    }
    if (keypad(stdscr, TRUE) == ERR) {
        endwin();
        return 1;
    }
#endif

    return 0;
}

void centre_cursor(struct gapbuf *b, int text_height)
{
    /*
     * Sets draw start so that the cursor might be in the middle
     * of the screen. Does not handle long lines.
     */
    char *q;
    int up;
    up = text_height / 2;
    if (!up)
        up = 1;
    if (b->g == b->a) {
        b->d = 0;
        return;
    }
    q = b->g - 1;
    while (q != b->a) {
        if (*q == '\n' && !--up)
            break;
        --q;
    }

    /* Move to start of line */
    if (q != b->a)
        ++q;

    b->d = q - b->a;
}

int draw_screen(struct gapbuf *b, struct gapbuf *cl, int cl_active,
                char **sb, size_t * sb_s, int rv, int *req_centre,
                int *req_clear)
{
    /* Draws the text and command line gap buffers to the screen */
    char *q, ch;
    int height, width;          /* Screen size */
    size_t h, w;                /* Screen size as size_t */
    int cy = 0, cx = 0;         /* Final cursor position */
    int y, x;                   /* Changing cursor position */
    int centred = 0;            /* Indicates if centreing has occurred */
    int ret = 0;                /* Macro "return value" */

  draw_start:
    if (*req_clear) {
        if (clear() == ERR)
            return 1;
        *req_clear = 0;
    } else {
        if (erase() == ERR)
            return 1;
    }

    /* erase() updates stdscr->h and stdscr->w, so best to call afterwards */
    GET_MAX(height, width);

    if (height < 1 || width < 1)
        return 1;
    h = (size_t) height;
    w = (size_t) width;

    /* Cursor is above the screen */
    if (*req_centre || b->c < INDEX_TO_POINTER(b, d)) {
        centre_cursor(b, height >= 3 ? height - 2 : height);
        *req_centre = 0;
        centred = 1;
    }

    /* Commence from draw start */
    q = b->d + b->a;
    /* Start highlighting if mark is before draw start */
    if (b->m_set && b->m < b->d)
        if (standout() == ERR)
            return 1;

    /* Before gap */
    while (q != b->g) {
        /* Mark is on screen before cursor */
        if (b->m_set && q == INDEX_TO_POINTER(b, m))
            if (standout() == ERR)
                return 1;

#define DISPLAYCH(ch) (isgraph(ch) || ch == ' ' || ch == '\t' || ch == '\n' \
    ? ch : (ch == '\0' ? '~' : '?'))

        ch = *q;
        ch = DISPLAYCH(ch);
        PRINTCH(ch);
        GET_CURSOR(y, x);
        if ((height >= 3 && y >= height - 2) || ret == ERR) {
            /* Cursor out of text portion of the screen */
            if (!centred) {
                centre_cursor(b, height >= 3 ? height - 2 : height);
                centred = 1;
                goto draw_start;
            } else {
                /*
                 * Do no attempt to centre again, draw from the cursor instead.
                 * This is to accommodate lines that are very long. Please note
                 * that the screen may have changed size since it was centred,
                 * but this is OK.
                 */
                b->d = CURSOR_INDEX(b);
                goto draw_start;
            }
        }
        ++q;
    }

    /* Don't highlight the cursor itself */
    if (b->m_set) {
        if (INDEX_TO_POINTER(b, m) > b->c) {
            if (standout() == ERR)
                return 1;
        } else {
            /* Stop highlighting */
            if (standend() == ERR)
                return 1;
        }
    }

    /* Record cursor position */
    if (!cl_active)
        GET_CURSOR(cy, cx);
    /* After gap */
    q = b->c;
    while (q <= b->e) {
        /* Mark is after cursor */
        if (b->m_set && q == INDEX_TO_POINTER(b, m))
            if (standend() == ERR)
                return 1;
        ch = *q;
        ch = DISPLAYCH(ch);
        PRINTCH(ch);
        GET_CURSOR(y, x);
        if ((height >= 3 && y >= height - 2) || ret == ERR)
            break;
        ++q;
    }

    if (height >= 3) {
        /* Status bar */
        MOVE_CURSOR(h - 2, 0);
        if (ret == ERR)
            return 1;

        /* sb_s needs to include the '\0' terminator */
        if (w >= *sb_s) {
            free(*sb);
            *sb = NULL;
            *sb_s = 0;
            if (AOF(w, 1))
                return 1;
            if ((*sb = malloc(w + 1)) == NULL)
                return 1;
            *sb_s = w + 1;
        }
        if (snprintf
            (*sb, *sb_s, "%c%c %s (%lu,%lu) %02X", rv ? '!' : ' ',
             b->mod ? '*' : ' ', b->fn, (unsigned long) b->r,
             (unsigned long) col_num(b), (unsigned char) *b->c) < 0)
            return 1;
        if (addnstr(*sb, width) == ERR)
            return 1;
        /* Highlight status bar */
        MOVE_CURSOR(h - 2, 0);
        if (ret == ERR)
            return 1;
        STANDOUT_TO_EOL();
        if (ret == ERR)
            return 1;

        /* Command line gap buffer */
        MOVE_CURSOR(h - 1, 0);
        if (ret == ERR)
            return 1;

      cl_draw_start:
        CLEAR_DOWN();
        if (ret == ERR)
            return 1;

        /* Commence from draw start */
        q = cl->d + cl->a;
        /* Start highlighting if mark is before draw start */
        if (cl->m_set && cl->m < cl->d)
            if (standout() == ERR)
                return 1;

        /* Before gap */
        while (q != cl->g) {
            /* Mark is on screen before cursor */
            if (cl->m_set && q == INDEX_TO_POINTER(cl, m))
                if (standout() == ERR)
                    return 1;
            ch = *q;
            ch = DISPLAYCH(ch);
            PRINTCH(ch);
            if (ret == ERR) {
                /* Draw from the cursor */
                cl->d = cl->g - cl->a;
                goto cl_draw_start;
            }
            ++q;
        }

        /* Don't highlight the cursor itself */
        if (cl->m_set) {
            if (INDEX_TO_POINTER(cl, m) > cl->c) {
                if (standout() == ERR)
                    return 1;
            } else {
                /* Stop highlighting */
                if (standend() == ERR)
                    return 1;
            }
        }

        /* Record cursor position */
        if (cl_active)
            GET_CURSOR(cy, cx);
        /* After gap */
        q = cl->c;
        while (q <= cl->e) {
            /* Mark is after cursor */
            if (cl->m_set && q == INDEX_TO_POINTER(cl, m))
                if (standend() == ERR)
                    return 1;
            ch = *q;
            ch = DISPLAYCH(ch);
            PRINTCH(ch);
            if (ret == ERR)
                break;
            ++q;
        }
    }

    /* Position cursor */
    MOVE_CURSOR((size_t) cy, (size_t) cx);
    if (ret == ERR)
        return 1;
    if (refresh() == ERR)
        return 1;
    return 0;
}

int rename_gapbuf(struct gapbuf *b, char *new_fn)
{
    /* Sets the filename fn associated with gap buffer b to new_fn */
    char *t;
    if ((t = strdup(new_fn)) == NULL)
        return 1;
    free(b->fn);
    b->fn = t;
    return 0;
}

struct gapbuf *new_gapbuf(struct gapbuf *b, char *fn)
{
    /*
     * Creates a new gap buffer to the right of gap buffer b in the doubly
     * linked list and sets the associated filename to fn. The file will be
     * loaded into the gap buffer if it exists.
     */
    struct gapbuf *t;
    if ((t = init_gapbuf()) == NULL)
        return NULL;

#ifndef F_OK
#define F_OK 0
#endif

    if (fn != NULL) {
        if (!access(fn, F_OK)) {
            /* File exists */
            if (insert_file(t, fn)) {
                free_gapbuf_list(t);
                return NULL;
            }
            /* Clear modification indicator */
            t->mod = 0;
        }
        if (rename_gapbuf(t, fn)) {
            free_gapbuf_list(t);
            return NULL;
        }
    }

    /* Link in the new node */
    if (b != NULL) {
        if (b->next != NULL) {
            b->next->prev = t;
            t->next = b->next;
        }
        b->next = t;
        t->prev = b;
    }
    return t;
}

struct gapbuf *kill_gapbuf(struct gapbuf *b)
{
    /*
     * Kills (frees and unlinks) gap buffer b from the doubly linked list and
     * returns the gap buffer to the left (if present) or right (if present).
     */
    struct gapbuf *t = NULL;
    if (b == NULL)
        return NULL;
    /* Unlink b */
    if (b->prev != NULL) {
        t = b->prev;
        b->prev->next = b->next;
    }
    if (b->next != NULL) {
        if (t == NULL)
            t = b->next;
        b->next->prev = b->prev;
    }
    /* Isolate b */
    b->prev = NULL;
    b->next = NULL;

    /* Free b */
    free_gapbuf_list(b);

    return t;
}

int main(int argc, char **argv)
{
    int ret = 0;                /* Return value of text editor */
    int rv = 0;                 /* Internal function return value */
    int running = 1;            /* Text editor is on */
    int x;                      /* Read char */
    /* Current gap buffer of the doubly linked list of text gap buffers */
    struct gapbuf *b = NULL;
    struct gapbuf *cl = NULL;   /* Command line gap buffer */
    struct gapbuf *z;           /* Shortcut to the active gap buffer */
    struct gapbuf *p = NULL;    /* Paste gap buffer */
    char *sb = NULL;            /* Status bar */
    size_t sb_s = 0;            /* Status bar size */
    int req_centre = 0;         /* User requests cursor centreing */
    int req_clear = 0;          /* User requests screen clearing */
    int cl_active = 0;          /* Command line is being used */
    /* Operation for which command line is being used */
    char operation = ' ';
    size_t mult;                /* Command multiplier */
    /* Persist the sticky column (used for repeated up or down) */
    int persist_sc = 0;
    int i;
    /* For displaying keybindings help */
    HELP;
    char **h;

#ifdef _WIN32
    if (_setmode(_fileno(stdin), _O_BINARY) == -1)
        return 1;
    if (_setmode(_fileno(stdout), _O_BINARY) == -1)
        return 1;
    if (_setmode(_fileno(stderr), _O_BINARY) == -1)
        return 1;
#endif

    if (argc <= 1) {
        if ((b = new_gapbuf(NULL, NULL)) == NULL) {
            ret = 1;
            goto clean_up;
        }
    } else {
        for (i = 1; i < argc; ++i) {
            if ((b = new_gapbuf(b, *(argv + i))) == NULL) {
                ret = 1;
                goto clean_up;
            }
        }
    }

    if ((cl = init_gapbuf()) == NULL) {
        ret = 1;
        goto clean_up;
    }

    if ((p = init_gapbuf()) == NULL) {
        ret = 1;
        goto clean_up;
    }

    if (init_ncurses()) {
        ret = 1;
        goto clean_up;
    }

    while (running) {
      top:
        if (draw_screen
            (b, cl, cl_active, &sb, &sb_s, rv, &req_centre, &req_clear)) {
            ret = 1;
            goto clean_up;
        }
        /* Clear internal return value */
        rv = 0;

        /* Active gap buffer */
        if (cl_active)
            z = cl;
        else
            z = b;

        /* Update sticky column */
        if (persist_sc) {
            /* Turn off persist status, but do not clear sticky column yet */
            persist_sc = 0;
        } else {
            /* Clear stick column */
            z->sc = 0;
            z->sc_set = 0;
        }

        x = getch();

        mult = 0;
        if (x == C('u')) {
            x = getch();
            while (ISASCII(x) && isdigit(x)) {
                if (MOF(mult, 10)) {
                    rv = 1;
                    goto top;
                }
                mult *= 10;
                if (AOF(mult, x - '0')) {
                    rv = 1;
                    goto top;
                }
                mult += x - '0';
                x = getch();
            }
        }
        /* mult cannot be zero */
        if (!mult)
            mult = 1;

        /* Map Carriage Returns to Line Feeds */
        if (x == '\r' || x == KEY_ENTER)
            x = '\n';

        if (cl_active && x == '\n') {
            switch (operation) {
            case 'i':
                str_gapbuf(cl);
                rv = insert_file(b, cl->c);
                break;
            case 's':
                start_of_gapbuf(cl);
                rv = search(b, cl->c, cl->e - cl->c);
                break;
            case 'r':
                str_gapbuf(cl);
                rv = rename_gapbuf(b, cl->c);
                break;
            case 'n':
                str_gapbuf(cl);
                if (new_gapbuf(b, cl->c) == NULL) {
                    rv = 1;
                } else {
                    b = b->next;
                    rv = 0;
                }
                break;
            }
            cl_active = 0;
            operation = ' ';
            goto top;
        }
        switch (x) {
        case C_2:
            set_mark(z);
            break;
        case C('g'):
            if (z->m_set) {
                /* Clear mark if set */
                clear_mark(z);
            } else {
                /* Or quit the command line */
                cl_active = 0;
                operation = ' ';
            }
            break;
        case C('h'):
        case 127:
        case KEY_BACKSPACE:
            rv = backspace_ch(z, mult);
            break;
        case C('b'):
        case KEY_LEFT:
            rv = left_ch(z, mult);
            break;
        case C('f'):
        case KEY_RIGHT:
            rv = right_ch(z, mult);
            break;
        case C('p'):
        case KEY_UP:
            rv = up_line(z, mult);
            persist_sc = 1;
            break;
        case C('n'):
        case KEY_DOWN:
            rv = down_line(z, mult);
            persist_sc = 1;
            break;
        case C('a'):
        case KEY_HOME:
            start_of_line(z);
            break;
        case C('e'):
        case KEY_END:
            end_of_line(z);
            break;
        case C('d'):
        case KEY_DC:
            rv = delete_ch(z, mult);
            break;
        case C('l'):
            req_centre = 1;
            req_clear = 1;
            break;
        case C('s'):
            /* Search */
            DELETEGAPBUF(cl);
            operation = 's';
            cl_active = 1;
            break;
        case C('r'):
            /* Rename gap buffer */
            DELETEGAPBUF(cl);
            operation = 'r';
            cl_active = 1;
            break;
        case C('w'):
            DELETEGAPBUF(p);
            rv = cut_region(z, p);
            break;
        case C('o'):
            /* Cut, inserting at end of paste gap buffer */
            rv = cut_region(z, p);
            break;
        case C('y'):
            rv = paste(z, p, mult);
            break;
        case C('k'):
            DELETEGAPBUF(p);
            rv = cut_to_eol(z, p);
            break;
        case C('t'):
            trim_clean(z);
            break;
        case C('q'):
            rv = insert_hex(z, mult);
            break;
        case C('x'):
            switch (x = getch()) {
            case C('c'):
                running = 0;
                break;
            case C('s'):
                rv = write_file(z);
                break;
            case 'i':
                /* Insert file at the cursor */
                DELETEGAPBUF(cl);
                operation = 'i';
                cl_active = 1;
                break;
            case C('f'):
                /* New gap buffer */
                DELETEGAPBUF(cl);
                operation = 'n';
                cl_active = 1;
                break;
            case KEY_LEFT:
                if (b->prev != NULL)
                    b = b->prev;
                else
                    rv = 1;
                break;
            case KEY_RIGHT:
                if (b->next != NULL)
                    b = b->next;
                else
                    rv = 1;
                break;
            }
            break;
        case ESC:
            switch (x = getch()) {
            case 'n':
                /* Search without editing the command line */
                start_of_gapbuf(cl);
                rv = search(z, cl->c, cl->e - cl->c);
                break;
            case 'm':
                rv = match_bracket(z);
                break;
            case 'w':
                DELETEGAPBUF(p);
                rv = copy_region(z, p);
                clear_mark(z);
                break;
            case 'o':
                /* Copy, inserting at end of paste gap buffer */
                rv = copy_region(z, p);
                break;
            case '!':
                /* Close editor if last gap buffer is killed */
                if ((b = kill_gapbuf(b)) == NULL)
                    running = 0;
                break;
            case 'k':
                DELETEGAPBUF(p);
                rv = cut_to_sol(z, p);
                break;
            case 'b':
                backward_word(z, mult);
                break;
            case 'f':
                forward_word(z, 0, mult);
                break;
            case 'u':
                forward_word(z, 1, mult);
                break;
            case 'l':
                forward_word(z, 2, mult);
                break;
            case '<':
                start_of_gapbuf(z);
                break;
            case '>':
                end_of_gapbuf(z);
                break;
            case '?':
                if (new_gapbuf(b, NULL) == NULL) {
                    rv = 1;
                } else {
                    b = b->next;
                    h = help;
                    while (*h != NULL)
                        if ((rv = insert_help_line(b, *h++)))
                            break;
                }
                break;
            }
            break;
        default:
            if (ISASCII(x)
                && (isgraph(x) || x == ' ' || x == '\t' || x == '\n')) {
                rv = insert_ch(z, x, mult);
            }
            break;
        }
    }

  clean_up:
    free_gapbuf_list(b);
    free_gapbuf_list(cl);
    free_gapbuf_list(p);
    free(sb);
    if (stdscr != NULL)
        if (endwin() == ERR)
            ret = 1;

    return ret;
}
