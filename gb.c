/*
 * Copyright (c) 2023 Logan Ryan McLintock
 *
 * Permission to use, copy, modify, and/or distribute this software for any
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

/* Gap buffer module */

#include "toucanlib.h"


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
    return b;
}

void free_gb(struct gb *b)
{
    if (b != NULL) {
        free(b->fn);
        free(b->a);
        free(b);
    }
}

void free_gb_list(struct gb *b)
{
    struct gb *t;

    if (b != NULL) {
        while (b->prev != NULL)
            b = b->prev;

        while (b != NULL) {
            t = b->next;
            free_gb(b);
            b = t;
        }
    }
}

void delete_gb(struct gb *b)
{
    /* Soft delete */
    b->g = 0;
    b->c = b->e;
    b->m_set = 0;
    b->m = 0;
    b->r = 1;
    b->col = 0;
    b->sc_set = 0;
    b->sc = 0;
    b->d = 0;
    b->mod = 1;
}

static int grow_gap(struct gb *b, size_t will_use)
{
    unsigned char *t;
    size_t s, new_s, increase;

    if (will_use <= b->c - b->g)
        return 0;               /* Nothing to do */

    s = b->e + 1;               /* OK as in memory already */

    if (aof(s, will_use, SIZE_MAX))
        return GEN_ERROR;

    new_s = s + will_use;

    if (mof(new_s, 2, SIZE_MAX))
        return GEN_ERROR;

    new_s *= 2;

    if ((t = realloc(b->a, new_s)) == NULL)
        return GEN_ERROR;

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
        return GEN_ERROR;

    *(b->a + b->g) = ch;
    ++b->g;
    if (ch == '\n') {
        ++b->r;
        b->col = 0;
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

    while ((ch = *str++) != '\0')
        if (insert_ch(b, ch))
            return GEN_ERROR;

    return 0;
}

int insert_mem(struct gb *b, const char *mem, size_t mem_len)
{
    while (mem_len) {
        if (insert_ch(b, *mem++))
            return GEN_ERROR;

        --mem_len;
    }

    return 0;
}

int insert_file(struct gb *b, const char *fn)
{
    int ret = GEN_ERROR;
    FILE *fp = NULL;
    size_t fs;

    b->sc_set = 0;

    errno = 0;
    if ((fp = fopen(fn, "rb")) == NULL) {
        if (errno == ENOENT)    /* File does not exist */
            return 2;
        else
            return GEN_ERROR;
    }

    if (get_file_size(fn, &fs))
        mgoto(clean_up);

    if (!fs)
        goto done;

    if (fs > b->c - b->g && grow_gap(b, fs))
        mgoto(clean_up);

    /* Right side of gap insert */
    if (fread(b->a + b->c - fs, 1, fs, fp) != fs)
        mgoto(clean_up);

    b->c -= fs;
    b->m_set = 0;
    b->mod = 1;

  done:
    ret = 0;
  clean_up:
    if (fp != NULL)
        if (fclose(fp))
            ret = GEN_ERROR;

    return ret;
}

int delete_ch(struct gb *b)
{
    b->sc_set = 0;

    if (b->c == b->e)
        return GEN_ERROR;

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
        return GEN_ERROR;

    --b->g;
    --b->c;
    *(b->a + b->c) = *(b->a + b->g);
    u = *(b->a + b->c);
    if (u == '\n') {
        --b->r;
        /* Need to work out col */
        i = b->g;
        count = 0;
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
        return GEN_ERROR;

    u = *(b->a + b->c);
    if (u == '\n') {
        ++b->r;
        b->col = 0;
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
    if (left_ch(b))
        return GEN_ERROR;

    return delete_ch(b);
}

void start_of_line(struct gb *b)
{
    while (b->col)
        left_ch(b);
}

void end_of_line(struct gb *b)
{
    while (*(b->a + b->c) != '\n' && b->c != b->e)
        right_ch(b);
}

int up_line(struct gb *b)
{
    size_t r_orig = b->r, target_col;

    if (b->sc_set)
        target_col = b->sc;
    else
        target_col = b->col;

    /* Row number starts from 1, not 0 */
    if (b->r == 1)
        return GEN_ERROR;

    while (b->r == r_orig)
        left_ch(b);

    while (b->col > target_col)
        left_ch(b);

    /* left_ch will clear this, so need to do it again */
    b->sc_set = 1;
    b->sc = target_col;

    return 0;
}

int down_line(struct gb *b)
{
    size_t r_orig = b->r, target_col;

    if (b->sc_set)
        target_col = b->sc;
    else
        target_col = b->col;

    while (b->r == r_orig) {
        if (right_ch(b)) {
            /* Go back */
            while (b->col != target_col)
                left_ch(b);

            return GEN_ERROR;
        }
    }

    while (b->col != target_col && *(b->a + b->c) != '\n')
        if (right_ch(b))
            break;

    /* right_ch will clear this, so need to do it again */
    b->sc_set = 1;
    b->sc = target_col;

    return 0;
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
    while (b->g && is_alpha_u(*(b->a + b->g - 1)))
        left_ch(b);
}

void right_word(struct gb *b, char transform)
{
    /*
     * Moves the cursor right one word. If transform is 'L' then uppercase
     * chars are converted to lowercase. Likewise, if transform is 'U' then
     * lowercase chars are converted to uppercase.
     */
    unsigned char u;

    while (!is_alpha_u(*(b->a + b->c)))
        if (right_ch(b))
            return;

    u = *(b->a + b->c);
    do {
        if (isupper(u) && transform == 'L') {
            *(b->a + b->c) = 'a' + u - 'A';
            b->m_set = 0;
            b->mod = 1;
        } else if (islower(u) && transform == 'U') {
            *(b->a + b->c) = 'A' + u - 'a';
            b->m_set = 0;
            b->mod = 1;
        }

        if (right_ch(b))
            break;

        u = *(b->a + b->c);
    } while (is_alnum_u(u));
}

#undef is_alpha_u
#undef is_alnum_u


int goto_row(struct gb *b, struct gb *cl)
{
    size_t x;

    start_of_gb(cl);
    if (str_to_size_t((const char *) cl->a + cl->c, &x))
        return GEN_ERROR;

    start_of_gb(b);
    while (b->r != x)
        if (right_ch(b))
            return GEN_ERROR;

    return 0;
}

int insert_hex(struct gb *b, struct gb *cl)
{
    const unsigned char *str;
    unsigned char h1, h0, x;

    start_of_gb(cl);
    str = cl->a + cl->c;
    while ((h1 = *str++) != '\0') {
        if ((h0 = *str++) == '\0')
            return GEN_ERROR;

        if (hex_to_val(h1, h0, &x))
            return GEN_ERROR;

        if (insert_ch(b, x))
            return GEN_ERROR;
    }
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
        return GEN_ERROR;

    if (b->c > b->m) {
        m_orig = b->m;
        b->m = b->c;
        while (b->g != m_orig)
            left_ch(b);
    } else {
        g_orig = b->g;
        while (b->c != b->m)
            right_ch(b);

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
        return GEN_ERROR;

    if ((q =
         quick_search(b->a + b->c + 1, b->e - (b->c + 1), cl->a + cl->c,
                      cl->e - cl->c)) == NULL)
        return GEN_ERROR;

    num = q - (b->a + b->c);
    while (num--)
        right_ch(b);

    return 0;
}

int regex_forward_search(struct gb *b, struct gb *cl)
{
    /* Moves cursor to after the match */
    size_t match_offset, match_len, move;

    start_of_gb(cl);

    if (b->c == b->e)
        return GEN_ERROR;

    if (regex_search
        ((char *) b->a + b->c + 1,
         b->e - (b->c + 1),
         *(b->a + b->c) == '\n' ? 1 : 0,
         (char *) cl->a + cl->c, 0, &match_offset, &match_len, 0))
        return GEN_ERROR;

    move = 1 + match_offset + match_len;
    while (move) {
        right_ch(b);
        --move;
    }

    return 0;
}

int regex_replace_region(struct gb *b, struct gb *cl)
{
    int ret = GEN_ERROR;
    char delim, *find, *sep, *replace, *res = NULL;
    size_t res_len;

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

    if (regex_replace((char *) b->a + b->c,
                      b->m - b->c, find, 0, replace, &res, &res_len, 0))
        mgoto(clean_up);

    /* Delete region */
    b->c = b->m;
    b->m_set = 0;
    b->mod = 1;

    if (insert_mem(b, res, res_len))
        mgoto(clean_up);

    ret = 0;
  clean_up:
    free(res);

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
        return GEN_ERROR;
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
        while (b->c != c_orig)
            left_ch(b);
    else
        while (b->c != c_orig)
            right_ch(b);

    return GEN_ERROR;
}

void trim_clean(struct gb *b)
{
    size_t r_orig = b->r, col_orig = b->col;
    unsigned char ch;
    int eol;

    end_of_gb(b);
    if (left_ch(b))
        return;

    if (*(b->a + b->c) == '\n')
        while (1) {
            if (left_ch(b))
                break;

            if (*(b->a + b->c) == '\n')
                delete_ch(b);   /* Eat surplus trailing new lines */
            else
                break;
        }

    eol = 1;
    while (1) {
        ch = *(b->a + b->c);

        if (ch == '\n')
            eol = 1;
        else if (eol && (ch == ' ' || ch == '\t'))
            delete_ch(b);       /* Eat trailing whitespace */
        else if (!isprint(ch) && ch != '\t')
            delete_ch(b);
        else
            eol = 0;

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

    b->sc_set = 0;

    if (!b->m_set)
        return GEN_ERROR;

    delete_gb(p);

    if (b->m == b->c)
        return 0;

    if (b->m < b->c) {
        for (i = b->m; i < b->g; ++i)
            if (insert_ch(p, *(b->a + i)))
                return GEN_ERROR;

        if (cut) {
            num = b->g - b->m;
            while (num--)
                backspace_ch(b);
        }
    } else {
        for (i = b->c; i < b->m; ++i)
            if (insert_ch(p, *(b->a + i)))
                return GEN_ERROR;

        if (cut) {
            num = b->m - b->c;
            while (num--)
                delete_ch(b);
        }
    }

    /* Clear mark even when just copying */
    if (!cut)
        b->m_set = 0;

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
    p_stop = b->a + b->e;       /* Exclusive */
    delete_gb(tmp);

    if ((u = *p) == ' ' || u == '\t')
        return GEN_ERROR;       /* Invalid character */

    while ((u = *p) != ' ' && u != '\n' && u != '\t' && p != p_stop) {
        if (u && insert_ch(tmp, u))     /* Skip embedded \0 chars */
            return GEN_ERROR;

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
                        return GEN_ERROR;

                    if (left_ch(tmp))
                        return GEN_ERROR;
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
    while (b->col || (b->g >= 2 && *(b->a + b->g - 2) == '\\'))
        left_ch(b);

    b->m_set = 1;
    b->m = b->c;

    /* Move to end of logical line */
    while ((*(b->a + b->c) != '\n' || (b->g && *(b->a + b->g - 1) == '\\'))
           && b->c != b->e)
        right_ch(b);

    if (copy_region(b, tmp, 0))
        return GEN_ERROR;

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

    /* Open a new line */
    if (insert_ch(b, '\n'))
        return GEN_ERROR;

    if ((fp = popen(cmd, "r")) == NULL)
        return GEN_ERROR;

    while ((x = getc(fp)) != EOF) {
        if ((isprint(x) || x == '\t' || x == '\n') && insert_ch(b, x)) {
            pclose(fp);
            return GEN_ERROR;
        }
    }
    if (ferror(fp) || !feof(fp)) {
        pclose(fp);
        return GEN_ERROR;
    }
    if ((st = pclose(fp)) == -1)
        return GEN_ERROR;

#ifndef _WIN32
    if (!WIFEXITED(st))
        return GEN_ERROR;

    st = WEXITSTATUS(st);
#endif

    *es = st;

    return 0;
}

int shell_line(struct gb *b, struct gb *tmp, int *es)
{
    if (copy_logical_line(b, tmp))
        return GEN_ERROR;

    end_of_gb(tmp);
    if (insert_str(tmp, " 2>&1"))
        return GEN_ERROR;

    /* Embedded \0 will terminate string early */
    start_of_gb(tmp);

    if (insert_shell_cmd(b, (const char *) tmp->a + tmp->c, es))
        return GEN_ERROR;

    return 0;
}

int paste(struct gb *b, struct gb *p)
{
    size_t i;

    for (i = 0; i < p->g; ++i)
        if (insert_ch(b, *(p->a + i)))
            return GEN_ERROR;

    /* Cursor should be at end of gap buffer, but just in case */
    for (i = p->c; i < p->e; ++i)
        if (insert_ch(b, *(p->a + i)))
            return GEN_ERROR;

    return 0;
}

int save(struct gb *b)
{
    FILE *fp;

    b->sc_set = 0;

    if (b->fn == NULL || *b->fn == '\0')
        return GEN_ERROR;

    if ((fp = fopen_w(b->fn, 0)) == NULL)
        return GEN_ERROR;

    if (fwrite(b->a, 1, b->g, fp) != b->g) {
        fclose(fp);
        return GEN_ERROR;
    }
    if (fwrite(b->a + b->c, 1, b->e - b->c, fp) != b->e - b->c) {
        fclose(fp);
        return GEN_ERROR;
    }
    if (fclose(fp))
        return GEN_ERROR;

    b->mod = 0;

    return 0;
}

int rename_gb(struct gb *b, const char *fn)
{
    char *new_fn;

    b->sc_set = 0;

    if (fn == NULL)
        return GEN_ERROR;

    if ((new_fn = strdup(fn)) == NULL)
        return GEN_ERROR;

    free(b->fn);
    b->fn = new_fn;
    b->mod = 1;
    return 0;
}

int new_gb(struct gb **b, const char *fn, size_t s)
{
    struct gb *t = NULL;

    if ((t = init_gb(s)) == NULL)
        return GEN_ERROR;

    if (fn != NULL && *fn != '\0') {
        /* OK for file to not exist */
        if (insert_file(t, fn) == 1) {
            free_gb(t);
            return GEN_ERROR;
        }
        if (rename_gb(t, fn)) {
            free_gb(t);
            return GEN_ERROR;
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
            *b = (*b)->prev;    /* Move left by default */
        }
        free_gb(t);
    }
}
