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
#include <conio.h>
#include <fcntl.h>
#include <io.h>
#else
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>
#endif

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

#include "alias.h"
#include "buf.h"
#include "debug.h"
#include "input.h"

#define INIT_BUF_ELEMENTS 512

struct input {
    FILE *fp;     /* File pointer (stream). */
    int fd;       /* File descriptor. */
    int binary;   /* Input is in binary mode. */
    int is_tty;   /* Input is a TTY. */
    int blocking; /* BLOCKING or NON_BLOCKING_TTY. */
    int cooking;  /* RAW, COOKED, or DOUBLE_COOKED. */
    /* Key map used for the second level of cooking. */
    const struct key_map *second_level_km;
#ifndef _WIN32
    int terminal_backup;   /* Indicates if t_orig has been saved. */
    struct termios t_orig; /* Used to restore the terminal settings. */
#endif
    Buf tmp_buf;           /* Raw unsigned char temporary buffer. */
    Buf raw_buf;           /* Raw unsigned char unget buffer. */
    Buf cooked_buf;        /* Cooked int unget buffer. */
    Buf double_cooked_buf; /* Double-cooked int unget buffer. */
};

typedef int (*Get_ch_func)(Input, int *);
typedef int (*Unget_ch_func)(Input, int);

int free_input(Input ip)
{
    int r = 0;

    if (ip != NULL) {
        if (ip->fp != NULL && ip->fp != stdin)
            if (fclose(ip->fp))
                debug(r = 1);

#ifndef _WIN32
        /* Restore terminal settings. */
        if (ip->terminal_backup && tcsetattr(ip->fd, TCSANOW, &ip->t_orig))
            debug(r = 1);
#endif

        free_buf(ip->tmp_buf);
        free_buf(ip->raw_buf);
        free_buf(ip->cooked_buf);
        free_buf(ip->double_cooked_buf);
        free(ip);
    }

    return r;
}

