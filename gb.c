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

/* Gap buffer module */

#include "toucanlib.h"

#define record_buf (b->mode == 'U' ? b->redo : b->undo)
#define replay_buf (b->mode == 'U' ? b->undo : b->redo)

#define START_GROUP                                                           \
    if (add_to_op_buf(record_buf, 'S', b->g, ' '))                            \
    return 1

#define END_GROUP                                                             \
    if (add_to_op_buf(record_buf, 'E', b->g, ' '))                            \
    return 1

static int add_to_op_buf(
    struct op_buf *op, unsigned char id, size_t g_loc, char ch)
{
    struct atomic_op *t;
    size_t new_n, new_s;

    if (op->i == op->n) {
        /* Grow */
        if (aof(op->n, 1, SIZE_MAX))
            return 1;

        new_n = op->n + 1;

        if (mof(new_n, 2, SIZE_MAX))
            return 1;

        new_n *= 2;

        if (mof(new_n, sizeof(struct atomic_op), SIZE_MAX))
            return 1;

        new_s = new_n * sizeof(struct atomic_op);

        if ((t = realloc(op->a, new_s)) == NULL)
            return 1;

        op->a = t;
        op->n = new_n;
    }

    (*(op->a + op->i)).id = id;
    (*(op->a + op->i)).g_loc = g_loc;
    (*(op->a + op->i)).ch = ch;

    ++op->i;

    return 0;
}

static struct op_buf *init_op_buf(size_t n)
{
    struct op_buf *op;

    if ((op = calloc(1, sizeof(struct op_buf))) == NULL)
        return NULL;

    if (mof(n, sizeof(struct atomic_op), SIZE_MAX)) {
        free(op);
        return NULL;
    }

    if ((op->a = calloc(n, sizeof(struct atomic_op))) == NULL) {
        free(op);
        return NULL;
    }

    op->n = n;
    return op;
}

static void free_op_buf(struct op_buf *op)
{
    if (op != NULL) {
        free(op->a);
        free(op);
    }
}

int reverse(struct gb *b, unsigned char mode)
{
    size_t depth = 0;

    switch (mode) {
    case 'U':
        b->mode = 'U';
        break;
    case 'R':
        b->mode = 'R';
        break;
    default:
        return 1;
    }

    if (!replay_buf->i)
        return NO_HISTORY;

    do {
        if (!replay_buf->i)
            break;

        /* Move into position */
        while (b->g > (*(replay_buf->a + replay_buf->i - 1)).g_loc)
            if (left_ch(b))
                break;

        while (b->g < (*(replay_buf->a + replay_buf->i - 1)).g_loc)
            if (right_ch(b))
                break;

        /* Check */
        if (b->g != (*(replay_buf->a + replay_buf->i - 1)).g_loc)
            return 1;

        /* Reverse the operation */
        switch ((*(replay_buf->a + replay_buf->i - 1)).id) {
        case 'S':
            if (add_to_op_buf(record_buf,
                    (*(replay_buf->a + replay_buf->i - 1)).id,
                    (*(replay_buf->a + replay_buf->i - 1)).g_loc,
                    (*(replay_buf->a + replay_buf->i - 1)).ch))
                return 1;

            ++depth;
            break;
        case 'E':
            if (add_to_op_buf(record_buf,
                    (*(replay_buf->a + replay_buf->i - 1)).id,
                    (*(replay_buf->a + replay_buf->i - 1)).g_loc,
                    (*(replay_buf->a + replay_buf->i - 1)).ch))
                return 1;

            --depth;
            break;
        case 'I':
            if (delete_ch(b))
                return 1;

            break;
        case 'D':
            if (insert_ch(b, (*(replay_buf->a + replay_buf->i - 1)).ch))
                return 1;

            if (left_ch(b))
                return 1;

            break;
        default:
            return 1;
        }

        --replay_buf->i;
    } while (depth);

    b->mode = 'N'; /* Normal */

    return 0;
}

