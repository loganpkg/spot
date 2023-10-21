/*
 * Copyright (c) 2023 Logan Ryan McLintock
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

/* Gap buffer module */

#ifdef __linux__
/* For: strdup */
#define _XOPEN_SOURCE 500
#endif

#include <ctype.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "gb.h"
#include "gen.h"
#include "num.h"

struct gb *init_gb(size_t s)
{
    struct gb *b;

    if ((b = malloc(sizeof(struct gb))) == NULL)
        return NULL;

    b->fn = NULL;
    if ((b->a = malloc(s)) == NULL) {
        free(b);
        return NULL;
    }
    b->g = 0;
    b->c = s - 1;
    b->e = s - 1;

    /* Last char cannot be deleted. Enables use as a string. Do not change. */
    *(b->a + s - 1) = '\0';

    b->m_set = 0;
    b->m = 0;
    b->r = 1;
    b->col = 0;
    b->d = 0;
    b->mod = 0;
    b->prev = NULL;
    b->next = NULL;
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
    b->d = 0;
    b->mod = 1;
}

static int grow_gap(struct gb *b, size_t will_use)
{
    unsigned char *t;
    size_t s, new_s, increase;

    s = b->e + 1;               /* OK as in memory already */

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
    if (b->g == b->c && grow_gap(b, 1))
        return 1;

    *(b->a + b->g) = ch;
    ++b->g;
    if (ch == '\n') {
        ++b->r;
        b->col = 0;
    } else {
        ++b->col;
    }
    b->m_set = 0;
    b->m = 0;
    b->mod = 1;
    return 0;
}

int insert_str(struct gb *b, const char *str)
{
    char ch;

    while ((ch = *str++) != '\0')
        if (insert_ch(b, ch))
            return 1;

    return 0;
}

int insert_file(struct gb *b, const char *fn)
{
    int ret = 1;
    FILE *fp = NULL;
    long fs_l;
    size_t fs;

    errno = 0;
    if ((fp = fopen(fn, "rb")) == NULL) {
        if (errno == ENOENT)    /* File does not exist */
            return 2;
        else
            return 1;
    }

    if (fseek(fp, 0L, SEEK_END))
        goto clean_up;

    if ((fs_l = ftell(fp)) == -1 || fs_l < 0)
        goto clean_up;

    if (fseek(fp, 0L, SEEK_SET))
        goto clean_up;

    if (!fs_l)
        goto done;

    fs = (size_t) fs_l;

    if (fs > b->c - b->g && grow_gap(b, fs))
        goto clean_up;

    /* Right side of gap insert */
    if (fread(b->a + b->c - fs, 1, fs, fp) != fs)
        goto clean_up;

    b->c -= fs;
    b->m_set = 0;
    b->m = 0;
    b->mod = 1;

  done:
    ret = 0;
  clean_up:
    if (fp != NULL)
        if (fclose(fp))
            ret = 1;

    return ret;
}

int delete_ch(struct gb *b)
{
    if (b->c == b->e)
        return 1;

    ++b->c;
    b->m_set = 0;
    b->m = 0;
    b->mod = 1;
    return 0;
}

