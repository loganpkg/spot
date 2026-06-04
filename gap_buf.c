/*
 * Copyright (c) 2025, 2026 Logan Ryan McLintock. All rights reserved.
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

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "alias.h"
#include "buf.h"
#include "debug.h"
#include "gap_buf.h"
#include "input.h"
#include "int.h"
#include "memmem.h"

/* Copy type. */
#define COPY_REGION 0
#define CUT_REGION  1

/* Operation type: */
#define INSERT      1
#define DELETE      2
#define BEGIN_MULTI 4
#define END_MULTI   8

/* Mode: */
#define NORMAL 1
#define UNDO   2
#define REDO   4

/*
 * When undoing, the undo buffer is used to replay (the opposite)
 * of operations. But when redoing, the redo buffer is used to replay.
 */
#define replay_buf(gb) ((gb)->mode == UNDO ? (gb)->undo : (gb)->redo)

/*
 * Normally operations are recorded in the undo buffer,
 * including when redoing.
 * But, when undoing, the operations are recorded in the redo buffer.
 */
#define record_buf(gb) ((gb)->mode == UNDO ? (gb)->redo : (gb)->undo)

#define clear_mark(gb)                                                        \
    do {                                                                      \
        (gb)->m_set = 0;                                                      \
        (gb)->m = 0;                                                          \
    } while (0)

#define clear_sticky_column(gb)                                               \
    do {                                                                      \
        (gb)->sc_set = 0;                                                     \
        (gb)->sc = 0;                                                         \
    } while (0)

struct operation {
    /* Copy of the g location. g does not change with realloc. */
    size_t g;
    unsigned char type; /* Type of operation. */
    char ch;            /* The inserted or deleted character. */
};

/*
 * The region is the text between the mark (inclusive) and the
 * cursor (exclusive), or the cursor (inclusive) and the
 * mark (exclusive), depending on which one comes first.
 * The start is always inclusive and the end is always exclusive.
 */

struct gap_buf {
    char *fn;    /* Filename associated with the gap buffer. */
    Buf undo;    /* Undo stack. */
    Buf redo;    /* Redo stack. */
    int mode;    /* Mode: NORMAL, UNDO, REDO. */
    char *a;     /* Memory. */
    size_t g;    /* Start of gap. */
    size_t c;    /* Cursor. */
    size_t e;    /* End of gap buffer (included in memory). */
    size_t m;    /* Mark. This is a saved "g-value." */
    int m_set;   /* Indicates that the mark is set. */
    size_t row;  /* Cursor row. Starts from 1. */
    size_t col;  /* Cursor column. Starts from 0. */
    size_t sc;   /* Sticky column. Used for repeared up or down. */
    int sc_set;  /* Indicates that the sticky column is set. */
    size_t d;    /* Draw start. This can be compared with g. */
    int rc;      /* Request centring. */
    int mod;     /* Modified indicator. */
    char *sb;    /* Status bar. */
    size_t sb_s; /* Status bar allocated size. */
};

/* ######################################################################## */
/* ################ Initialise, reset and free functions  ################# */
/* ######################################################################## */

void gb_free(Gap_buf gb)
{
    if (gb != NULL) {
        free(gb->fn);
        free_buf(gb->undo);
        free_buf(gb->redo);
        free(gb->a);
        free(gb->sb);
        free(gb);
    }
}

void gb_reset(Gap_buf gb)
{
    /*
     * Resets a buffer without freeing memmory allocations.
     * The buffer name is preserved. History is lost and the buffer
     * is marked as modified.
     */
    truncate_buf(gb->undo);
    truncate_buf(gb->redo);
    gb->mode = NORMAL;
    gb->g = 0;
    gb->c = gb->e;
    clear_mark(gb);
    gb->row = 1;
    gb->col = 0;
    gb->d = 0;
    gb->rc = 0;
    gb->mod = 1;
    if (gb->sb != NULL)
        *gb->sb = '\0';
}

