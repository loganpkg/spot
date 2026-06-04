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

#include <ctype.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "asprintf.h"
#include "buf.h"
#include "debug.h"
#include "doubly_linked_list.c"
#include "gap_buf.h"
#include "get_time.h"
#include "input.h"
#include "random.h"
#include "screen.h"

/* ######### Configuration: ######### */
#define INIT_NUM_GB_ELEMENTS 512
/* ################################## */

#ifdef _WIN32
#define lx "llx"
#else
#define lx "lx"
#endif

#ifdef _WIN32
#define DIR_SEP_STR "\\"
#else
#define DIR_SEP_STR "/"
#endif

#define sringify(t)            #t
#define expand_and_sringify(t) sringify(t)

/* The direction when changing buffers. */
#define LEFT_GB  0
#define RIGHT_GB 1

/* This moves the command identifiers beyond the normal cooked key range. */
#define CMD_ID_OFFSET 0x200

/* Increments each time it is used. */
#define CMD_COUNTER __COUNTER__

/* Command Identifier. */
#define ID (CMD_ID_OFFSET + CMD_COUNTER)

/* Split types: */
#define NO_SPLIT         1
#define VERTICAL_SPLIT   2
#define HORIZONTAL_SPLIT 4

/* Operation. 0 means no operation. */
#define ED_RENAME         1
#define ED_OPEN_FILE      2
#define ED_INSERT_FILE    3
#define ED_FORWARD_SEARCH 4
#define ED_INSERT_HEX     5

/* Current gap buffer, excluding the cl. */
#define c_gb (ed->view_2 ? ed->n_2->data : ed->n->data)

/* Active gap buffer, including the cl. */
#define a_gb (ed->cl_a ? ed->cl : c_gb)

/*
 * full_clear:
 * 0 is the default, which does a soft clear of the sub screen,
 *     not the full screen.
 * SOFT_CLEAR is set when split or active buffer changes.
 * HARD_CLEAR is used when requested.
 */
struct editor {
    /* n and n_2 are nodes of the same doubly linked list of gap buffers. */
    Dlln n;    /* Left or top view. */
    Dlln n_2;  /* Right or bottom view. Do not free. */
    int split; /* Split type. */
    int full_clear;
    int view_2;     /* n_2 is in use (cl_a could be set too). */
    Gap_buf cl;     /* Command line gap buffer. */
    int cl_a;       /* Command line is active. */
    Gap_buf search; /* Search gap buffer. */
    Gap_buf paste;  /* Paste gap buffer. */
    int operation;  /* The operation that is using the cl. */
    Input ip;
    int ch; /* Read character. */
    Screen sc;
    int rv;      /* Return value of the last command. */
    int running; /* Text editor is on. */
};

typedef struct editor *Editor;
typedef void (*Ed_func)(Editor);

static int free_dll_node_data(void *data)
{
    gb_free(data);
    return 0;
}

static int free_editor(Editor ed)
{
    int r = 0;

    if (ed != NULL) {
        if (free_dll(&ed->n, &free_dll_node_data))
            debug(r = 1);

        gb_free(ed->cl);
        gb_free(ed->search);
        gb_free(ed->paste);

        if (free_input(ed->ip))
            debug(r = 1);

        if (free_screen(ed->sc))
            debug(r = 1);

        free(ed);
    }
    return r;
}

static Editor init_editor(const struct key_map *km)
{
    Editor ed = NULL;

    if ((ed = calloc(1, sizeof(struct editor))) == NULL)
        debug(goto error);

    ed->n = NULL;
    ed->n_2 = NULL;
    ed->split = NO_SPLIT;
    ed->full_clear = SOFT_CLEAR;
    ed->cl = NULL;
    ed->search = NULL;
    ed->paste = NULL;
    ed->ip = NULL;
    ed->sc = NULL;

    if ((ed->cl = gb_init(INIT_NUM_GB_ELEMENTS)) == NULL)
        debug(goto error);

    if ((ed->search = gb_init(INIT_NUM_GB_ELEMENTS)) == NULL)
        debug(goto error);

    if ((ed->paste = gb_init(INIT_NUM_GB_ELEMENTS)) == NULL)
        debug(goto error);

    if (init_input_stdin(&ed->ip, BLOCKING, DOUBLE_COOKED, km))
        debug(goto error);

    if ((ed->sc = init_screen()) == NULL)
        debug(goto error);

    ed->running = 1;

    return ed;

error:
    free_editor(ed);
    debug(return NULL);
}