int left_ch(struct gb *b)
{
    size_t i, count;

    if (!b->g)
        return 1;

    --b->g;
    --b->c;
    *(b->a + b->c) = *(b->a + b->g);
    if (*(b->a + b->c) == '\n') {
        --b->r;
        /* Need to work out col */
        i = b->g;
        count = 0;
        while (i) {
            --i;
            if (*(b->a + i) == '\n')
                break;

            ++count;
        }
        b->col = count;
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
    if (b->c == b->e)
        return 1;

    if (*(b->a + b->c) == '\n') {
        ++b->r;
        b->col = 0;
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
        return 1;

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
    size_t r_orig = b->r, col_orig = b->col;

    /* Row number starts from 1, not 0 */
    if (b->r == 1)
        return 1;

    while (b->r == r_orig)
        left_ch(b);

    while (b->col > col_orig)
        left_ch(b);

    return 0;
}

int down_line(struct gb *b)
{
    size_t r_orig = b->r, col_orig = b->col;

    while (b->r == r_orig) {
        if (right_ch(b)) {
            /* Go back */
            while (b->col != col_orig)
                left_ch(b);

            return 1;
        }
    }

    while (b->col != col_orig && *(b->a + b->c) != '\n')
        if (right_ch(b))
            break;

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
            b->m = 0;
            b->mod = 1;
        } else if (islower(u) && transform == 'U') {
            *(b->a + b->c) = 'A' + u - 'a';
            b->m_set = 0;
            b->m = 0;
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
    unsigned char ch[2], x;
    size_t i;

    start_of_gb(cl);
    str = cl->a + cl->c;
    while ((ch[0] = *str) != '\0') {
        ++str;

        if ((ch[1] = *str) == '\0')
            return 1;

        ++str;

        x = 0;
        for (i = 0; i < 2; ++i) {
            if (i)
                x *= 16;
            if (!isxdigit(ch[i]))
                return 1;
            else if (isdigit(ch[i]))
                x += ch[i] - '0';
            else if (islower(ch[i]))
                x += ch[i] - 'a' + 10;
            else if (isupper(ch[i]))
                x += ch[i] - 'A' + 10;
        }
        if (insert_ch(b, x))
            return 1;
    }
    return 0;
}

int search_str(struct gb *b, const char *find, int skip_immediate)
{
    /* Embedded \0 chars will terminate strings early */
    const char *q;
    size_t num;

    if (b->c == b->e)
        return 1;

    if ((q =
         strstr((const char *) b->a + b->c + (skip_immediate ? 1 : 0),
                find)) == NULL)
        return 1;

    num = q - ((char *) b->a + b->c);
    while (num--)
        right_ch(b);

    return 0;
}

int search(struct gb *b, struct gb *cl)
{
    start_of_gb(cl);
    return search_str(b, (const char *) cl->a + cl->c, 1);
}

int replace_region(struct gb *b, struct gb *cl)
{
    /* Embedded \0 chars in b will terminate the string early (but not cl) */
    unsigned char u, *find, *replace;
    int escape_mode = 0, in_find = 1;
    size_t count, find_len = 0, replace_len, region_size, g_start, n, i;

    if (!b->m_set)
        return 1;

    start_of_gb(cl);
    count = 0;
    while (1) {
        if (cl->c == cl->e)
            break;

        u = *(cl->a + cl->c);

        if (u == '\0' || (u == '\\' && !escape_mode)) {
            if (u == '\\')
                escape_mode = 1;

            delete_ch(cl);
        } else if (u == '|' && !escape_mode && in_find) {
            /* Separator between find and replace components */
            *(cl->a + cl->c) = '\0';
            find_len = count;
            in_find = 0;
            count = 0;
            right_ch(cl);
        } else {
            if (escape_mode) {
                switch (u) {
                case 'n':
                    *(cl->a + cl->c) = '\n';
                    break;
                case 't':
                    *(cl->a + cl->c) = '\t';
                    break;
                case '\\':
                case '|':
                    break;      /* Literal */
                default:
                    return 1;   /* Invalid escape sequence */
                }
                escape_mode = 0;
            }
            ++count;
            right_ch(cl);
        }
    }

    if (!find_len || escape_mode)
        return 1;

    replace_len = count;

    start_of_gb(cl);
    find = cl->a + cl->c;
    replace = cl->a + cl->e - replace_len;

    /* Move cursor to start of region */
    if (b->c > b->m) {
        region_size = b->g - b->m;
        while (b->c != b->m)
            left_ch(b);
    } else {
        region_size = b->m - b->c;
    }

    g_start = b->g;

    while (!search_str(b, (const char *) find, 0)) {
        if (b->g >= g_start + region_size) {
            /* Out of original region */
            while (b->g != g_start + region_size)
                left_ch(b);     /* Go back */

            break;
        }

        n = find_len;
        while (n--)
            delete_ch(b);

        region_size -= find_len;

        for (i = 0; i < replace_len; ++i)
            if (insert_ch(b, *(replace + i)))
                return 1;

        region_size += replace_len;
    }

    return 0;
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
        while (b->c != c_orig)
            left_ch(b);
    else
        while (b->c != c_orig)
            right_ch(b);

    return 1;
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

    if (!b->m_set)
        return 1;

    delete_gb(p);

    if (b->m == b->c)
        return 0;

    if (b->m < b->c) {
        for (i = b->m; i < b->g; ++i)
            if (insert_ch(p, *(b->a + i)))
                return 1;

        if (cut) {
            num = b->g - b->m;
            while (num--)
                backspace_ch(b);
        }
    } else {
        for (i = b->c; i < b->m; ++i)
            if (insert_ch(p, *(b->a + i)))
                return 1;

        if (cut) {
            num = b->m - b->c;
            while (num--)
                delete_ch(b);
        }
    }

    /* Clear mark even when just copying */
    if (!cut) {
        b->m_set = 0;
        b->m = 0;
    }

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

int copy_logical_line(struct gb *b, struct gb *p)
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

    if (copy_region(b, p, 0))
        return 1;

    /* Delete backslash at the end of lines and combine lines */
    start_of_gb(p);
    while (p->c != p->e) {
        switch (*(p->a + p->c)) {
        case '\\':
            if (p->c + 1 == p->e || *(p->a + p->c + 1) == '\n') {
                delete_ch(p);   /* Remove backslash */
            } else {
                right_ch(p);
            }
            break;
        case '\n':
            delete_ch(p);
            break;
        default:
            right_ch(p);
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

    return 0;
}

int shell_line(struct gb *b, struct gb *p, int *es)
{
    if (copy_logical_line(b, p))
        return 1;

    end_of_gb(p);
    if (insert_str(p, " 2>&1"))
        return 1;

    /* Embedded \0 will terminate string early */
    start_of_gb(p);

    if (insert_shell_cmd(b, (const char *) p->a + p->c, es))
        return 1;

    return 0;
}

int paste(struct gb *b, struct gb *p)
{
    size_t i;

    for (i = 0; i < p->g; ++i)
        if (insert_ch(b, *(p->a + i)))
            return 1;

    /* Cursor should be at end of gap buffer, but just in case */
    for (i = p->c; i < p->e; ++i)
        if (insert_ch(b, *(p->a + i)))
            return 1;

    return 0;
}

int save(struct gb *b)
{
    FILE *fp;

    if (b->fn == NULL || *b->fn == '\0')
        return 1;

    if ((fp = fopen(b->fn, "wb")) == NULL)
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
    char *fn_copy;

    if ((fn_copy = strdup(fn)) == NULL)
        return 1;

    free(b->fn);
    b->fn = fn_copy;
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
            *b = (*b)->prev;    /* Move left by default */
        }
        free_gb(t);
    }
}
