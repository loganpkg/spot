/*
 * Copyright (c) 2023 Logan Ryan McLintock
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

/* Gap buffer module */

#ifndef GB_H
#define GB_H

#include <stddef.h>

#define start_of_gb(b) while (!left_ch(b))

#define end_of_gb(b) while (!right_ch(b))

struct gb {
    char *fn;
    unsigned char *a;
    size_t g;                   /* Gap start */
    size_t c;                   /* Cursor */
    size_t e;                   /* End of buffer */
    int m_set;                  /* Mark set */
    size_t m;                   /* Mark */
    size_t r;                   /* Row number (starts from 1) */
    size_t col;                 /* Column number (starts from 0) */
    size_t d;                   /* Draw start */
    int mod;                    /* Modified */
    struct gb *prev;
    struct gb *next;
};

/* Function declarations */
struct gb *init_gb(size_t s);
void free_gb(struct gb *b);
void free_gb_list(struct gb *b);
void delete_gb(struct gb *b);
int insert_ch(struct gb *b, char ch);
int insert_str(struct gb *b, const char *str);
int insert_mem(struct gb *b, const char *mem, size_t mem_len);
int insert_file(struct gb *b, const char *fn);
int delete_ch(struct gb *b);
int left_ch(struct gb *b);
int right_ch(struct gb *b);
int backspace_ch(struct gb *b);
void start_of_line(struct gb *b);
void end_of_line(struct gb *b);
int up_line(struct gb *b);
int down_line(struct gb *b);
void left_word(struct gb *b);
void right_word(struct gb *b, char transform);
int goto_row(struct gb *b, struct gb *cl);
int insert_hex(struct gb *b, struct gb *cl);
int swap_cursor_and_mark(struct gb *b);
int exact_forward_search(struct gb *b, struct gb *cl);
int regex_forward_search(struct gb *b, struct gb *cl);
int regex_replace_region(struct gb *b, struct gb *cl);
int match_bracket(struct gb *b);
void trim_clean(struct gb *b);
int copy_region(struct gb *b, struct gb *p, int cut);
int cut_to_eol(struct gb *b, struct gb *p);
int cut_to_sol(struct gb *b, struct gb *p);
int word_under_cursor(struct gb *b, struct gb *tmp);
int copy_logical_line(struct gb *b, struct gb *tmp);
int insert_shell_cmd(struct gb *b, const char *cmd, int *es);
int shell_line(struct gb *b, struct gb *tmp, int *es);
int paste(struct gb *b, struct gb *p);
int save(struct gb *b);
int rename_gb(struct gb *b, const char *fn);
int new_gb(struct gb **b, const char *fn, size_t s);
void remove_gb(struct gb **b);

#endif