static char *get_backup_fn(Gap_buf gb)
{
    const char *fn;
    char *nm, *p, ch;
    char *fn_copy = NULL;
    char *tm = NULL;
    char *backup_fn = NULL;
    uint64_t x;

    fn = gb_get_fn(gb);

    if (fn == NULL || *fn == '\0') {
        nm = "NO_NAME";
    } else {
        if (asprintf(&fn_copy, "%s", fn) < 0)
            debug(return NULL);

        p = fn_copy;
        while ((ch = *p)) {
            if (ch == '/' || ch == '\\')
                *p = '_';

            ++p;
        }

        nm = fn_copy;
    }

    if (memset_random(&x, sizeof(uint64_t))) {
        free(fn_copy);
        debug(return NULL);
    }

    if ((tm = get_time_str()) == NULL) {
        free(fn_copy);
        debug(return NULL);
    }

    if (asprintf(&backup_fn, "%s%s%s_%016" lx "_%s",
            expand_and_sringify(SUCO_BACKUP_DIR), DIR_SEP_STR, tm, x, nm)
        < 0) {
        free(fn_copy);
        free(tm);
        debug(return NULL);
    }

    free(fn_copy);
    free(tm);

    return backup_fn;
}

static int add_gap_buf(Editor ed, const char *fn)
{
    Gap_buf gb = NULL;
    char *backup_fn = NULL;
    int r;

    if ((gb = gb_init(INIT_NUM_GB_ELEMENTS)) == NULL)
        debug(goto error);

    if (fn != NULL) {
        r = gb_insert_file(gb, fn);
        if (r == -1)
            debug(goto error);

        if (gb_set_fn(gb, fn))
            debug(goto error);

        if (!r) {
            /* Do not backup non-existent files. */
            if ((backup_fn = get_backup_fn(gb)) == NULL)
                debug(goto error);

            if (gb_write_file(gb, backup_fn))
                debug(goto error);
        }

        gb_clear_mod(gb);
    }

    if (dll_add_node(&ed->n, gb))
        debug(goto error);

    return 0;

error:
    gb_free(gb);

    if (backup_fn != NULL)
        free(backup_fn);

    debug(return 1);
}