static int init_input(Input *y, FILE *fp, const char *fn, int blocking,
    int cooking, const struct key_map *second_level_km)
{
    /*
     * Since this function is not accessible, fp should be open for reading.
     * blocking: Must be exactly one of BLOCKING or NON_BLOCKING_TTY.
     * cooking: Must be exactly one of; RAW, COOKED, or DOUBLE_COOKED.
     * second_level_km: Must be provided (non-NULL) when DOUBLE_COOKED.
     *     Otherwise it must be NULL.
     */

    Input ip = NULL;
#ifndef _WIN32
    struct termios t_new;
#endif

    if ((ip = calloc(1, sizeof(struct input))) == NULL)
        debug(goto error);

    /* Do not assume that NULL is zero. */
    ip->fp = NULL;
    ip->raw_buf = NULL;
    ip->cooked_buf = NULL;
    ip->double_cooked_buf = NULL;

    /* Enforce one or the other of fp and fn, but not both. */
    if ((fp == NULL && fn == NULL) || (fp != NULL && fn != NULL))
        debug(goto error);

    if (fp != NULL) {
        ip->fp = fp;
    } else {
        errno = 0;
#ifdef _WIN32
        if (fopen_s(&ip->fp, fn, "rb"))
#else
        if ((ip->fp = fopen(fn, "rb")) == NULL)
#endif
        {
            if (errno == ENOENT) {
                free_input(ip);
                return ENOENT;
            }

            debug(goto error);
        }

        ip->binary = 1;
    }

#ifdef _WIN32
    if (!ip->binary) {
        /* Put existing fp into binary mode. */
        if (_setmode(ip->fd, _O_BINARY) == -1)
            debug(goto error);

        ip->binary = 1;
    }
#endif

    if ((ip->fd = fileno(ip->fp)) == -1)
        debug(goto error);

    errno = 0;
    if (isatty(ip->fd)) {
        ip->is_tty = 1;
    } else {
        if (errno == EBADF)
            debug(goto error);
    }

    switch (blocking) {
    case BLOCKING:
        ip->blocking = BLOCKING;
        break;
    case NON_BLOCKING_TTY:
        if (ip->is_tty)
            ip->blocking = NON_BLOCKING_TTY;
        else
            ip->blocking = BLOCKING;
        break;
    default:
        debug(goto error);
    }

    switch (cooking) {
    case RAW:
        ip->cooking = RAW;
        if (second_level_km != NULL)
            debug(goto error);

        break;
    case COOKED:
        ip->cooking = COOKED;
        if (second_level_km != NULL)
            debug(goto error);

        break;
    case DOUBLE_COOKED:
        ip->cooking = DOUBLE_COOKED;
        if (second_level_km == NULL)
            debug(goto error);

        ip->second_level_km = second_level_km;
        break;
    default:
        debug(goto error);
    }

#ifndef _WIN32
    if (ip->is_tty) {
        if (tcgetattr(ip->fd, &ip->t_orig))
            debug(goto error);

        t_new = ip->t_orig;

        cfmakeraw(&t_new);

        if (tcsetattr(ip->fd, TCSANOW, &t_new))
            debug(goto error);

        ip->terminal_backup = 1;
    }
#endif

    if ((ip->tmp_buf = init_buf(INIT_BUF_ELEMENTS, sizeof(unsigned char)))
        == NULL)
        debug(goto error);

    if ((ip->raw_buf = init_buf(INIT_BUF_ELEMENTS, sizeof(unsigned char)))
        == NULL)
        debug(goto error);

    if (ip->cooking != RAW
        && (ip->cooked_buf = init_buf(INIT_BUF_ELEMENTS, sizeof(int))) == NULL)
        debug(goto error);

    if (ip->cooking == DOUBLE_COOKED
        && (ip->double_cooked_buf = init_buf(INIT_BUF_ELEMENTS, sizeof(int)))
            == NULL)
        debug(goto error);

    *y = ip;
    return 0;

error:
    free_input(ip);
    debug(return -1);
}

int init_input_stdin(Input *ip, int blocking, int cooking,
    const struct key_map *second_level_km)
{
    return init_input(ip, stdin, NULL, blocking, cooking, second_level_km);
}

int init_input_fn(Input *ip, const char *fn, int blocking, int cooking,
    const struct key_map *second_level_km)
{
    return init_input(ip, NULL, fn, blocking, cooking, second_level_km);
}

static int get_raw_ch(Input ip, int *ch)
{
    unsigned char u;
#ifndef _WIN32
    int num_bytes, i, x;
#endif

    if (pop(ip->raw_buf, &u) == 0) {
        *ch = u;
        return 0;
    }
#ifdef _WIN32
    if (ip->is_tty) {
        if (ip->blocking == NON_BLOCKING_TTY) {
            if (_kbhit())
                *ch = _getch();
            else
                return WOULD_BLOCK;
        } else {
            *ch = _getch();
        }
    }
#else
    if (ip->blocking == NON_BLOCKING_TTY) {
        /* Get the number of bytes that are ready for reading. */
        if (ioctl(ip->fd, FIONREAD, &num_bytes) == -1)
            debug(return 1);

        if (num_bytes) {
            i = num_bytes;
            while (i--) {
                x = getc(ip->fp);
                if (x == EOF)
                    debug(return 1);

                if (push(ip->tmp_buf, &x))
                    debug(return 1);
            }

            i = num_bytes;
            while (i--) {
                /* Reverse order of characters. */
                if (pop(ip->tmp_buf, &u))
                    debug(return 1);

                if (push(ip->raw_buf, &u))
                    debug(return 1);
            }

            if (pop(ip->raw_buf, &u))
                debug(return 1);

            *ch = u;
            return 0;
        } else {
            return WOULD_BLOCK;
        }
    }
#endif
    else {
        *ch = getc(ip->fp);
        if (*ch == EOF && (ferror(ip->fp) || !feof(ip->fp)))
            debug(return 1);
    }

    return 0;
}