Gap_buf gb_init(size_t init_num_elements)
{
    Gap_buf gb = NULL;

    if (!init_num_elements)
        debug(goto error);

    if ((gb = calloc(1, sizeof(struct gap_buf))) == NULL)
        debug(goto error);

    /* Do not assume that NULL is zero. */
    gb->fn = NULL;
    gb->undo = NULL;
    gb->redo = NULL;
    gb->a = NULL;
    gb->sb = NULL;

    if ((gb->undo = init_buf(init_num_elements, sizeof(struct operation)))
        == NULL)
        debug(goto error);

    if ((gb->redo = init_buf(init_num_elements, sizeof(struct operation)))
        == NULL)
        debug(goto error);

    gb->mode = NORMAL;

    if ((gb->a = calloc(init_num_elements, sizeof(char))) == NULL)
        debug(goto error);

    gb->c = init_num_elements - 1;
    gb->e = init_num_elements - 1;
    *(gb->a + gb->e) = '~'; /* The end of buffer character. */
    gb->row = 1;

    return gb;

error:
    gb_free(gb);
    debug(return NULL);
}

/* ######################################################################## */

/* ######################################################################## */
/* ############### Fundamental operations on the gap buffer ############### */
/* ######################################################################## */

int gb_insert_ch(Gap_buf gb, char ch)
{
    /*
     * The character is written at g, then g is incremented.
     *
     * Before:
     * woXXXXld~
     * ^ ^   ^ ^
     * | |   | |
     * a g   c e
     *
     * After:
     * worXXXld~
     * ^  ^  ^ ^
     * |  |  | |
     * a  g  c e
     *
     */

    size_t s;
    char *t;
    struct operation op;

    clear_sticky_column(gb);

    if (gb->g == gb->c) {
        /* Need to grow the gap. */
        s = gb->e + 1; /* Cannot overflow, as already in memory. */
        if (mult_overflow(s, 2))
            debug(return 1);

        if ((t = realloc(gb->a, s * 2)) == NULL)
            debug(return 1);

        gb->a = t;

        /* Move down data after the gap. */
        memmove(gb->a + gb->c + s, gb->a + gb->c, gb->e - gb->c + 1);

        /* Update values that are after the gap. */
        gb->c += s;
        gb->e += s;
    }

    /* Record the operation. */
    op.g = gb->g; /* Record g before it changes. */
    op.type = INSERT;
    op.ch = ch;

    if (push(record_buf(gb), &op))
        debug(return 1);

    /* Cannot fail now. */

    /* Need to truncate the redo buffer when in normal mode. */
    if (gb->mode == NORMAL)
        truncate_buf(gb->redo);

    /* Write character to the left of the gap, making the gap smaller. */
    *(gb->a + gb->g++) = ch;

    /* Update row number and column number. */
    if (ch == '\n') {
        ++gb->row;
        gb->col = 0;
    } else {
        ++gb->col;
    }

    clear_mark(gb);

    /* Set the modified indicator. */
    gb->mod = 1;

    return 0;
}

int gb_delete_ch(Gap_buf gb)
{
    /*
     * c is incremented.
     *
     * Before:
     * elepXXXXjhant~
     * ^   ^   ^    ^
     * |   |   |    |
     * a   g   c    e
     *
     * After:
     * elepXXXXXhant~
     * ^   ^    ^   ^
     * |   |    |   |
     * a   g    c   e
     *
     * Not displaying the gap:
     * Before:
     * elepjhant~
     *     ^
     *     |
     *     c
     *
     * After:
     * elephant~
     *     ^
     *     |
     *     c
     *
     */

    struct operation op;

    clear_sticky_column(gb);

    /* Cannot delete the last character in the gap buffer. */
    if (gb->c == gb->e)
        return 1;

    op.g = gb->g;
    op.type = DELETE;
    op.ch = *(gb->a + gb->c);

    /* Record the operation. */
    if (push(record_buf(gb), &op))
        debug(return 1);

    /* Cannot fail now. */

    /* Need to truncate the redo buffer when in normal mode. */
    if (gb->mode == NORMAL)
        truncate_buf(gb->redo);

    /* Expand the gap to the right. */
    ++gb->c;

    clear_mark(gb);

    gb->mod = 1;

    return 0;
}

int gb_left_ch(Gap_buf gb)
{
    /*
     * g and c are decremented, then the character at g is copied to c.
     *
     * Before:
     * worXXXXld~
     * ^  ^   ^ ^
     * |  |   | |
     * a  g   c e
     *
     * Decrement:
     * worXXXXld~
     * ^ ^   ^  ^
     * | |   |  |
     * a g   c  e
     *
     * After:
     * woXXXXrld~
     * ^ ^   ^  ^
     * | |   |  |
     * a g   c  e
     *
     */

    size_t i;

    clear_sticky_column(gb);

    if (!gb->g)
        return 1; /* At start of gap buffer. */

    /* Cannot fail now. */

    if ((*(gb->a + --gb->c) = *(gb->a + --gb->g)) == '\n') {
        --gb->row; /* Gone up a line. */

        /* Need to recalculate the column number. */
        i = gb->g;
        while (i && *(gb->a + --i) != '\n') ++gb->col;
    } else {
        --gb->col;
    }

    return 0;
}