static int draw_screen(Editor ed)
{
    size_t h, w, y = 0, x = 0, y_2 = 0, x_2 = 0, cl_y = 0, cl_x = 0;

    if (ed->full_clear)
        if (clear_screen(ed->sc, ed->full_clear))
            debug(return 1);

    h = get_screen_height(ed->sc);
    w = get_screen_width(ed->sc);

    /* Check the minimum screen height. */
    if (ed->split == HORIZONTAL_SPLIT) {
        if (h < 5)
            debug(return 1);
    } else {
        if (h < 3)
            debug(return 1);
    }

    /* Check the minimum screen width. */
    if (w < 1)
        debug(return 1);

    switch (ed->split) {
    case NO_SPLIT:
        if (ed->full_clear || !ed->cl_a) {
            if (!ed->full_clear)
                if (soft_clear_sub_screen(ed->sc, 0, 0, h - 1, w))
                    debug(return 1);

            if (gb_print(ed->n->data, ed->sc, 0, 0, h - 1, w,
                    INCLUDE_STATUS_BAR, &y, &x))
                debug(return 1);
        }

        break;
    case VERTICAL_SPLIT:
        if (ed->full_clear || (!ed->cl_a && !ed->view_2)) {
            if (!ed->full_clear)
                if (soft_clear_sub_screen(ed->sc, 0, 0, h - 1, w / 2))
                    debug(return 1);

            if (gb_print(ed->n->data, ed->sc, 0, 0, h - 1, w / 2,
                    INCLUDE_STATUS_BAR, &y, &x))
                debug(return 1);
        }

        if (ed->full_clear || (!ed->cl_a && ed->view_2)) {
            if (!ed->full_clear)
                if (soft_clear_sub_screen(ed->sc, 0, w / 2, h - 1, w / 2))
                    debug(return 1);

            if (gb_print(ed->n_2->data, ed->sc, 0, w / 2, h - 1, w / 2,
                    INCLUDE_STATUS_BAR, &y_2, &x_2))
                debug(return 1);
        }

        break;
    case HORIZONTAL_SPLIT:
        if (ed->full_clear || (!ed->cl_a && !ed->view_2)) {
            if (!ed->full_clear)
                if (soft_clear_sub_screen(ed->sc, 0, 0, h / 2, w))
                    debug(return 1);

            if (gb_print(ed->n->data, ed->sc, 0, 0, h / 2, w,
                    INCLUDE_STATUS_BAR, &y, &x))
                debug(return 1);
        }

        if (ed->full_clear || (!ed->cl_a && ed->view_2)) {
            if (!ed->full_clear)
                if (soft_clear_sub_screen(
                        ed->sc, h / 2, 0, h % 2 ? h / 2 : h / 2 - 1, w))
                    debug(return 1);

            if (gb_print(ed->n_2->data, ed->sc, h / 2, 0,
                    h % 2 ? h / 2 : h / 2 - 1, w, INCLUDE_STATUS_BAR, &y_2,
                    &x_2))
                debug(return 1);
        }

        break;
    default:
        debug(return 1); /* Invalid split type. */
    }

    if (ed->full_clear || ed->cl_a) {
        if (!ed->full_clear)
            if (soft_clear_sub_screen(ed->sc, h - 1, 1, 1, w - 1))
                debug(return 1);

        if (gb_print(ed->cl, ed->sc, h - 1, 1, 1, w - 1, EXCLUDE_STATUS_BAR,
                &cl_y, &cl_x))
            debug(return 1);
    }

    /* Display global last return value (shared across all buffers). */
    if (move(ed->sc, h - 1, 0))
        debug(return 1);

    highlight_on(ed->sc);
    if (print_ch(ed->sc, ed->rv ? '!' : ' '))
        debug(return 1);

    highlight_off(ed->sc);

    /* Position cursor on the display. */
    if (move(ed->sc, ed->cl_a ? cl_y : (ed->view_2 ? y_2 : y),
            ed->cl_a ? cl_x : (ed->view_2 ? x_2 : x)))
        debug(return 1);

    if (refresh_screen(ed->sc))
        debug(return 1);

    ed->full_clear = 0;

    return 0;
}

static void ed_delete_ch(Editor ed)
{
    ed->rv = gb_delete_ch(a_gb);
}

static void ed_backspace_ch(Editor ed)
{
    ed->rv = gb_backspace_ch(a_gb);
}

static void ed_left_ch(Editor ed)
{
    ed->rv = gb_left_ch(a_gb);
}

static void ed_right_ch(Editor ed)
{
    ed->rv = gb_right_ch(a_gb);
}

static void ed_undo(Editor ed)
{
    ed->rv = gb_undo(a_gb);
}

static void ed_redo(Editor ed)
{
    ed->rv = gb_redo(a_gb);
}

static void ed_start_of_line(Editor ed)
{
    gb_start_of_line(a_gb);
}

static void ed_end_of_line(Editor ed)
{
    gb_end_of_line(a_gb);
}

static void ed_start_of_buffer(Editor ed)
{
    gb_start_of_buffer(a_gb);
}

static void ed_end_of_buffer(Editor ed)
{
    gb_end_of_buffer(a_gb);
}

static void ed_up_line(Editor ed)
{
    ed->rv = gb_up_line(a_gb);
}

static void ed_down_line(Editor ed)
{
    ed->rv = gb_down_line(a_gb);
}

static void ed_match_brace(Editor ed)
{
    ed->rv = gb_match_brace(a_gb);
}

static void ed_trim_clean(Editor ed)
{
    ed->rv = gb_trim_clean(a_gb);
}

static void ed_set_mark(Editor ed)
{
    gb_set_mark(a_gb);
}