static int unget_raw_ch(Input ip, int ch)
{
    return push(ip->raw_buf, &ch);
}

static int unget_cooked_ch(Input ip, int ch)
{
    return push(ip->cooked_buf, &ch);
}

static int unget_double_cooked_ch(Input ip, int ch)
{
    return push(ip->double_cooked_buf, &ch);
}

static int cook_input(Input ip, Get_ch_func gf, Unget_ch_func uf,
    Buf this_level_unget_buf, const struct key_map *km, int *ch)
{
    int r;
    int partial_match;
    int x[MAX_SEQ];
    /*
     * Index of next free element in x.
     * The index of the last used element is x_i - 1.
     */
    size_t x_i = 0;
    size_t i, j;

    if (pop(this_level_unget_buf, ch) == 0)
        return 0;

    do {
        partial_match = 0;

        if (x_i == MAX_SEQ)
            break;

        /* Read a character. */
        r = (*gf)(ip, x + x_i);
        if (r == 1)
            return 1;
        else if (r == WOULD_BLOCK)
            break;
        else
            ++x_i;

        i = 0;

        /* key equal to zero marks the end of the mapping table. */
        while (km[i].key != 0) {
            /* Check the read sequence for a match in the mapping row. */
            for (j = 0; j < x_i; ++j)
                if (x[j] != km[i].seq[j])
                    goto next_row; /* No match on this row. */

            /*
             * See if end of sequence. Zero does not terminate when
             * in the first element of the sequence.
             */
            if (j && km[i].seq[j] == 0) {
                /* Full match. All characters in x will be eaten. */
                *ch = km[i].key;
                return 0;
            } else {
                /* Need to keep reading. This is a table-wide indicator. */
                partial_match = 1;
            }

        next_row:
            /* Advance to the next row in the mapping table. */
            ++i;
        }
    } while (partial_match);

    if (!x_i)
        return WOULD_BLOCK;

    /* No match. So unget all characters except for the the first. */
    while (x_i > 1)
        if ((*uf)(ip, x[x_i-- - 1]))
            debug(return 1);

    /* Check. */
    if (x_i != 1)
        debug(return 1);

    /* Return the first character with no extra cooking. */
    *ch = x[x_i - 1];
    return 0;
}