void free_gb(struct gb *b)
{
    if (b != NULL) {
        free_op_buf(b->undo);
        free_op_buf(b->redo);
        free(b->fn);
        free(b->a);
        free(b);
    }
}

struct gb *init_gb(size_t s)
{
    struct gb *b;

    if ((b = calloc(1, sizeof(struct gb))) == NULL)
        return NULL;

    b->fn = NULL;
    if ((b->a = calloc(s, 1)) == NULL) {
        free(b);
        return NULL;
    }
    b->c = s - 1;
    b->e = s - 1;

    /* Last char cannot be deleted. Enables use as a string. Do not change. */
    *(b->a + s - 1) = '\0';

    b->r = 1;
    b->col = 1;

    if ((b->undo = init_op_buf(s)) == NULL) {
        free_gb(b);
        return NULL;
    }

    if ((b->redo = init_op_buf(s)) == NULL) {
        free_gb(b);
        return NULL;
    }

    return b;
}

void free_gb_list(struct gb *b)
{
    struct gb *t;

    if (b != NULL) {
        while (b->prev != NULL) b = b->prev;

        while (b != NULL) {
            t = b->next;
            free_gb(b);
            b = t;
        }
    }
}

void reset_gb(struct gb *b)
{
    /* fn, prev and next are preserved. Nothing is freed. */

    b->g = 0;
    b->c = b->e;
    b->m_set = 0;
    b->m = 0;
    b->r = 1;
    b->col = 1;
    b->sc_set = 0;
    b->sc = 0;
    b->d = 0;
    b->mod = 1;
    b->undo->i = 0;
    b->redo->i = 0;
}

static int grow_gap(struct gb *b, size_t will_use)
{
    unsigned char *t;
    size_t s, new_s, increase;

    if (will_use <= b->c - b->g)
        return 0; /* Nothing to do */

    s = b->e + 1; /* OK as in memory already */

    if (aof(s, will_use, SIZE_MAX))
        return 1;

    new_s = s + will_use;

    if (mof(new_s, 2, SIZE_MAX))
        return 1;

    new_s *= 2;

    if ((t = realloc(b->a, new_s)) == NULL)
        return 1;

    b->a = t;
    increase = new_s - s;
    memmove(b->a + b->c + increase, b->a + b->c, b->e - b->c + 1);

    /* Mark will be cleared upon modification, but just in case */
    if (b->m_set && b->m >= b->c)
        b->m += increase;

    b->c += increase;
    b->e += increase;
    return 0;
}

int insert_ch(struct gb *b, char ch)
{
    b->sc_set = 0;
    if (b->g == b->c && grow_gap(b, 1))
        return 1;

    if (add_to_op_buf(record_buf, 'I', b->g, ch))
        return 1;

    /*
     * Truncate the redo buffer under normal operations to prevent a fork
     * in history.
     */
    if (b->mode == 'N' && b->redo->i)
        b->redo->i = 0;

    *(b->a + b->g) = ch;
    ++b->g;
    if (ch == '\n') {
        ++b->r;
        b->col = 1;
    } else if (ch == '\t') {
        b->col += TAB_SIZE;
    } else {
        ++b->col;
    }
    b->m_set = 0;
    b->mod = 1;
    return 0;
}

int insert_str(struct gb *b, const char *str)
{
    char ch;

    START_GROUP;

    while ((ch = *str++) != '\0')
        if (insert_ch(b, ch))
            return 1;

    END_GROUP;

    return 0;
}

int insert_mem(struct gb *b, const char *mem, size_t mem_len)
{
    START_GROUP;

    while (mem_len) {
        if (insert_ch(b, *mem++))
            return 1;

        --mem_len;
    }

    END_GROUP;

    return 0;
}

