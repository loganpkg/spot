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

/*
 * spot: text editor.
 * Dedicated to my son who was only a 4mm "spot" in his first ultrasound.
 *
 * Jesus answered, "Those who drink this water will get thirsty again,
 * but those who drink the water that I will give them will never be thirsty
 * again. The water that I will give them will become in them a spring which
 * will provide them with life-giving water and give them eternal life."
 *                                                          John 4:13-14 GNT
 */

#include "toucanlib.h"
#include <curses.h>

#define INIT_GB_SIZE 512

#define ESC 27

/* Excludes EKS */
#define MAX_KEY_SEQ 12

/* End of Key Sequence. Cannot use zero, like in a string, as zero is bound. */
#define EKS INT_MAX

#define clean_ch(ch)                                                          \
    (isprint(ch) || (ch) == '\t' || (ch) == '\n'                              \
            ? (ch)                                                            \
            : ((ch) == '\0' ? '~' : '?'))

#define z (ed->cl_active ? ed->cl : ed->b)

struct editor {
    int ret;     /* Return value of the text editor */
    int running; /* Indicates if the text editor is running */
    int rv;      /* Return value of last internal command */
    int es_set;
    int es;         /* Exit status of last shell command */
    char *sb;       /* Memory for status bar */
    size_t sb_s;    /* Size of status bar memory */
    char *msg;      /* Message on status bar (static storage) */
    int centre;     /* Vertically centre cursor on the screen */
    int clr;        /* Redraw physical screen */
    struct gb *b;   /* Text buffers linked together */
    struct gb *p;   /* Paste buffer */
    struct gb *cl;  /* Command line buffer */
    struct gb *se;  /* Search buffer */
    struct gb *tmp; /* Temporary buffer */
    /* ' ' = None, s = Exact search, z = Regex search, c = Case insen regex */
    char search_type;
    int cl_active; /* Cursor is in the command line */
    /* The command line operation which is in progress */
    char op; /* ' ' = None */
};

typedef void (*edFptr)(struct editor *);

struct key_binding {
    edFptr cmd_func;
    int key_seq[MAX_KEY_SEQ + 1];
};

static void free_editor(struct editor *ed)
{
    free(ed->sb);
    ed->sb = NULL;
    free_gb_list(ed->b);
    ed->b = NULL;
    free_gb(ed->p);
    ed->p = NULL;
    free_gb(ed->cl);
    ed->cl = NULL;
    free_gb(ed->se);
    ed->se = NULL;
    free_gb(ed->tmp);
    ed->tmp = NULL;
}

static int init_editor(struct editor *ed)
{
    ed->ret = 0;
    ed->running = 1;
    ed->rv = 0;
    ed->es_set = 0;
    ed->es = 0;
    ed->sb = NULL;
    ed->sb_s = 0;
    ed->msg = "";
    ed->centre = 0;
    ed->clr = 0;
    ed->b = NULL;
    ed->p = NULL;
    ed->cl = NULL;
    ed->se = NULL;
    ed->tmp = NULL;
    ed->search_type = ' ';
    ed->cl_active = 0;
    ed->op = ' ';

    if ((ed->p = init_gb(INIT_GB_SIZE)) == NULL)
        mgoto(clean_up);

    if ((ed->cl = init_gb(INIT_GB_SIZE)) == NULL)
        mgoto(clean_up);

    if ((ed->se = init_gb(INIT_GB_SIZE)) == NULL)
        mgoto(clean_up);

    if ((ed->tmp = init_gb(INIT_GB_SIZE)) == NULL)
        mgoto(clean_up);

    return 0;

clean_up:
    free_editor(ed);
    return 1;
}

static void commence_cl_cmd(struct editor *ed, char op)
{
    reset_gb(ed->cl);
    ed->cl_active = 1;
    ed->op = op;
}

/* ****************** Navigation commands ******************* */
/* These commands do not have undo. */

static void ed_left_ch(struct editor *ed)
{
    ed->rv = left_ch(z);
}

static void ed_right_ch(struct editor *ed)
{
    ed->rv = right_ch(z);
}