static int get_cooked_ch(Input ip, int *ch)
{
    const struct key_map km[] = {
        { { 0x08 }, KEY_BACKSPACE }, /* Only one byte, but inconsistent. */
        { { 0xE0, 0x4B }, KEY_LEFT },
        { { 0xE0, 0x4D }, KEY_RIGHT },
        { { 0xE0, 0x48 }, KEY_UP },
        { { 0xE0, 0x50 }, KEY_DOWN },
        { { 0xE0, 0x47 }, KEY_HOME },
        { { 0xE0, 0x4F }, KEY_END },
        { { 0xE0, 0x52 }, KEY_INSERT },
        { { 0xE0, 0x53 }, KEY_DELETE },
        { { 0xE0, 0x49 }, KEY_PAGE_UP },
        { { 0xE0, 0x51 }, KEY_PAGE_DOWN },
        { { 0x00, 0x3B }, KEY_F1 },
        { { 0x00, 0x3C }, KEY_F2 },
        { { 0x00, 0x3D }, KEY_F3 },
        { { 0x00, 0x3E }, KEY_F4 },
        { { 0x00, 0x3F }, KEY_F5 },
        { { 0x00, 0x40 }, KEY_F6 },
        { { 0x00, 0x41 }, KEY_F7 },
        { { 0x00, 0x42 }, KEY_F8 },
        { { 0x00, 0x43 }, KEY_F9 },
        { { 0x00, 0x44 }, KEY_F10 },
        { { 0xE0, 0x85 }, KEY_F11 },
        { { 0xE0, 0x86 }, KEY_F12 },
        { { 0xE0, 0x73 }, CTRL_LEFT },
        { { 0xE0, 0x74 }, CTRL_RIGHT },
        { { 0xE0, 0x8D }, CTRL_UP },
        { { 0xE0, 0x91 }, CTRL_DOWN },

        { { 0x7F }, KEY_BACKSPACE },
        { { 0x1B, 0x5B, 0x44 }, KEY_LEFT },
        { { 0x1B, 0x5B, 0x43 }, KEY_RIGHT },
        { { 0x1B, 0x5B, 0x41 }, KEY_UP },
        { { 0x1B, 0x5B, 0x42 }, KEY_DOWN },
        { { 0x1B, 0x5B, 0x48 }, KEY_HOME },
        { { 0x1B, 0x5B, 0x46 }, KEY_END },
        { { 0x1B, 0x5B, 0x32, 0x7E }, KEY_INSERT },
        { { 0x1B, 0x5B, 0x33, 0x7E }, KEY_DELETE },
        { { 0x1B, 0x5B, 0x35, 0x7E }, KEY_PAGE_UP },
        { { 0x1B, 0x5B, 0x36, 0x7E }, KEY_PAGE_DOWN },
        { { 0x1B, 0x4F, 0x50 }, KEY_F1 },
        { { 0x1B, 0x4F, 0x51 }, KEY_F2 },
        { { 0x1B, 0x4F, 0x52 }, KEY_F3 },
        { { 0x1B, 0x4F, 0x53 }, KEY_F4 },
        { { 0x1B, 0x5B, 0x31, 0x35, 0x7E }, KEY_F5 },
        { { 0x1B, 0x5B, 0x31, 0x37, 0x7E }, KEY_F6 },
        { { 0x1B, 0x5B, 0x31, 0x38, 0x7E }, KEY_F7 },
        { { 0x1B, 0x5B, 0x31, 0x39, 0x7E }, KEY_F8 },
        { { 0x1B, 0x5B, 0x32, 0x30, 0x7E }, KEY_F9 },
        { { 0x1B, 0x5B, 0x32, 0x31, 0x7E }, KEY_F10 },
        { { 0x1B, 0x5B, 0x32, 0x33, 0x7E }, KEY_F11 },
        { { 0x1B, 0x5B, 0x32, 0x34, 0x7E }, KEY_F12 },
        { { 0x1B, 0x5B, 0x31, 0x3B, 0x35, 0x44 }, CTRL_LEFT },
        { { 0x1B, 0x5B, 0x31, 0x3B, 0x35, 0x43 }, CTRL_RIGHT },
        { { 0x1B, 0x5B, 0x31, 0x3B, 0x35, 0x41 }, CTRL_UP },
        { { 0x1B, 0x5B, 0x31, 0x3B, 0x35, 0x42 }, CTRL_DOWN },
        { { 0 }, 0 },
    };

    return cook_input(ip, &get_raw_ch, &unget_raw_ch, ip->cooked_buf, km, ch);
}

static int get_double_cooked_ch(Input ip, int *ch)
{
    return cook_input(ip, &get_cooked_ch, &unget_cooked_ch,
        ip->double_cooked_buf, ip->second_level_km, ch);
}

int get_ch(Input ip, int *ch)
{
    switch (ip->cooking) {
    case RAW:
        return get_raw_ch(ip, ch);
    case COOKED:
        return get_cooked_ch(ip, ch);
    case DOUBLE_COOKED:
        return get_double_cooked_ch(ip, ch);
    }
    debug(return 1);
}

int unget_ch(Input ip, int ch)
{
    switch (ip->cooking) {
    case RAW:
        return unget_raw_ch(ip, ch);
    case COOKED:
        return unget_cooked_ch(ip, ch);
    case DOUBLE_COOKED:
        return unget_double_cooked_ch(ip, ch);
    }
    debug(return 1);
}