int insert_file(struct gb *b, const char *fn)
{
    FILE *fp = NULL;
    int x;

    START_GROUP;

    b->sc_set = 0;

    errno = 0;
    if ((fp = fopen(fn, "rb")) == NULL) {
        if (errno == ENOENT) /* File does not exist */
            return 2;
        else
            return 1;
    }

    while ((x = getc(fp)) != EOF) {
        if (insert_ch(b, x)) {
            fclose(fp);
            return 1;
        }
    }
    if (ferror(fp) || !feof(fp)) {
        fclose(fp);
        return 1;
    }

    if (fclose(fp))
        return 1;

    start_of_gb(b);

    END_GROUP;

    return 0;
}

int delete_ch(struct gb *b)
{
    b->sc_set = 0;

    if (b->c == b->e)
        return 1;

    if (add_to_op_buf(record_buf, 'D', b->g, *(b->a + b->c)))
        return 1;

    /*
     * Truncate the redo buffer under normal operations to prevent a fork
     * in history.
     */
    if (b->mode == 'N' && b->redo->i)
        b->redo->i = 0;

    ++b->c;
    b->m_set = 0;
    b->mod = 1;
    return 0;
}

int left_ch(struct gb *b)
{
    size_t i, count;
    unsigned char u, ch;

    b->sc_set = 0;

    if (!b->g)
        return 1;

    --b->g;
    --b->c;
    *(b->a + b->c) = *(b->a + b->g);
    u = *(b->a + b->c);
    if (u == '\n') {
        --b->r;
        /* Need to work out col */
        i = b->g;
        count = 1;
        while (i) {
            --i;
            ch = *(b->a + i);
            if (ch == '\n')
                break;
            else if (ch == '\t')
                count += TAB_SIZE;
            else
                ++count;
        }
        b->col = count;
    } else if (u == '\t') {
        b->col -= TAB_SIZE;
    } else {
        --b->col;
    }
    /* Move mark across gap */
    if (b->m_set && b->m == b->g)
        b->m = b->c;

    return 0;
}

int right_ch(struct gb *b)
{
    unsigned char u;

    b->sc_set = 0;

    if (b->c == b->e)
        return 1;

    u = *(b->a + b->c);
    if (u == '\n') {
        ++b->r;
        b->col = 1;
    } else if (u == '\t') {
        b->col += TAB_SIZE;
    } else {
        ++b->col;
    }
    *(b->a + b->g) = *(b->a + b->c);
    /* Move mark across gap */
    if (b->m_set && b->m == b->c)
        b->m = b->g;

    ++b->g;
    ++b->c;
    return 0;
}

int backspace_ch(struct gb *b)
{
    START_GROUP;

    if (left_ch(b))
        return 1;

    if (delete_ch(b))
        return 1;

    END_GROUP;

    return 0;
}

void start_of_line(struct gb *b)
{
    while (b->col != 1) left_ch(b);
}

void end_of_line(struct gb *b)
{
    while (*(b->a + b->c) != '\n' && b->c != b->e) right_ch(b);
}

int up_line(struct gb *b)
{
    size_t r_orig = b->r, target_col;

    if (b->sc_set)
        target_col = b->sc;
    else
        target_col = b->col;

    /*
     * Row number starts from 1, not 0.
     * sc will still be set as left_ch has not been called yet.
     */
    if (b->r == 1)
        return 1;

    while (b->r == r_orig) left_ch(b);

    while (b->col > target_col) left_ch(b);

    /* left_ch will clear this, so need to do it again */
    b->sc_set = 1;
    b->sc = target_col;

    return 0;
}

int down_line(struct gb *b)
{
    int ret = 0;
    size_t r_orig = b->r, target_col;

    if (b->sc_set)
        target_col = b->sc;
    else
        target_col = b->col;

    while (b->r == r_orig)
        if (right_ch(b)) {
            /*
             * Trying to exceed end of buffer which is on the same line.
             * Go back.
             */
            while (b->col > target_col) left_ch(b);

            ret = 1;
            goto end;
        }

    while (b->col != target_col && *(b->a + b->c) != '\n')
        if (right_ch(b))
            break;

end:
    /* right_ch will clear this, so need to do it again */
    b->sc_set = 1;
    b->sc = target_col;

    return ret;
}