int gb_right_ch(Gap_buf gb)
{
    /*
     * The character at c is copied to g, then g and c are incremented.
     *
     * Before:
     * woXXXXrld~
     * ^ ^   ^  ^
     * | |   |  |
     * a g   c  e
     *
     * Copy:
     * worXXXrld~
     * ^ ^   ^  ^
     * | |   |  |
     * a g   c  e
     *
     * After:
     * worXXXXld~
     * ^  ^   ^ ^
     * |  |   | |
     * a  g   c e
     *
     */

    clear_sticky_column(gb);

    if (gb->c == gb->e)
        return 1; /* At end of the buffer. */

    /* Cannot fail now. */

    if ((*(gb->a + gb->g++) = *(gb->a + gb->c++)) == '\n') {
        ++gb->row;
        gb->col = 0;
    } else {
        ++gb->col;
    }

    return 0;
}

/* ######################################################################## */

/* ######################################################################## */
/* #################### Undo and redo related commands #################### */
/* ######################################################################## */

static int record_multi(Gap_buf gb, unsigned char type)
{
    struct operation op;

    if (type != BEGIN_MULTI && type != END_MULTI)
        debug(return 1);

    op.g = 0;
    op.type = type;
    op.ch = '\0';

    if (push(record_buf(gb), &op))
        debug(return 1);

    return 0;
}

static int undo(Gap_buf gb, int mode)
{
    struct operation op;
    size_t depth;

    if (mode != UNDO && mode != REDO)
        debug(goto error);

    gb->mode = mode;

    depth = 0;
    do {
        if (pop(replay_buf(gb), &op))
            break; /* No more. */

        if (op.type == BEGIN_MULTI || op.type == END_MULTI) {
            if (op.g || op.ch) /* These should not be used. */
                debug(goto error);
        } else {
            /* Move into position. */
            while (gb->g < op.g)
                if (gb_right_ch(gb))
                    debug(goto error); /* Should not fail. */

            while (gb->g > op.g)
                if (gb_left_ch(gb))
                    debug(goto error); /* Should not fail. */

            /* Check. */
            if (gb->g != op.g)
                debug(goto error); /* Should not fail. */
        }

        /* Perform the opposite operation. */
        switch (op.type) {
        case INSERT:
            if (gb_delete_ch(gb))
                debug(goto error);

            break;
        case DELETE:
            if (gb_insert_ch(gb, op.ch))
                debug(goto error);

            break;
        case BEGIN_MULTI:
            --depth;
            if (record_multi(gb, END_MULTI))
                debug(goto error);

            break;
        case END_MULTI:
            ++depth;
            if (record_multi(gb, BEGIN_MULTI))
                debug(goto error);

            break;
        default:
            debug(goto error); /* Invalid operation type. */
        }
    } while (depth);

    gb->mode = NORMAL;
    return 0;

error:
    gb->mode = NORMAL;
    debug(return 1);
}

int gb_undo(Gap_buf gb)
{
    return undo(gb, UNDO);
}

int gb_redo(Gap_buf gb)
{
    return undo(gb, REDO);
}

/* ######################################################################## */

/* ######################################################################## */
/* ######################### Navigation commands ########################## */
/* ######################################################################## */

void gb_start_of_line(Gap_buf gb)
{
    while (gb->g && *(gb->a + gb->g - 1) != '\n')
        if (gb_left_ch(gb))
            break;
}

void gb_end_of_line(Gap_buf gb)
{
    while (*(gb->a + gb->c) != '\n')
        if (gb_right_ch(gb))
            break;
}

void gb_start_of_buffer(Gap_buf gb)
{
    while (!gb_left_ch(gb));
}

void gb_end_of_buffer(Gap_buf gb)
{
    while (!gb_right_ch(gb));
}