static void ed_copy_region(Editor ed)
{
    ed->rv = gb_copy_region(a_gb, ed->paste);
}

static void ed_cut_region(Editor ed)
{
    ed->rv = gb_cut_region(a_gb, ed->paste);
}

static void ed_cut_to_start_of_line(Editor ed)
{
    ed->rv = gb_cut_to_start_of_line(a_gb, ed->paste);
}

static void ed_cut_to_end_of_line(Editor ed)
{
    ed->rv = gb_cut_to_end_of_line(a_gb, ed->paste);
}

static void ed_paste(Editor ed)
{
    ed->rv = gb_insert_gb(a_gb, ed->paste);
}

static void ed_centre(Editor ed)
{
    gb_request_centring(a_gb);
    ed->full_clear = HARD_CLEAR; /* Complete redraw. */
}

static void ed_save(Editor ed)
{
    char *backup_fn = NULL;

    if (!gb_is_mod_set(a_gb))
        return; /* No changes to save. */

    if (gb_write_file(a_gb, gb_get_fn(a_gb))) {
        ed->rv = 1;
        return;
    }

    if ((backup_fn = get_backup_fn(a_gb)) == NULL) {
        ed->rv = 1;
        debug(return);
    }

    if (gb_write_file(a_gb, backup_fn)) {
        free(backup_fn);
        ed->rv = 1;
        return;
    }

    free(backup_fn);
}

static void ed_repeat_last_search(Editor ed)
{
    ed->rv = gb_forward_search(c_gb, ed->search);
}

/* ######################################################################## */
/* #################### Command line related functions #################### */
/* ######################################################################## */

static int prepare_cl(Editor ed, int operation)
{
    /* Can only have one command line operation at a time. */
    if (ed->operation)
        return 1;

    gb_reset(ed->cl);
    ed->cl_a = 1;
    ed->operation = operation;
    return 0;
}

static void ed_rename(Editor ed)
{
    ed->rv = prepare_cl(ed, ED_RENAME);
}

static void ed_open_file(Editor ed)
{
    ed->rv = prepare_cl(ed, ED_OPEN_FILE);
}

static void ed_insert_file(Editor ed)
{
    /* Inserts a file at the cursor, not in a new buffer. */
    ed->rv = prepare_cl(ed, ED_INSERT_FILE);
}

static void ed_forward_search(Editor ed)
{
    ed->rv = prepare_cl(ed, ED_FORWARD_SEARCH);
}

static void ed_insert_hex(Editor ed)
{
    ed->rv = prepare_cl(ed, ED_INSERT_HEX);
}

static void process_cl_operation(Editor ed)
{
    const char *cl_str = NULL;

    ed->rv = 1; /* Default is failure. */

    if (ed->operation != ED_FORWARD_SEARCH)
        if ((cl_str = gb_to_str(ed->cl)) == NULL)
            debug(goto end);

    switch (ed->operation) {
    case ED_RENAME:
        ed->rv = gb_set_fn(c_gb, cl_str);
        break;
    case ED_OPEN_FILE:
        ed->rv = add_gap_buf(ed, cl_str);
        break;
    case ED_INSERT_FILE:
        ed->rv = gb_insert_file(c_gb, cl_str);
        break;
    case ED_FORWARD_SEARCH:
        gb_reset(ed->search);
        if (gb_insert_gb(ed->search, ed->cl))
            debug(break);

        ed->rv = gb_forward_search(c_gb, ed->search);
        break;
    case ED_INSERT_HEX:
        ed->rv = gb_insert_hex_str(c_gb, cl_str);
        break;
    default:
        debug(break); /* Invalid operation. */
    }

end:
    ed->operation = 0;
    ed->cl_a = 0;
}

static void ed_clear_mark_or_esc_cmd(Editor ed)
{
    if (gb_is_mark_set(a_gb)) {
        gb_clear_mark(a_gb);
        return;
    }

    ed->operation = 0;
    ed->cl_a = 0;
}

/* ######################################################################## */