static void ed_up_line(struct editor *ed)
{
    ed->rv = up_line(z);
}

static void ed_down_line(struct editor *ed)
{
    ed->rv = down_line(z);
}

static void ed_left_word(struct editor *ed)
{
    left_word(z);
}

static void ed_right_word(struct editor *ed)
{
    ed->rv = right_word(z, ' ');
}

static void ed_start_of_line(struct editor *ed)
{
    start_of_line(z);
}

static void ed_end_of_line(struct editor *ed)
{
    end_of_line(z);
}

static void ed_start_of_buffer(struct editor *ed)
{
    start_of_gb(z);
}

static void ed_end_of_buffer(struct editor *ed)
{
    end_of_gb(z);
}

static void ed_match_bracket(struct editor *ed)
{
    ed->rv = match_bracket(z);
}

static void ed_swap_cursor_and_mark(struct editor *ed)
{
    ed->rv = swap_cursor_and_mark(z);
}

/* ********************************************************** */

/* ***************** Basic editing commands ***************** */

static void ed_delete_ch(struct editor *ed)
{
    ed->rv = delete_ch(z);
}

static void ed_backspace_ch(struct editor *ed)
{
    ed->rv = backspace_ch(z);
}

static void ed_lowercase_word(struct editor *ed)
{
    ed->rv = right_word(z, 'L');
}

static void ed_uppercase_word(struct editor *ed)
{
    ed->rv = right_word(z, 'U');
}

static void ed_shell_current_line(struct editor *ed)
{
    ed->rv = shell_line(z, ed->tmp, &ed->es);
    if (!ed->rv)
        ed->es_set = 1;
}

static void ed_trim_clean(struct editor *ed)
{
    ed->rv = trim_clean(z);
}

/* ********************************************************** */

/* **************** Copy and paste commands ***************** */

static void ed_set_mark(struct editor *ed)
{
    set_mark(z);
}

static void ed_escape_cl(struct editor *ed)
{
    /* Clears the mark OR escapes from the command line */
    if (z->m_set) {
        z->m_set = 0;
        z->m = 0;
    } else if (ed->cl_active) {
        reset_gb(ed->cl);
        ed->cl_active = 0;
    }
}

static void ed_copy_region(struct editor *ed)
{
    ed->rv = copy_region(z, ed->p, 0);
}

static void ed_cut_region(struct editor *ed)
{
    ed->rv = copy_region(z, ed->p, 1);
}

static void ed_cut_to_eol(struct editor *ed)
{
    ed->rv = cut_to_eol(z, ed->p);
}

static void ed_cut_to_sol(struct editor *ed)
{
    ed->rv = cut_to_sol(z, ed->p);
}

static void ed_paste(struct editor *ed)
{
    ed->rv = paste(z, ed->p);
}

/* ********************************************************** */

/* ***************** Editor level commands ****************** */
/* These commands do not have undo. */

static void ed_clear_screen(struct editor *ed)
{
    /* Clears and centres the screen */
    ed->centre = 1;
    ed->clr = 1;
}

static void ed_left_buffer(struct editor *ed)
{
    if (ed->b->prev != NULL)
        ed->b = ed->b->prev;
}

static void ed_right_buffer(struct editor *ed)
{
    if (ed->b->next != NULL)
        ed->b = ed->b->next;
}

static void ed_save_buffer(struct editor *ed)
{
    ed->rv = save(ed->b); /* Cannot save the command line */
}

static void ed_remove_buffer(struct editor *ed)
{
    remove_gb(&ed->b);
    if (ed->b == NULL)
        ed->running = 0;
}

static void ed_close_editor(struct editor *ed)
{
    ed->running = 0;
}

/* ********************************************************** */

/* *********** Commands that use the command line *********** */

/* No undo: */

static void ed_set_filename(struct editor *ed)
{
    commence_cl_cmd(ed, '=');

    /* Load existing filename */
    if (ed->b->fn != NULL && insert_str(ed->cl, ed->b->fn))
        reset_gb(ed->cl);
}

static void ed_goto_row(struct editor *ed)
{
    commence_cl_cmd(ed, 'u');
}