int gb_up_line(Gap_buf gb)
{
    size_t row_orig, backup_sc;

    row_orig = gb->row;

    if (!gb->sc_set) {
        gb->sc = gb->col;
        gb->sc_set = 1;
    }

    backup_sc = gb->sc;

    if (row_orig == 1)
        return 1; /* On top line already. */

    while (!gb_left_ch(gb))
        if (gb->row == row_orig - 1 && gb->col <= backup_sc)
            break;

    /* Restore, as sticky column will be cleared by gb_left_ch function. */
    gb->sc = backup_sc;
    gb->sc_set = 1;

    if (gb->row == row_orig - 1)
        return 0; /* Moved up a line, so success. */

    return 1;
}

int gb_down_line(Gap_buf gb)
{
    size_t row_orig, col_orig, backup_sc;

    row_orig = gb->row;
    col_orig = gb->col; /* To go back if on the last line. */

    if (!gb->sc_set) {
        gb->sc = gb->col;
        gb->sc_set = 1;
    }

    backup_sc = gb->sc;

    if (add_overflow(row_orig, 1))
        debug(return 1);

    while (!gb_right_ch(gb))
        if (gb->row == row_orig + 1
            && (gb->col == backup_sc || *(gb->a + gb->c) == '\n'))
            break;

    if (gb->row == row_orig)
        while (gb->col != col_orig)
            if (gb_left_ch(gb))
                break;

    /* Restore, as sticky column will be cleared by left and right functions.
     */
    gb->sc = backup_sc;
    gb->sc_set = 1;

    if (gb->row == row_orig + 1)
        return 0; /* Moved down line, so success. */

    return 1;
}

int gb_forward_search(Gap_buf gb, Gap_buf search)
{
    /*
     * Forward of the cursor, exact match search (case sensitive).
     * Excludes a match at the current cursor position.
     */
    char *p;

    if (gb->c == gb->e)
        return 1; /* No match possible. */

    gb_start_of_buffer(search);
    if ((p = memmem(gb->a + gb->c + 1, gb->e - gb->c, search->a + search->c,
             search->e - search->c))
        == NULL)
        return 1; /* No match found. */

    while (gb->a + gb->c != p)
        if (gb_right_ch(gb))
            debug(return 1);

    return 0;
}

int gb_match_brace(Gap_buf gb)
{
    /* Moves to the matching brace that is under the cursor. */

    int (*move)(Gap_buf);
    int (*back)(Gap_buf);

    char brace, target, ch;
    size_t g_orig, depth;

    brace = *(gb->a + gb->c);

    switch (brace) {
    case '<':
        target = '>';
        move = &gb_right_ch;
        break;
    case '(':
        target = ')';
        move = &gb_right_ch;
        break;
    case '{':
        target = '}';
        move = &gb_right_ch;
        break;
    case '[':
        target = ']';
        move = &gb_right_ch;
        break;

    case '>':
        target = '<';
        move = &gb_left_ch;
        break;
    case ')':
        target = '(';
        move = &gb_left_ch;
        break;
    case '}':
        target = '{';
        move = &gb_left_ch;
        break;
    case ']':
        target = '[';
        move = &gb_left_ch;
        break;

    default:
        return 1; /* Not a brace style character. */
    }

    /* Backup starting position in case there is no match. */
    g_orig = gb->g;

    depth = 1; /* As already on a brace. */

    while (!move(gb)) {
        ch = *(gb->a + gb->c);
        if (ch == brace)
            ++depth;
        else if (ch == target)
            --depth;

        if (!depth)
            return 0; /* Success. */
    }

    /* Not found. Go back to starting location. */
    if (move == &gb_right_ch)
        back = &gb_left_ch;
    else
        back = &gb_right_ch;

    while (gb->g != g_orig) back(gb);

    return 1;
}

/* ######################################################################## */

/* ######################################################################## */
/* ####################### Miscellaneous commands  ######################## */
/* ######################################################################## */

int gb_backspace_ch(Gap_buf gb)
{
    if (gb_left_ch(gb))
        return 1;

    return gb_delete_ch(gb);
}