static void ed_toggle_split(Editor ed)
{
    if (ed->split == NO_SPLIT) {
        if (ed->n->next != NULL) {
            ed->n_2 = ed->n->next;
        } else if (ed->n->prev != NULL) {
            ed->n_2 = ed->n->prev;
        } else {
            /*
             * New nodes are linked in on the left,
             * and n is updated automatically,
             * so n_2 will be the next.
             */
            if (add_gap_buf(ed, NULL))
                ed->rv = 1;

            ed->n_2 = ed->n;
            ed->n = ed->n->next;
        }
        ed->split = VERTICAL_SPLIT;
    } else if (ed->split == VERTICAL_SPLIT) {
        ed->split = HORIZONTAL_SPLIT;
    } else {
        if (ed->view_2)
            ed->n = ed->n_2;

        ed->n_2 = NULL;
        ed->view_2 = 0;
        ed->split = NO_SPLIT;
    }

    ed->full_clear = SOFT_CLEAR;
}

static void ed_toggle_view(Editor ed)
{
    if (ed->n_2 == NULL)
        return;
    if (ed->view_2)
        ed->view_2 = 0;
    else
        ed->view_2 = 1;
}

static int change_gb(Editor ed, int direction)
{
    Dlln t, skip;

    if (ed->view_2) {
        t = ed->n_2;
        skip = ed->n;
    } else {
        t = ed->n;
        skip = ed->n_2;
    }

    while ((t = direction == LEFT_GB ? t->prev : t->next) != NULL)
        if (t != skip) {
            if (ed->view_2)
                ed->n_2 = t;
            else
                ed->n = t;

            return 0;
        }

    return 1;
}

static void ed_close(Editor ed)
{
    char *backup_fn = NULL;

    ed->n_2 = NULL;
    ed->view_2 = 0;
    ed->split = NO_SPLIT;

    /* Move to the first buffer. */
    while (!change_gb(ed, LEFT_GB));

    do {
        /* Backup modified buffers. */
        if (gb_is_mod_set(c_gb)) {
            if ((backup_fn = get_backup_fn(c_gb)) == NULL) {
                ed->rv = 1;
            } else {
                if (gb_write_file(c_gb, backup_fn))
                    ed->rv = 1;

                if (backup_fn != NULL)
                    free(backup_fn);

                backup_fn = NULL;
            }
        }
        /* Move forward, buffer by buffer. */
    } while (!change_gb(ed, RIGHT_GB));

    ed->running = 0;
}

static void ed_left_gb(Editor ed)
{
    ed->rv = change_gb(ed, LEFT_GB);
}

static void ed_right_gb(Editor ed)
{
    ed->rv = change_gb(ed, RIGHT_GB);
}

int main(int argc, char **argv)
{
    Editor ed = NULL;
    int i;

    const struct key_map km[] = {
#include ".key_sequence_records.txt"
        { { 0 }, 0 },
    };

    const Ed_func edf[] = {
#include ".key_func_pointer_records.txt"
    };

    if ((ed = init_editor(km)) == NULL)
        debug(goto error);

    if (argc > 1) {
        /* Do backwards, as nodes are linked in on the left. */
        for (i = argc - 1; i >= 1; --i)
            if (add_gap_buf(ed, *(argv + i)))
                debug(goto error);
    } else {
        if (add_gap_buf(ed, NULL))
            debug(goto error);
    }

    while (ed->running) {
        if (draw_screen(ed))
            debug(goto error);

        /*
         * Clear the return value after being displayed.
         * This is important, as some functions do not set or clear rv.
         */
        ed->rv = 0;

        if (get_ch(ed->ip, &ed->ch))
            debug(goto error);

        if (ed->ch == '\r')
            ed->ch = '\n';

        if (ed->ch >= CMD_ID_OFFSET)
            (*edf[ed->ch - CMD_ID_OFFSET])(ed);
        else if (ed->ch == '\n' && ed->cl_a)
            process_cl_operation(ed);
        else if (ed->ch <= UCHAR_MAX
            && (isprint(ed->ch) || ed->ch == '\t' || ed->ch == '\n')
            && gb_insert_ch(a_gb, (char) ed->ch))
            debug(goto error);
    }

    return free_editor(ed);

error:
    free_editor(ed);
    debug(return 1);
}