static void ed_forward_search(struct editor *ed)
{
    commence_cl_cmd(ed, 's');
}

static void ed_regex_search(struct editor *ed)
{
    commence_cl_cmd(ed, 'z');
}

static void ed_regex_search_case_ins(struct editor *ed)
{
    commence_cl_cmd(ed, 'a');
}

/* With undo: */

static void ed_regex_rep(struct editor *ed)
{
    commence_cl_cmd(ed, 'r');
}

static void ed_regex_rep_case_ins(struct editor *ed)
{
    commence_cl_cmd(ed, 'b');
}

static void ed_insert_hex(struct editor *ed)
{
    commence_cl_cmd(ed, 'q');
}

static void ed_insert_shell_cmd(struct editor *ed)
{
    commence_cl_cmd(ed, '$');
}

static void ed_open_file(struct editor *ed)
{
    commence_cl_cmd(ed, 'f');
}

static void ed_insert_file(struct editor *ed)
{
    commence_cl_cmd(ed, 'i');
}

/* ********************************************************** */

/* ********************* Undo and redo ********************** */
static void ed_undo(struct editor *ed)
{
    ed->rv = reverse(ed->b, 'U');
    if (ed->rv == NO_HISTORY)
        ed->msg = "No more undo";
}

static void ed_redo(struct editor *ed)
{
    ed->rv = reverse(ed->b, 'R');
    if (ed->rv == NO_HISTORY)
        ed->msg = "No more redo";
}

/* ********************************************************** */

/* ******************** Special commands ******************** */

static void ed_repeat_search(struct editor *ed)
{
    /* Repeat last search */
    switch (ed->search_type) {
    case 's':
        ed->rv = exact_forward_search(ed->b, ed->se);
        break;
    case 'z':
        ed->rv = regex_forward_search(ed->b, ed->se, 0);
        break;
    case 'c': /* Case insensitive */
        ed->rv = regex_forward_search(ed->b, ed->se, 1);
        break;
    default:
        ed->rv = 1;
        break;
    }
}

static void ed_execute_cl(struct editor *ed)
{
    struct gb *t; /* For switching gap buffers */
    if (ed->cl_active) {
        switch (ed->op) {
        case 's':
        case 'z':
        case 'a':
            /* Swap gap buffers */
            t = ed->se;
            ed->se = ed->cl;
            ed->cl = t;
            reset_gb(ed->cl);
            switch (ed->op) {
            case 's':
                ed->search_type = 's';
                ed->rv = exact_forward_search(ed->b, ed->se);
                break;
            case 'z':
                ed->search_type = 'z';
                ed->rv = regex_forward_search(ed->b, ed->se, 0);
                break;
            case 'a':
                ed->search_type = 'c';
                ed->rv = regex_forward_search(ed->b, ed->se, 1);
                break;
            }
            break;
        case 'r':
            ed->rv = regex_replace_region(ed->b, ed->cl, 0);
            break;
        case 'b':
            ed->rv = regex_replace_region(ed->b, ed->cl, 1);
            break;
        case '=':
            start_of_gb(ed->cl);
            ed->rv = rename_gb(ed->b, (const char *) ed->cl->a + ed->cl->c);
            break;
        case 'u':
            ed->rv = goto_row(ed->b, ed->cl);
            break;
        case 'q':
            ed->rv = insert_hex(ed->b, ed->cl);
            break;
        case 'f':
            start_of_gb(ed->cl);
            ed->rv = new_gb(
                &ed->b, (const char *) ed->cl->a + ed->cl->c, INIT_GB_SIZE);
            break;
        case 'i':
            start_of_gb(ed->cl);
            ed->rv = insert_file(ed->b, (const char *) ed->cl->a + ed->cl->c);
            break;
        case '$':
            start_of_gb(ed->cl);
            ed->rv = insert_shell_cmd(
                ed->b, (const char *) ed->cl->a + ed->cl->c, &ed->es);
            if (!ed->rv)
                ed->es_set = 1;

            break;
        }
        ed->cl_active = 0;
        ed->op = ' ';
    } else {
        ed->rv = insert_ch(ed->b, '\n');
    }
}