int gb_insert_hex_str(Gap_buf gb, const char *hex_str)
{
    /*
     * The 1 and 0 refers to the powers of 16, the hexadecimal places
     * of the digits.
     */
    const unsigned char *p;
    unsigned char h_1, h_0, num_1, num_0, x;

    if (record_multi(gb, BEGIN_MULTI))
        debug(return 1);

    p = (unsigned char *) hex_str;

    while ((h_1 = *p++)) {
        /* Hex digit must be in pairs of two in order to represent bytes. */
        if (!(h_0 = *p++))
            goto error;

        if (hex_digit_to_num(&num_1, h_1))
            goto error;

        if (hex_digit_to_num(&num_0, h_0))
            goto error;

        x = num_1 * 16 + num_0;

        if (gb_insert_ch(gb, x))
            goto error;
    }

    if (record_multi(gb, END_MULTI))
        debug(return 1);

    return 0;

error:
    if (record_multi(gb, END_MULTI))
        debug(return 1);

    /* Attempt undo. */
    gb_undo(gb);
    return 1;
}

int gb_trim_clean(Gap_buf gb)
{
    size_t g_orig;
    int at_end_of_buffer, delete_nl, at_end_of_line;
    char ch;

    /* Record starting location. */
    g_orig = gb->g;

    gb_end_of_buffer(gb);

    if (record_multi(gb, BEGIN_MULTI))
        debug(return 1);

    at_end_of_buffer = 1;
    delete_nl = 0;
    at_end_of_line = 0;

    while (!gb_left_ch(gb)) {
        ch = *(gb->a + gb->c);

        if (isgraph(ch)) {
            /* These characters are never deleted. */
            at_end_of_buffer = 0;
            at_end_of_line = 0;
        } else if (ch == '\n') {
            if (at_end_of_buffer) {
                /* Trim excess new line characters from the end of the buffer.
                 */
                if (delete_nl) {
                    if (gb_delete_ch(gb))
                        debug(goto error);
                } else {
                    /*
                     * Do not delete the first encountered new line
                     * but delete all trailing ones after that.
                     */
                    delete_nl = 1;
                }
            }
            at_end_of_line = 1;
        } else if (ch == ' ' || ch == '\t') {
            /* Trim trailing whitespace from the end of lines. */
            if (at_end_of_line && gb_delete_ch(gb))
                debug(goto error);
        } else {
            /* Clean: Delete all other characters. */
            if (gb_delete_ch(gb))
                debug(goto error);
        }
    }

    if (record_multi(gb, END_MULTI))
        debug(return 1);

    /*
     * Try to travel back to the same starting position,
     * although it may not be possible due to the deletions.
     */
    while (!gb_right_ch(gb))
        if (gb->g == g_orig)
            break;

    return 0;

error:
    if (record_multi(gb, END_MULTI))
        debug(return 1);

    /* Attempt undo. */
    gb_undo(gb);
    debug(return 1);
}

/* ######################################################################## */

/* ######################################################################## */
/* ######################## File related commands  ######################## */
/* ######################################################################## */

int gb_insert_file(Gap_buf gb, const char *fn)
{
    Input ip = NULL;
    int r, ch, bm = 0, em = 0;
    size_t count;

    if ((r = init_input_fn(&ip, fn, BLOCKING, RAW, NULL)))
        return r;

    if (record_multi(gb, BEGIN_MULTI))
        debug(goto error);

    bm = 1;
    count = 0;

    while (1) {
        if (get_ch(ip, &ch))
            debug(goto error);

        if (ch == EOF)
            break;

        if (gb_insert_ch(gb, (char) ch))
            debug(goto error);

        ++count;
    }

    if (record_multi(gb, END_MULTI))
        debug(goto error);

    em = 1;

    if (free_input(ip)) {
        ip = NULL; /* Do not try to free again. */
        debug(goto error);
    }

    ip = NULL;

    /* Move to the start of the inserted file. */
    while (count--)
        if (gb_left_ch(gb))
            debug(goto error);

    return 0;

error:
    if (ip != NULL)
        free_input(ip);

    if (bm && !em)
        if (!record_multi(gb, END_MULTI))
            em = 1;

    /* Attempt undo. */
    if (em)
        gb_undo(gb);

    debug(return -1);
}

const char *gb_to_str(Gap_buf gb)
{
    /* Only safe to use while the gap buffer is not changed. */

    gb_start_of_buffer(gb);

    do {
        if (*(gb->a + gb->c) == '\0')
            if (gb_delete_ch(gb))
                debug(return NULL);
    } while (!gb_right_ch(gb));

    if (gb_insert_ch(gb, '\0'))
        debug(return NULL);

    return gb->a;
}