#define is_alpha_u(u) (isalpha(u) || (u) == '_')
#define is_alnum_u(u) (isalnum(u) || (u) == '_')

void left_word(struct gb *b)
{
    do
        if (left_ch(b))
            return;
    while (!is_alpha_u(*(b->a + b->c)));

    /* Look behind before moving, as want to stop at the start of a word */
    while (b->g && is_alpha_u(*(b->a + b->g - 1))) left_ch(b);
}

int right_word(struct gb *b, char transform)
{
    /*
     * Moves the cursor right one word. If transform is 'L' then uppercase
     * chars are converted to lowercase. Likewise, if transform is 'U' then
     * lowercase chars are converted to uppercase.
     */
    unsigned char u;

    START_GROUP;

    while (!is_alpha_u(*(b->a + b->c)))
        if (right_ch(b))
            return 0;

    do {
        u = *(b->a + b->c);
        if (isupper(u) && transform == 'L') {
            if (delete_ch(b))
                return 1;

            if (insert_ch(b, u - 'A' + 'a'))
                return 1;

        } else if (islower(u) && transform == 'U') {
            if (delete_ch(b))
                return 1;

            if (insert_ch(b, u - 'a' + 'A'))
                return 1;
        } else {
            if (right_ch(b))
                break;
        }
    } while (is_alnum_u(u));

    END_GROUP;

    return 0;
}

#undef is_alpha_u
#undef is_alnum_u

int goto_row(struct gb *b, struct gb *cl)
{
    size_t x;

    start_of_gb(cl);
    if (str_to_size_t((const char *) cl->a + cl->c, &x))
        return 1;

    start_of_gb(b);
    while (b->r != x)
        if (right_ch(b))
            return 1;

    return 0;
}

int insert_hex(struct gb *b, struct gb *cl)
{
    const unsigned char *str;
    unsigned char h1, h0, x;

    START_GROUP;

    start_of_gb(cl);
    str = cl->a + cl->c;
    while ((h1 = *str++) != '\0') {
        if ((h0 = *str++) == '\0')
            return 1;

        if (hex_to_val(h1, h0, &x))
            return 1;

        if (insert_ch(b, x))
            return 1;
    }

    END_GROUP;

    return 0;
}

void set_mark(struct gb *b)
{
    b->m_set = 1;
    b->m = b->c;
}

int swap_cursor_and_mark(struct gb *b)
{
    size_t m_orig, g_orig;

    if (!b->m_set)
        return 1;

    if (b->c > b->m) {
        m_orig = b->m;
        b->m = b->c;
        while (b->g != m_orig) left_ch(b);
    } else {
        g_orig = b->g;
        while (b->c != b->m) right_ch(b);

        b->m = g_orig;
    }

    return 0;
}

int exact_forward_search(struct gb *b, struct gb *cl)
{
    /* Moves cursor to the start of the match */
    const unsigned char *q;
    size_t num;

    start_of_gb(cl);

    if (b->c == b->e)
        return 1;

    if ((q = quick_search(
             b->a + b->c + 1, b->e - (b->c + 1), cl->a + cl->c, cl->e - cl->c))
        == NULL)
        return 1;

    num = q - (b->a + b->c);
    while (num--) right_ch(b);

    return 0;
}

int regex_forward_search(struct gb *b, struct gb *cl, int case_ins)
{
    /* Moves cursor to after the match */
    size_t match_offset, match_len, move;

    start_of_gb(cl);

    if (b->c == b->e)
        return 1;

    if (regex_search((char *) b->a + b->c + 1, b->e - (b->c + 1),
            *(b->a + b->c) == '\n' ? 1 : 0, (char *) cl->a + cl->c, 0,
            case_ins, &match_offset, &match_len, 0))
        return 1;

    move = 1 + match_offset + match_len;
    while (move) {
        right_ch(b);
        --move;
    }

    return 0;
}