/* ********************************************************** */

int draw(struct editor *ed)
{
    size_t up, target_up, ts, i;
    int have_centred = 0;
    unsigned char ch;
    char num_str[NUM_BUF_SIZE];
    int r, len;
    size_t h;  /* Screen height */
    size_t w;  /* Screen width */
    size_t s;  /* Screen size */
    size_t y;  /* Current vertical position on screen */
    size_t x;  /* Current horizontal position on screen */
    size_t si; /* Current screen index */
    /* Final vertical cursor position on screen */
    size_t c_y;
    /* Final horizontal cursor position on screen */
    size_t c_x;
    char *tmp;

    /* For easier reference */
    struct gb *b = ed->b;
    struct gb *cl = ed->cl;

start:
    if (ed->clr) {
        if (clear() == ERR)
            return 1;

        ed->clr = 0;

    } else {
        if (erase() == ERR)
            return 1;
    }

    getmaxyx(stdscr, h, w);
    s = h * w; /* Cannot overflow as already in memory */

    if (b->d > b->g || ed->centre) {
        /* Does not consider long lines that wrap */
        b->d = b->g;
        up = 0;
        target_up = h <= 4 ? 1 : (h - 1) / 2;
        while (b->d) {
            --b->d;
            if (*(b->a + b->d) == '\n')
                ++up;

            if (up == target_up) {
                ++b->d;
                break;
            }
        }
        ed->centre = 0;
        have_centred = 1;
    }

    /* Size of the text portion of the screen */
    ts = s - (h >= 2 ? w : 0) - (h >= 3 ? w : 0);

    standend();

    /* Region commenced before draw start */
    if (b->m_set && b->m < b->d)
        standout();

    /* Before the gap */
    i = b->d;
    getyx(stdscr, y, x);
    si = y * w + x;

    while (i < b->g && si < ts) {
        if (b->m_set && b->m == i)
            standout();

        ch = *((b)->a + i);
        addch(clean_ch(ch));
        ++i;

        getyx(stdscr, y, x);
        si = y * w + x;
    }

    if (si >= ts) {
        /* Off screen */
        if (!have_centred)
            ed->centre = 1;
        else
            b->d = b->g;
        goto start;
    }

    /* At the cursor */
    c_y = y;
    c_x = x;

    if (b->m_set) {
        if (b->m < b->c)
            standend(); /* End of region */
        else
            standout(); /* Start of region */
    }

    /* After the gap (from the cursor onwards) */
    i = b->c;
    getyx(stdscr, y, x);
    si = y * w + x;

    while (i <= b->e && si < ts) {
        if (b->m_set && b->m == i)
            standend();

        ch = *((b)->a + i);
        addch(clean_ch(ch));
        ++i;

        getyx(stdscr, y, x);
        si = y * w + x;
    }

    if (h >= 2) {
        /* Status bar */

        if (ed->es_set) {
            r = snprintf(num_str, NUM_BUF_SIZE, "%d", ed->es);
            if (r < 0 || r >= NUM_BUF_SIZE)
                return 1;
        } else {
            *num_str = '\0';
        }

        move(h - 2, 0);

        if (ed->sb_s < w) {
            if ((tmp = realloc(ed->sb, w + 1)) == NULL)
                return 1;

            ed->sb = tmp;
            ed->sb_s = w + 1;
        }

        len = snprintf(ed->sb, ed->sb_s, "%c%c %s (%lu,%lu) %02X %s %s",
            ed->rv ? '!' : ' ', b->mod ? '*' : ' ', b->fn,
            (unsigned long) b->r, (unsigned long) b->col,
            ed->cl_active ? *(cl->a + cl->c) : *(b->a + b->c), num_str,
            ed->msg);
        if (len < 0)
            return 1;

        if ((size_t) len < ed->sb_s) {
            memset(ed->sb + len, ' ', w - len);
            *(ed->sb + w) = '\0';
        }

        standout();

        if (addnstr(ed->sb, w) == ERR)
            return 1;

        standend();
    }

    if (h >= 3) {
        /* Command line */
    cl_start:
        standend();

        if (cl->m_set && cl->m < cl->d)
            standout();

        /* Start of last line in virtual screen */
        move(h - 1, 0);

        /* Erase virtual line */
        clrtoeol();

        if (cl->d > cl->g)
            cl->d = cl->g;

        /* Before the gap */
        i = cl->d;
        getyx(stdscr, y, x);
        si = y * w + x;

        while (i < cl->g && si < s) {
            if (cl->m_set && cl->m == i)
                standout();

            ch = *((cl)->a + i);
            addch(clean_ch(ch));
            ++i;

            getyx(stdscr, y, x);
            si = y * w + x;
        }

        if (si >= s) {
            /* Off screen */
            cl->d = cl->g; /* Draw from cursor */
            goto cl_start;
        }

        /* At the cursor */
        if (ed->cl_active) {
            c_y = y;
            c_x = x;
        }

        if (cl->m_set) {
            if (cl->m < cl->c)
                standend(); /* End of region */
            else
                standout(); /* Start of region */
        }

        /* After the gap (from the cursor onwards) */
        i = cl->c;
        while (i <= cl->e && si < s) {
            if (cl->m_set && cl->m == i)
                standend();

            ch = *((cl)->a + i);
            addch(clean_ch(ch));
            ++i;

            getyx(stdscr, y, x);
            si = y * w + x;
        }
    }

    move(c_y, c_x);
    refresh();

    return 0;
}