int gb_set_fn(Gap_buf gb, const char *fn)
{
    size_t len;
    char *t;

    if (fn == NULL) {
        if (gb->fn != NULL)
            free(gb->fn);

        gb->fn = NULL;
        return 0;
    }

    len = strlen(fn);

    /* Cannot overflow as already in memory. */
    if ((t = calloc(len + 1, sizeof(char))) == NULL)
        debug(return 1);

    memmove(t, fn, len + 1);
    gb->fn = t;

    /* Set mod so that the buffer can be re-saved as a different filename. */
    gb->mod = 1;

    return 0;
}

const char *gb_get_fn(Gap_buf gb)
{
    return gb->fn;
}

void gb_clear_mod(Gap_buf gb)
{
    gb->mod = 0;
}

int gb_is_mod_set(Gap_buf gb)
{
    return gb->mod;
}

int gb_write_file(Gap_buf gb, const char *fn)
{
    FILE *fp = NULL;

    if (fn == NULL || *fn == '\0')
        goto error;

#ifdef _WIN32
    if (fopen_s(&fp, fn, "wb"))
#else
    if ((fp = fopen(fn, "wb")) == NULL)
#endif
        debug(goto error);

    /* Write before the gap. */
    if (fwrite(gb->a, 1, gb->g, fp) != gb->g)
        debug(goto error);

    /* Write after the gap. */
    if (fwrite(gb->a + gb->c, 1, gb->e - gb->c, fp) != gb->e - gb->c)
        debug(goto error);

    if (fclose(fp))
        return 1;

    if (gb->fn != NULL && *gb->fn != '\0' && !strcmp(gb->fn, fn))
        gb->mod = 0;

    return 0;

error:
    if (fp != NULL)
        fclose(fp);

    return 1;
}

/* ######################################################################## */

/* ######################################################################## */
/* ################ Copy, cut and paste related commands  ################# */
/* ######################################################################## */

void gb_set_mark(Gap_buf gb)
{
    gb->m = gb->g;
    gb->m_set = 1;
}

void gb_clear_mark(Gap_buf gb)
{
    clear_mark(gb);
}

int gb_is_mark_set(Gap_buf gb)
{
    return gb->m_set;
}

static int gb_copy_or_cut_region(Gap_buf gb, Gap_buf paste, int type)
{
    size_t i_start; /* Index of the start of the region (inclusive). */
    size_t i_end;   /* Index of the end of the region (exclusive). */
    size_t region_size;
    size_t i;

    if (!gb->m_set)
        return 1;

    if (gb->m == gb->g)
        return 0; /* Nothing to do, empty region. */

    gb_reset(paste);

    if (gb->m < gb->g) {
        /* Region: mark (inclusive) -> cursor (exclusive). */
        i_start = gb->m;
        i_end = gb->g;
    } else {
        /* Region: cursor (inclusive) -> mark (exclusive). */
        i_start = gb->c;
        i_end = gb->c + gb->m - gb->g;
    }

    region_size = i_end - i_start;

    for (i = i_start; i < i_end; ++i)
        if (gb_insert_ch(paste, *(gb->a + i)))
            debug(return 1);

    if (type == CUT_REGION) {
        if (record_multi(gb, BEGIN_MULTI))
            debug(return 1);

        if (gb->m < gb->g) {
            for (i = 0; i < region_size; ++i)
                if (gb_backspace_ch(gb))
                    debug(goto error);
        } else {
            for (i = 0; i < region_size; ++i)
                if (gb_delete_ch(gb))
                    debug(goto error);
        }

        if (record_multi(gb, END_MULTI))
            debug(return 1);
    } else {
        clear_mark(gb);
    }

    return 0;

error:
    if (record_multi(gb, END_MULTI))
        debug(return 1);

    /* Attempt undo. */
    gb_undo(gb);
    debug(return 1);
}

int gb_copy_region(Gap_buf gb, Gap_buf paste)
{
    return gb_copy_or_cut_region(gb, paste, COPY_REGION);
}

int gb_cut_region(Gap_buf gb, Gap_buf paste)
{
    return gb_copy_or_cut_region(gb, paste, CUT_REGION);
}

int gb_cut_to_start_of_line(Gap_buf gb, Gap_buf paste)
{
    gb_set_mark(gb);
    gb_start_of_line(gb);
    return gb_cut_region(gb, paste);
}