int regex_replace_region(struct gb *b, struct gb *cl, int case_ins)
{
    int ret = 1;
    char delim, *find, *sep, *replace, *res = NULL;
    size_t res_len, count;

    START_GROUP;

    b->sc_set = 0;

    if (!b->m_set)
        mgoto(clean_up);

    start_of_gb(cl);
    if (cl->c == cl->e)
        mgoto(clean_up);

    delim = *(cl->a + cl->c);
    find = (char *) cl->a + cl->c + 1;
    if ((sep = memchr(find, delim, cl->e - (cl->c + 1))) == NULL)
        mgoto(clean_up);

    *sep = '\0';
    replace = sep + 1;

    /* Move cursor to start of region */
    if (b->c > b->m)
        if (swap_cursor_and_mark(b))
            mgoto(clean_up);

    if (regex_replace((char *) b->a + b->c, b->m - b->c, find, 0, case_ins,
            replace, &res, &res_len, 0))
        mgoto(clean_up);

    /* Delete region */
    count = b->m - b->c;
    while (count--)
        if (delete_ch(b))
            mgoto(clean_up);

    if (insert_mem(b, res, res_len))
        mgoto(clean_up);

    ret = 0;
clean_up:
    free(res);

    END_GROUP;

    return ret;
}

int match_bracket(struct gb *b)
{
    unsigned char orig_ch, target, ch;
    int move_right = 0;
    size_t depth, c_orig = b->c;

    orig_ch = *(b->a + b->c);
    switch (orig_ch) {
    case '<':
        target = '>';
        move_right = 1;
        break;
    case '[':
        target = ']';
        move_right = 1;
        break;
    case '{':
        target = '}';
        move_right = 1;
        break;
    case '(':
        target = ')';
        move_right = 1;
        break;
    case '>':
        target = '<';
        break;
    case ']':
        target = '[';
        break;
    case '}':
        target = '{';
        break;
    case ')':
        target = '(';
        break;
    default:
        return 1;
    }
    depth = 1;
    while (1) {
        if (move_right) {
            if (right_ch(b))
                break;
        } else {
            if (left_ch(b))
                break;
        }

        ch = *(b->a + b->c);
        if (ch == orig_ch)
            ++depth;

        if (ch == target)
            --depth;

        if (!depth)
            return 0;
    }

    /* Go back */
    if (move_right)
        while (b->c != c_orig) left_ch(b);
    else
        while (b->c != c_orig) right_ch(b);

    return 1;
}

int trim_clean(struct gb *b)
{
    size_t r_orig = b->r, col_orig = b->col;
    unsigned char ch;
    int eol;

    START_GROUP;

    end_of_gb(b);
    if (left_ch(b))
        return 0;

    if (*(b->a + b->c) == '\n')
        while (1) {
            if (left_ch(b))
                break;

            if (*(b->a + b->c) == '\n') {
                if (delete_ch(b)) /* Eat surplus trailing new lines */
                    return 1;
            } else {
                break;
            }
        }

    eol = 1;
    while (1) {
        ch = *(b->a + b->c);

        if (ch == '\n') {
            eol = 1;
        } else if (eol && (ch == ' ' || ch == '\t')) {
            if (delete_ch(b)) /* Eat trailing whitespace */
                return 1;
        } else if (!isprint(ch) && ch != '\t') {
            if (delete_ch(b))
                return 1;
        } else {
            eol = 0;
        }

        if (left_ch(b))
            break;
    }

    /* Move back */
    while (b->r != r_orig)
        if (right_ch(b))
            break;

    while (b->col != col_orig && *(b->a + b->c) != '\n')
        if (right_ch(b))
            break;

    END_GROUP;

    return 0;
}