static int check_key_seq(int *keys, size_t num, const int *binding)
{
    size_t i;

    for (i = 0; i < num; ++i)
        if (keys[i] != binding[i])
            return NO_MATCH;

    if (binding[i] == EKS)
        return MATCH;

    return PARTIAL_MATCH;
}

static int keys_to_command(
    struct editor *ed, const struct key_binding *kb, size_t kb_num)
{
    int r, keep_matching;
    int keys[MAX_KEY_SEQ];
    size_t k_i, i;

    k_i = 0;

    while (1) {
    top:
        if (draw(ed))
            return 1;

        /* Clear message after displayed once */
        ed->msg = "";

        keys[k_i++] = getch();

        keep_matching = 0;
        for (i = 0; i < kb_num; ++i) {
            if ((r = check_key_seq(keys, k_i, kb[i].key_seq)) == MATCH) {
                ed->rv = 0;
                ed->es_set = 0;
                ed->es = 0;

                (kb[i].cmd_func)(ed); /* Execute command */

                if (!ed->running)
                    return 0;

                k_i = 0; /* Eat the read sequence */
                goto top;
            } else if (r == PARTIAL_MATCH) {
                keep_matching = 1;
            }
        }
        if (!keep_matching) {
            /* Return keys */
            while (k_i >= 2) {
                if (ungetch(keys[k_i - 1]) == ERR)
                    return 1;

                --k_i;
            }

            /* Literally insert first key */
            if (isprint(keys[k_i - 1]) || keys[k_i - 1] == '\t')
                ed->rv = insert_ch(z, keys[k_i - 1]);

            --k_i;
            if (k_i)
                return 1;
        }
    }
}