int gb_cut_to_end_of_line(Gap_buf gb, Gap_buf paste)
{
    /* Special case. If on a newline character then just delete it. */
    if (*(gb->a + gb->c) == '\n')
        return gb_delete_ch(gb);

    gb_set_mark(gb);
    gb_end_of_line(gb);
    return gb_cut_region(gb, paste);
}

int gb_insert_gb(Gap_buf target, Gap_buf source)
{
    /* Inserts source into target. */
    size_t i;

    if (record_multi(target, BEGIN_MULTI))
        debug(return 1);

    /* Before gap. */
    for (i = 0; i < source->g; ++i)
        if (gb_insert_ch(target, *(source->a + i)))
            debug(goto error);

    /* After gap. */
    for (i = source->c; i < source->e; ++i)
        if (gb_insert_ch(target, *(source->a + i)))
            debug(goto error);

    if (record_multi(target, END_MULTI))
        debug(return 1);

    return 0;

error:
    if (record_multi(target, END_MULTI))
        debug(return 1);

    /* Attempt undo. */
    gb_undo(target);
    debug(return 1);
}

/* ######################################################################## */

/* ######################################################################## */
/* ###################### Graphics related commands ####################### */
/* ######################################################################## */

void gb_debug_print(Gap_buf gb)
{
    /* Useful for debugging. */
    size_t i;

    /* Before gap. */
    for (i = 0; i < gb->g; ++i) putchar(*(gb->a + i));

    /* In gap. */
    for (i = gb->g; i < gb->c; ++i) putchar('X');

    /* After gap. */
    for (i = gb->c; i <= gb->e; ++i) putchar(*(gb->a + i));

    putchar('\n');
}

void gb_request_centring(Gap_buf gb)
{
    gb->rc = 1;
}

#define check()                                                               \
    do {                                                                      \
        ++x;                                                                  \
        if (ch == '\n' || x == sub_w) {                                       \
            ++y;                                                              \
            x = 0;                                                            \
        }                                                                     \
        if (!d_centre_set && y == sub_h / 2 + 1) {                            \
            /*                                                                \
             * Save d_centre for later.                                       \
             * Plus 1 to move back in bounds.                                 \
             * Prevents mult-position chars from being half-off.              \
             */                                                               \
            d_centre = i + 1;                                                 \
            d_centre_set = 1;                                                 \
            if (gb->rc) /* Stop early. */                                     \
                goto end;                                                     \
        }                                                                     \
        if (y == sub_h) {                                                     \
            /* Off sub-screen. */                                             \
            ++i;                                                              \
            goto end;                                                         \
        }                                                                     \
    } while (0)

static int gb_centre(Gap_buf gb, size_t sub_h, size_t sub_w)
{
    /*
     * This function simulates printing backwards to see if the cursor would
     * be printed within the bounds of an abstract sub_h by sub_w screen size.
     * Draw start, d, will be changed if the cursor would be off the screen,
     * or if centring was requested (rc was set).
     */
    size_t y, x, i, j;
    char ch;
    int d_centre_set = 0;
    size_t d_centre = 0;

    if (!sub_h || !sub_w)
        debug(return 1);

    if (mult_overflow(sub_h, sub_w))
        debug(return 1);

    if (!gb->g) {
        /* At the start of the buffer. */
        gb->d = 0;
        gb->rc = 0;
        return 0;
    }

    /*
     * Obvious off screen checks.
     * If g is less than d, then the cursor is off the screen above.
     * Every character takes at least one screen position to be printed,
     * so the area check is a good short-circuit for when the cursor is
     * far off the screen below.
     */
    if (gb->g < gb->d || gb->g - gb->d > sub_h * sub_w)
        gb->rc = 1;

    i = gb->g - 1;
    y = 0;
    x = 0;
    while (1) {
        ch = *(gb->a + i);
        if (ch == '\n') {
            check();
        } else if (ch == '\t') {
            /*
             * A long character, like a tab, can wrap multiple times when
             * the screen is narrow. So need to check after each char.
             */
            for (j = 0; j < TAB_SIZE; ++j) check();
        } else if (iscntrl(ch)) {
            for (j = 0; j < CTRL_CH_SIZE; ++j) check();
        } else {
            check();
        }

        if (!i)
            break;

        --i;
    }

end:

    if (gb->rc || gb->d < i) {
        /* Would be off screen, so centre. */
        if (d_centre_set)
            gb->d = d_centre;
        else
            gb->d = i; /* Not enough text to reach the centre. */

        gb->rc = 0;
        return 0;
    }

    /* Current draw start is OK. */
    return 0;
}