int copy_region(struct gb *b, struct gb *p, int cut)
{
    /*
     * Region is:
     * mark (inclusive) to cursor (exclusive)
     * or
     * cursor (inclusive) to mark (exclusive).
     * The mark cannot be inside the gap.
     */
    size_t i, num;

    if (cut)
        START_GROUP;

    b->sc_set = 0;

    if (!b->m_set)
        return 1;

    reset_gb(p);

    if (b->m == b->c)
        return 0;

    if (b->m < b->c) {
        for (i = b->m; i < b->g; ++i)
            if (insert_ch(p, *(b->a + i)))
                return 1;

        if (cut) {
            num = b->g - b->m;
            while (num--)
                if (backspace_ch(b))
                    return 1;
        }
    } else {
        for (i = b->c; i < b->m; ++i)
            if (insert_ch(p, *(b->a + i)))
                return 1;

        if (cut) {
            num = b->m - b->c;
            while (num--)
                if (delete_ch(b))
                    return 1;
        }
    }

    /* Clear mark even when just copying */
    if (!cut)
        b->m_set = 0;

    if (cut)
        END_GROUP;

    return 0;
}

int cut_to_eol(struct gb *b, struct gb *p)
{
    if (*(b->a + b->c) == '\n')
        return delete_ch(b);

    b->m_set = 1;
    b->m = b->c;
    end_of_line(b);
    return copy_region(b, p, 1);
}

int cut_to_sol(struct gb *b, struct gb *p)
{
    b->m_set = 1;
    b->m = b->c;
    start_of_line(b);
    return copy_region(b, p, 1);
}

int word_under_cursor(struct gb *b, struct gb *tmp)
{
    unsigned char *p, *p_stop, u;
    p = b->a + b->c;
    p_stop = b->a + b->e; /* Exclusive */
    reset_gb(tmp);

    if ((u = *p) == ' ' || u == '\t')
        return 1; /* Invalid character */

    while ((u = *p) != ' ' && u != '\n' && u != '\t' && p != p_stop) {
        if (u && insert_ch(tmp, u)) /* Skip embedded \0 chars */
            return 1;

        ++p;
    }

    start_of_gb(tmp);
    if (b->g) {
        p = b->a + b->g - 1;
        while (1) {
            if ((u = *p) != ' ' && u != '\n' && u != '\t') {
                if (u) {
                    /* Skip embedded \0 chars */
                    if (insert_ch(tmp, u))
                        return 1;

                    if (left_ch(tmp))
                        return 1;
                }
            } else {
                break;
            }
            if (p == b->a)
                break;

            --p;
        }
    }
    return 0;
}

int copy_logical_line(struct gb *b, struct gb *tmp)
{
    /*
     * A backslash at the end of a line continues the logical line.
     * End-of-line backslashes are removed and the lines are combined.
     */

    /* Move to start of logical line */
    while (b->col != 1 || (b->g >= 2 && *(b->a + b->g - 2) == '\\'))
        left_ch(b);

    b->m_set = 1;
    b->m = b->c;

    /* Move to end of logical line */
    while ((*(b->a + b->c) != '\n' || (b->g && *(b->a + b->g - 1) == '\\'))
        && b->c != b->e)
        right_ch(b);

    if (copy_region(b, tmp, 0))
        return 1;

    /* Delete backslash at the end of lines and combine lines */
    start_of_gb(tmp);
    while (tmp->c != tmp->e) {
        switch (*(tmp->a + tmp->c)) {
        case '\\':
            if (tmp->c + 1 == tmp->e || *(tmp->a + tmp->c + 1) == '\n') {
                delete_ch(tmp); /* Remove backslash */
            } else {
                right_ch(tmp);
            }
            break;
        case '\n':
            delete_ch(tmp);
            break;
        default:
            right_ch(tmp);
            break;
        }
    }

    return 0;
}