int main(int argc, char **argv)
{
    const struct key_binding kb[] = {
        { &ed_left_ch, { KEY_LEFT, EKS } },
        { &ed_left_ch, { C('b'), EKS } },
        { &ed_right_ch, { KEY_RIGHT, EKS } },
        { &ed_right_ch, { C('f'), EKS } },
        { &ed_up_line, { KEY_UP, EKS } },
        { &ed_up_line, { C('p'), EKS } },
        { &ed_down_line, { KEY_DOWN, EKS } },
        { &ed_down_line, { C('n'), EKS } },
        { &ed_delete_ch, { KEY_DC, EKS } },
        { &ed_delete_ch, { C('d'), EKS } },
        { &ed_backspace_ch, { KEY_BACKSPACE, EKS } },
        { &ed_backspace_ch, { C('h'), EKS } },
        { &ed_backspace_ch, { 127, EKS } },
        { &ed_start_of_line, { KEY_HOME, EKS } },
        { &ed_start_of_line, { C('a'), EKS } },
        { &ed_end_of_line, { KEY_END, EKS } },
        { &ed_end_of_line, { C('e'), EKS } },
        { &ed_set_mark, { 0, EKS } },
        { &ed_set_mark, { ESC, 2, EKS } },
        { &ed_set_mark, { ESC, '@', EKS } },
        { &ed_escape_cl, { C('g'), EKS } },
        { &ed_clear_screen, { C('l'), EKS } },
        { &ed_cut_region, { C('w'), EKS } },
        { &ed_paste, { C('y'), EKS } },
        { &ed_cut_to_eol, { C('k'), EKS } },
        { &ed_trim_clean, { C('t'), EKS } },
        { &ed_forward_search, { C('s'), EKS } },
        { &ed_regex_search, { C('z'), EKS } },
        { &ed_regex_rep, { C('r'), EKS } },
        { &ed_goto_row, { C('u'), EKS } },
        { &ed_insert_hex, { C('q'), EKS } },
        { &ed_left_word, { ESC, 'b', EKS } },
        { &ed_right_word, { ESC, 'f', EKS } },
        { &ed_lowercase_word, { ESC, 'l', EKS } },
        { &ed_uppercase_word, { ESC, 'u', EKS } },
        { &ed_cut_to_sol, { ESC, 'k', EKS } },
        { &ed_match_bracket, { ESC, 'm', EKS } },
        { &ed_copy_region, { ESC, 'w', EKS } },
        { &ed_remove_buffer, { ESC, '!', EKS } },
        { &ed_set_filename, { ESC, '/', EKS } },
        { &ed_regex_search_case_ins, { ESC, 'z', EKS } },
        { &ed_regex_rep_case_ins, { ESC, 'r', EKS } },
        { &ed_insert_shell_cmd, { ESC, '$', EKS } },
        { &ed_shell_current_line, { ESC, '`', EKS } },
        { &ed_start_of_buffer, { ESC, '<', EKS } },
        { &ed_end_of_buffer, { ESC, '>', EKS } },
        { &ed_swap_cursor_and_mark, { C('x'), C('x'), EKS } },
        { &ed_close_editor, { C('x'), C('c'), EKS } },
        { &ed_save_buffer, { C('x'), C('s'), EKS } },
        { &ed_open_file, { C('x'), C('f'), EKS } },
        { &ed_insert_file, { C('x'), 'i', EKS } },
        { &ed_left_buffer, { C('x'), KEY_LEFT, EKS } },
        { &ed_right_buffer, { C('x'), KEY_RIGHT, EKS } },
        { &ed_repeat_search, { ESC, 'n', EKS } },
        { &ed_undo, { ESC, '-', EKS } },
        { &ed_redo, { ESC, '=', EKS } },

        /* Do not change these two: */
        { &ed_execute_cl, { '\r', EKS } },
        { &ed_execute_cl, { '\n', EKS } },
    };

    struct editor ed;
    int ret = 0;
    int i;

    if (init_editor(&ed))
        mgoto(clean_up);

    if (initscr() == NULL)
        mgoto(clean_up);

    if (raw() == ERR)
        mgoto(clean_up);

    if (noecho() == ERR)
        mgoto(clean_up);

    if (keypad(stdscr, TRUE) == ERR)
        mgoto(clean_up);

    if (nodelay(stdscr, FALSE) == ERR)
        mgoto(clean_up);

    if (set_tabsize(8) == ERR)
        mgoto(clean_up);

    if (argc > 1) {
        for (i = 1; i < argc; ++i)
            if (new_gb(&ed.b, *(argv + i), INIT_GB_SIZE))
                mgoto(clean_up);

        while (ed.b->prev) ed.b = ed.b->prev;
    } else {
        /* No args */
        if (new_gb(&ed.b, NULL, INIT_GB_SIZE))
            mgoto(clean_up);
    }

    ret = keys_to_command(&ed, kb, sizeof(kb) / sizeof(struct key_binding));

clean_up:
    if (endwin() == ERR)
        ret = 1;

    free_editor(&ed);

    return ret;
}