#undef check

int gb_print(Gap_buf gb, Screen sc, size_t y_origin, size_t x_origin,
    size_t sub_h, size_t sub_w, int sb_option, size_t *cursor_y,
    size_t *cursor_x)
{
    char *t;
    size_t h, w, text_h, i, y, x;

    if (sb_option != INCLUDE_STATUS_BAR && sb_option != EXCLUDE_STATUS_BAR)
        debug(return 1);

    if (!sub_h || !sub_w)
        debug(return 1);

    if (sb_option == INCLUDE_STATUS_BAR && sub_h < 2)
        debug(return 1);

    h = get_screen_height(sc);
    w = get_screen_width(sc);

    if (add_overflow(y_origin, sub_h) || add_overflow(x_origin, sub_w))
        debug(return 1);

    if (y_origin + sub_h > h || x_origin + sub_w > w)
        debug(return 1);

    if (sb_option == INCLUDE_STATUS_BAR)
        text_h = sub_h - 1;
    else
        text_h = sub_h;

    /* Minus one for the status bar. */
    if (gb_centre(gb, text_h, sub_w))
        debug(return 1);

    /* Place cursor at the left-hand side of the starting row. */
    if (move(sc, y_origin, x_origin))
        debug(return 1);

    /* Print before the gap. */
    i = gb->d;
    if (gb->m_set && gb->m < gb->g) {
        /* Before the region. */
        for (; i < gb->m; ++i)
            if (sub_screen_print_ch(
                    sc, y_origin, x_origin, text_h, sub_w, *(gb->a + i)))
                debug(return 1);

        highlight_on(sc);
    }

    for (; i < gb->g; ++i)
        if (sub_screen_print_ch(
                sc, y_origin, x_origin, text_h, sub_w, *(gb->a + i)))
            debug(return 1);

    if (gb->m_set && gb->m < gb->g)
        highlight_off(sc);

    /* Record location on the screen where the cursor should be. */
    y = get_y(sc);
    x = get_x(sc);

    /* Cursor out of designated sub-screen text area. */
    if (y >= y_origin + text_h || x >= x_origin + sub_w)
        debug(return 1);

    /* Print after the gap. */

    i = gb->c;
    /*
     * Print the cursor, always without highlighting.
     * Failure is OK, as only the first char of a multi-byte character,
     * or a newline char which is printed as a sequence of space chars
     * until the edge of the screen, needs to be on the screen.
     */
    sub_screen_print_ch(sc, y_origin, x_origin, text_h, sub_w, *(gb->a + i));

    ++i;

    /* Region. */
    if (gb->m_set && gb->m > gb->g) {
        highlight_on(sc);
        /*
         * m must be compared to g.
         * Failure is OK, as cursor has been printed.
         */
        for (; i < gb->c + (gb->m - gb->g); ++i)
            sub_screen_print_ch(
                sc, y_origin, x_origin, text_h, sub_w, *(gb->a + i));

        highlight_off(sc);
    }

    /* Failure is OK. */
    for (; i <= gb->e; ++i)
        sub_screen_print_ch(
            sc, y_origin, x_origin, text_h, sub_w, *(gb->a + i));

    if (add_overflow(sub_w, 1))
        debug(return 1);

    if (gb->sb_s < sub_w + 1) {
        /* Grow status bar memory allocation. */
        if ((t = realloc(gb->sb, sub_w + 1)) == NULL)
            debug(return 1);

        gb->sb = t;
        gb->sb_s = sub_w + 1;
    }

    if (sb_option == INCLUDE_STATUS_BAR) {
        /* Prepare status bar. */
        snprintf(gb->sb, gb->sb_s, "%c %s (%" lu ", %" lu ") %02X\n",
            gb->mod ? '*' : ' ', gb->fn == NULL ? "NULL" : gb->fn, gb->row,
            gb->col, *(gb->a + gb->c));

        if (move(sc, y_origin + text_h, x_origin))
            debug(return 1);

        highlight_on(sc);
        /*
         * Do not check for error, as it is OK to truncate the display
         * of the status bar.
         */
        sub_screen_print_str(
            sc, y_origin + sub_h - 1, x_origin, 1, sub_w, gb->sb);
        highlight_off(sc);
    }

    *cursor_y = y;
    *cursor_x = x;

    return 0;
}

/* ######################################################################## */