int insert_shell_cmd(struct gb *b, const char *cmd, int *es)
{
    FILE *fp;
    int x, st;

    START_GROUP;

    /* Open a new line */
    if (insert_ch(b, '\n'))
        return 1;

    if ((fp = popen(cmd, "r")) == NULL)
        return 1;

    while ((x = getc(fp)) != EOF) {
        if ((isprint(x) || x == '\t' || x == '\n') && insert_ch(b, x)) {
            pclose(fp);
            return 1;
        }
    }
    if (ferror(fp) || !feof(fp)) {
        pclose(fp);
        return 1;
    }
    if ((st = pclose(fp)) == -1)
        return 1;

#ifndef _WIN32
    if (!WIFEXITED(st))
        return 1;

    st = WEXITSTATUS(st);
#endif

    *es = st;

    END_GROUP;

    return 0;
}

int shell_line(struct gb *b, struct gb *tmp, int *es)
{

    START_GROUP;

    if (copy_logical_line(b, tmp))
        return 1;

    end_of_gb(tmp);
    if (insert_str(tmp, " 2>&1"))
        return 1;

    /* Embedded \0 will terminate string early */
    start_of_gb(tmp);

    if (insert_shell_cmd(b, (const char *) tmp->a + tmp->c, es))
        return 1;

    END_GROUP;

    return 0;
}

int paste(struct gb *b, struct gb *p)
{
    size_t i;

    START_GROUP;

    for (i = 0; i < p->g; ++i)
        if (insert_ch(b, *(p->a + i)))
            return 1;

    /* Cursor should be at end of gap buffer, but just in case */
    for (i = p->c; i < p->e; ++i)
        if (insert_ch(b, *(p->a + i)))
            return 1;

    END_GROUP;

    return 0;
}

int save(struct gb *b)
{
    FILE *fp;

    b->sc_set = 0;

    if (b->fn == NULL || *b->fn == '\0')
        return 1;

    if ((fp = fopen_w(b->fn, 0)) == NULL)
        return 1;

    if (fwrite(b->a, 1, b->g, fp) != b->g) {
        fclose(fp);
        return 1;
    }
    if (fwrite(b->a + b->c, 1, b->e - b->c, fp) != b->e - b->c) {
        fclose(fp);
        return 1;
    }
    if (fclose(fp))
        return 1;

    b->mod = 0;

    return 0;
}

int rename_gb(struct gb *b, const char *fn)
{
    char *new_fn;

    b->sc_set = 0;

    if (fn == NULL)
        return 1;

    if ((new_fn = strdup(fn)) == NULL)
        return 1;

    free(b->fn);
    b->fn = new_fn;
    b->mod = 1;
    return 0;
}

int new_gb(struct gb **b, const char *fn, size_t s)
{
    struct gb *t = NULL;

    if ((t = init_gb(s)) == NULL)
        return 1;

    if (fn != NULL && *fn != '\0') {
        /* OK for file to not exist */
        if (insert_file(t, fn) == 1) {
            free_gb(t);
            return 1;
        }
        if (rename_gb(t, fn)) {
            free_gb(t);
            return 1;
        }
        t->mod = 0;
    }
    /* Link in */
    if (*b != NULL) {
        if ((*b)->next == NULL) {
            (*b)->next = t;
            t->prev = *b;
        } else {
            (*b)->next->prev = t;
            t->next = (*b)->next;
            (*b)->next = t;
            t->prev = *b;
        }
    }
    *b = t;
    return 0;
}

void remove_gb(struct gb **b)
{
    struct gb *t;

    /* Unlink */
    if ((t = *b) != NULL) {
        if ((*b)->prev == NULL) {
            /* Start of list */
            *b = (*b)->next;
            if (*b != NULL)
                (*b)->prev = NULL;
        } else if ((*b)->next == NULL) {
            /* End of list */
            *b = (*b)->prev;
            if (*b != NULL)
                (*b)->next = NULL;
        } else {
            /* Middle of list, so bypass */
            (*b)->prev->next = (*b)->next;
            (*b)->next->prev = (*b)->prev;
            *b = (*b)->prev; /* Move left by default */
        }
        free_gb(t);
    }
}
