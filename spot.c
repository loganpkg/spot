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


#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <conio.h>
#include <io.h>
#include <fcntl.h>
#include <Windows.h>
#else
#define _BSD_SOURCE
#define _DEFAULT_SOURCE
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>
#endif

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#define popen _popen
#define pclose _pclose
#endif

#define BLOCK_SIZE 512
#define NUM_BUF_SIZE 32

#define LEFT_KEY  (UCHAR_MAX + 1)
#define RIGHT_KEY (UCHAR_MAX + 2)
#define UP_KEY    (UCHAR_MAX + 3)
#define DOWN_KEY  (UCHAR_MAX + 4)
#define DEL_KEY   (UCHAR_MAX + 5)
#define HOME_KEY  (UCHAR_MAX + 6)
#define END_KEY   (UCHAR_MAX + 7)

#define ESC 27

#define C(l) ((l) - 'a' + 1)

/* Overflow tests for size_t */
/* Addition */
#define aof(a, b) ((a) > SIZE_MAX - (b))

/* Multiplication */
#define mof(a, b) ((a) && (b) > SIZE_MAX / (a))


#define phy_move(pos) printf("\x1B[%lu;%luH", \
    (unsigned long) ((pos) / s->w + 1),       \
    (unsigned long) ((pos) % s->w + 1))

#define phy_hl_off() printf("\x1B[m")

/* Does not toggle */
#define phy_hl_on() printf("\x1B[7m")

#define phy_clear() printf("\x1B[2J\x1B[1;1H")

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

struct screen {
#ifdef _WIN32
    HANDLE term_handle;
#endif
    size_t h;                   /* Physical screen heigth */
    size_t w;                   /* Physical screen width */
    unsigned char *vs_c;        /* Current virtual screen */
    unsigned char *vs_n;        /* Next virtual screen */
    size_t vs_s;                /* Size of each virtual screen */
    int clear;                  /* Clear physical screen */
    int centre;                 /* Draw cursor on the centre row */
};


struct gb *init_gb(void)
{
    struct gb *b;

    if ((b = malloc(sizeof(struct gb))) == NULL)
        return NULL;

    b->fn = NULL;
    if ((b->a = malloc(BLOCK_SIZE)) == NULL) {
        free(b);
        return NULL;
    }
    b->g = 0;
    b->c = BLOCK_SIZE - 1;
    b->e = BLOCK_SIZE - 1;

    /* Last char cannot be deleted. Enables use as a string. Do not change. */
    *(b->a + BLOCK_SIZE - 1) = '\0';

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

int grow_gap(struct gb *b, size_t will_use)
{
    unsigned char *t;
    size_t s, new_s, num, increase;

    s = b->e + 1;               /* OK as in memory already */

    if (aof(s, will_use))
        return 1;

    new_s = s + will_use;

    if (mof(new_s, 2))
        return 1;

    new_s *= 2;

    num = new_s / BLOCK_SIZE;

    if (aof(num, 1))
        return 1;

    ++num;

    if (mof(num, BLOCK_SIZE))
        return 1;

    new_s = num * BLOCK_SIZE;

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

int str_to_size_t(const unsigned char *str, size_t *res)
{
    unsigned char ch;
    size_t x = 0;

    if (str == NULL || *str == '\0')
        return 1;

    while ((ch = *str) != '\0') {
        if (isdigit(ch)) {
            if (mof(x, 10))
                return 1;

            x *= 10;
            if (aof(x, ch - '0'))
                return 1;

            x += ch - '0';
        } else {
            return 1;
        }

        ++str;
    }
    *res = x;
    return 0;
}

int goto_row(struct gb *b, struct gb *cl)
{
    size_t x;

    start_of_gb(cl);
    if (str_to_size_t(cl->a + cl->c, &x))
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

int new_gb(struct gb **b, const char *fn)
{
    struct gb *t = NULL;

    if ((t = init_gb()) == NULL)
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

int get_key(void)
{
    int x, y;

#ifdef _WIN32
    while (1) {
        x = _getch();
        if (x == 0) {
            y = _getch();
            if (y == 3)
                return 0;       /* Ctrl 2 */
        } else if (x == 224) {
            y = _getch();
            switch (y) {
            case 'K':
                return LEFT_KEY;
            case 'M':
                return RIGHT_KEY;
            case 'H':
                return UP_KEY;
            case 'P':
                return DOWN_KEY;
            case 'S':
                return DEL_KEY;
            case 'G':
                return HOME_KEY;
            case 'O':
                return END_KEY;
            }
        } else {
            return x;
        }
    }

#else

    int z, w;

    while (1) {
        x = getchar();
        if (x == ESC) {
            y = getchar();
            if (y == '[') {
                z = getchar();
                if (!isdigit(z)) {
                    switch (z) {
                    case 'D':
                        return LEFT_KEY;
                    case 'C':
                        return RIGHT_KEY;
                    case 'A':
                        return UP_KEY;
                    case 'B':
                        return DOWN_KEY;
                    case 'H':
                        return HOME_KEY;
                    case 'F':
                        return END_KEY;
                    }
                } else {
                    if (z == '3' && getchar() == '~') {
                        return DEL_KEY;
                    } else {
                        /* Eat to end */
                        while ((w = getchar()) != '~'
                               && (isdigit(w) || w == ';'));
                    }
                }
            } else if (y == 'O') {
                getchar();      /* Eat */
            } else {
                if (ungetc(y, stdin) == EOF)
                    return EOF;

                return ESC;
            }
        } else {
            return x;
        }
    }

#endif
}

int get_screen_size(struct screen *s)
{
#ifdef _WIN32
    CONSOLE_SCREEN_BUFFER_INFO si;

    if (!GetConsoleScreenBufferInfo(s->term_handle, &si))
        return 1;

    s->h = si.srWindow.Bottom - si.srWindow.Top + 1;
    s->w = si.srWindow.Right - si.srWindow.Left + 1;
#else
    struct winsize ws;

    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1)
        return 1;

    s->h = ws.ws_row;
    s->w = ws.ws_col;
#endif
    return 0;
}

#define print_ch(b) do {                                  \
    ch = *((b)->a + i);                                   \
    if (ch == '\n') {                                     \
        *(s->vs_n + j) = v_hl ? ' ' | '\x80' : ' ';       \
        ++j;                                              \
        if (j % s->w)                                     \
            j = (j / s->w + 1) * s->w;                    \
    } else {                                              \
        if (ch == '\0')                                   \
            new_ch = '~';                                 \
        else if (ch == '\t')                              \
            new_ch = '>';                                 \
        else if (!isprint(ch))                            \
            new_ch = '?';                                 \
        else                                              \
            new_ch = ch;                                  \
                                                          \
        *(s->vs_n + j) = v_hl ? new_ch | '\x80' : new_ch; \
        ++j;                                              \
    }                                                     \
    ++i;                                                  \
} while (0)

int draw(struct gb *b, struct gb *cl, struct screen *s, int cl_active,
         int rv, int es_set, int es)
{
    size_t new_s_s, up, target_up, ts, i, j, cursor_pos, k;
    int v_hl, have_centred = 0;
    unsigned char *t, ch, new_ch, *sb;
    char num_str[NUM_BUF_SIZE];
    int r, len;

    if (get_screen_size(s))
        return 1;

    if (mof(s->h, s->w))
        return 1;

    new_s_s = s->h * s->w;      /* New screen size */

    if (!new_s_s)
        return 0;

  start:

    if (s->clear || new_s_s != s->vs_s) {
        if (new_s_s != s->vs_s) {
            if ((t = realloc(s->vs_c, new_s_s)) == NULL)
                return 1;

            s->vs_c = t;
            if ((t = realloc(s->vs_n, new_s_s)) == NULL)
                return 1;

            s->vs_n = t;
            s->vs_s = new_s_s;
        }
        memset(s->vs_c, ' ', s->vs_s);
        phy_hl_off();
        phy_clear();
        s->clear = 0;
    }
    memset(s->vs_n, ' ', s->vs_s);


    if (b->d > b->g || s->centre) {
        /* Does not consider long lines that wrap */
        b->d = b->g;
        up = 0;
        target_up = s->h <= 4 ? 1 : (s->h - 1) / 2;
        while (b->d) {
            --b->d;
            if (*(b->a + b->d) == '\n')
                ++up;

            if (up == target_up) {
                ++b->d;
                break;
            }
        }
        s->centre = 0;
        have_centred = 1;
    }

    /* Size of the text portion of the screen */
    ts = s->vs_s - (s->h >= 2 ? s->w : 0) - (s->h >= 3 ? s->w : 0);

    v_hl = 0;

    /* Region commenced before draw start */
    if (b->m_set && b->m < b->d)
        v_hl = 1;

    /* Before the gap */
    i = b->d;
    j = 0;
    while (i < b->g && j < ts) {
        if (b->m_set && b->m == i)
            v_hl = 1;

        print_ch(b);
    }

    if (j == ts) {
        /* Off screen */
        if (!have_centred)
            s->centre = 1;
        else
            b->d = b->g;
        goto start;
    }

    /* At the cursor */
    cursor_pos = j;
    if (b->m_set) {
        if (b->m < b->c)
            v_hl = 0;           /* End of region */
        else
            v_hl = 1;           /* Start of region */
    }

    /* After the gap (from the cursor onwards) */
    i = b->c;
    while (i <= b->e && j < ts) {
        if (b->m_set && b->m == i)
            v_hl = 0;

        print_ch(b);
    }

    if (s->h >= 2) {
        /* Status bar */

        if (es_set) {
            r = snprintf(num_str, NUM_BUF_SIZE, "%d", es);
            if (r < 0 || r >= NUM_BUF_SIZE)
                return 1;
        } else {
            *num_str = '\0';
        }

        sb = s->vs_n + ((s->h - 2) * s->w);
        len = snprintf((char *) sb, s->w, "%c%c %s (%lu,%lu) %02X %s",
                       rv ? '!' : ' ', b->mod ? '*' : ' ', b->fn,
                       (unsigned long) b->r, (unsigned long) b->col,
                       cl_active ? *(cl->a + cl->c) : *(b->a + b->c),
                       num_str);
        if (len < 0)
            return 1;

        /* Overwrite \0 char */
        if ((size_t) len >= s->w)       /* Truncated */
            *(sb + s->w - 1) = ' ';
        else
            *(sb + len) = ' ';

        /* Virtually highlight */
        for (k = 0; k < s->w; ++k)
            *(sb + k) |= '\x80';
    }

    if (s->h >= 3) {
        /* Command line */
        if (cl->d > cl->g || cl->g - cl->d > cl->col
            || cl->g - cl->d >= s->w) {
            if (cl->col < s->w)
                cl->d = cl->g - cl->col;
            else
                cl->d = cl->g;
        }

        v_hl = 0;

        if (cl->m_set && cl->m < cl->d)
            v_hl = 1;

        /* Start of last line in virtual screen */
        j = (s->h - 1) * s->w;

        /* Before the gap */
        i = cl->d;
        while (i < cl->g && j < s->vs_s) {
            if (cl->m_set && cl->m == i)
                v_hl = 1;

            print_ch(cl);
        }

        /* At the cursor */
        if (cl_active)
            cursor_pos = j;

        if (cl->m_set) {
            if (cl->m < cl->c)
                v_hl = 0;       /* End of region */
            else
                v_hl = 1;       /* Start of region */
        }

        /* After the gap (from the cursor onwards) */
        i = cl->c;
        while (i <= cl->e && j < s->vs_s) {
            if (cl->m_set && cl->m == i)
                v_hl = 0;

            print_ch(cl);
        }
    }

    /* Diff */
    for (k = 0; k < s->vs_s; ++k) {
        if ((ch = *(s->vs_n + k)) != *(s->vs_c + k)) {
            phy_move(k);
            if (ch & '\x80')
                phy_hl_on();
            else
                phy_hl_off();

            putchar(ch & '\x7F');
        }
    }
    phy_move(cursor_pos);

    /* Swap */
    t = s->vs_c;
    s->vs_c = s->vs_n;
    s->vs_n = t;
    return 0;
}

#undef print_ch

#define z (cl_active ? cl : b)

int main(int argc, char **argv)
{
    int ret = 0;                /* Return value of the text editor */
    int running = 1;            /* Indicates if the text editor is running */
    int rv = 0;                 /* Return value of last internal command */
    int es_set = 0;
    int es = 0;                 /* Exit status of last shell command */
    int i, x, y;
    struct screen s;
    struct gb *b = NULL;        /* Text buffers linked together */
    struct gb *p = NULL;        /* Paste buffer */
    struct gb *cl = NULL;       /* Command line buffer */
    int cl_active = 0;          /* Cursor is in the command line */
    char op = ' ';              /* The cl operation which is in progress */

#ifdef _WIN32
    DWORD term_orig, term_new;
#else
    struct termios term_orig;
    struct termios term_new;
#endif

    /* Init some screen variables */
    s.vs_c = NULL;
    s.vs_n = NULL;
    s.vs_s = 0;
    s.clear = 1;
    s.centre = 0;

/* Setup terminal */

#ifdef _WIN32
    if (_setmode(_fileno(stdin), _O_BINARY) == -1)
        return 1;

    if (_setmode(_fileno(stdout), _O_BINARY) == -1)
        return 1;

    if (_setmode(_fileno(stderr), _O_BINARY) == -1)
        return 1;

    if ((s.term_handle =
         GetStdHandle(STD_OUTPUT_HANDLE)) == INVALID_HANDLE_VALUE)
        return 1;

    if (!GetConsoleMode(s.term_handle, &term_orig))
        return 1;

    term_new =
        term_orig | ENABLE_PROCESSED_OUTPUT |
        ENABLE_VIRTUAL_TERMINAL_PROCESSING;

    if (!SetConsoleMode(s.term_handle, term_new))
        return 1;

#else

    if (tcgetattr(STDIN_FILENO, &term_orig))
        return 1;

    term_new = term_orig;

    cfmakeraw(&term_new);

    if (tcsetattr(STDIN_FILENO, TCSANOW, &term_new))
        return 1;

#endif

    if (argc > 1) {
        for (i = 1; i < argc; ++i) {
            if (new_gb(&b, *(argv + i)))
                goto clean_up;
        }
        while (b->prev)
            b = b->prev;
    } else {
        if (new_gb(&b, NULL))
            goto clean_up;
    }

    if ((p = init_gb()) == NULL)
        goto clean_up;

    if ((cl = init_gb()) == NULL)
        goto clean_up;


    while (running) {
        if (draw(b, cl, &s, cl_active, rv, es_set, es))
            goto clean_up;

        rv = 0;
        es_set = 0;
        es = 0;

        x = get_key();

        switch (x) {
        case C('b'):
        case LEFT_KEY:
            rv = left_ch(z);
            break;
        case C('f'):
        case RIGHT_KEY:
            rv = right_ch(z);
            break;
        case C('p'):
        case UP_KEY:
            rv = up_line(z);
            break;
        case C('n'):
        case DOWN_KEY:
            rv = down_line(z);
            break;
        case C('d'):
        case DEL_KEY:
            rv = delete_ch(z);
            break;
        case C('h'):
        case 127:
            rv = backspace_ch(z);
            break;
        case C('a'):
        case HOME_KEY:
            start_of_line(z);
            break;
        case C('e'):
        case END_KEY:
            end_of_line(z);
            break;
        case 0:
            z->m_set = 1;
            z->m = z->c;
            break;
        case C('g'):
            if (z->m_set) {
                z->m_set = 0;
                z->m = 0;
            } else if (cl_active) {
                cl_active = 0;
            }
            break;
        case C('l'):
            s.centre = 1;
            s.clear = 1;
            break;
        case C('w'):
            rv = copy_region(z, p, 1);
            break;
        case C('y'):
            rv = paste(z, p);
            break;
        case C('k'):
            rv = cut_to_eol(z, p);
            break;
        case C('o'):
            rv = shell_line(z, p, &es);
            if (!rv)
                es_set = 1;

            break;
        case C('t'):
            trim_clean(z);
            break;
        case C('s'):
            delete_gb(cl);
            cl_active = 1;
            op = 's';           /* search */
            break;
        case C('r'):
            delete_gb(cl);
            cl_active = 1;
            op = 'R';           /* replace_region */
            break;
        case C('u'):
            delete_gb(cl);
            cl_active = 1;
            op = 'g';           /* goto_row */
            break;
        case C('q'):
            delete_gb(cl);
            cl_active = 1;
            op = 'h';           /* insert_hex */
            break;
        case ESC:
            y = get_key();
            switch (y) {
            case 'b':
                left_word(z);
                break;
            case 'f':
                right_word(z, ' ');
                break;
            case 'l':
                /* Lowercase word */
                right_word(z, 'L');
                break;
            case 'u':
                /* Uppercase word */
                right_word(z, 'U');
                break;
            case 'k':
                rv = cut_to_sol(z, p);
                break;
            case 'm':
                rv = match_bracket(z);
                break;
            case 'n':
                rv = search(b, cl);
                break;
            case 'w':
                rv = copy_region(z, p, 0);
                break;
            case '!':
                remove_gb(&b);
                if (b == NULL)
                    running = 0;

                break;
            case '=':
                delete_gb(cl);
                cl_active = 1;
                op = 'r';       /* rename_gb */
                break;
            case '$':
                delete_gb(cl);
                cl_active = 1;
                op = '$';       /* insert_shell_cmd */
                break;
            case '<':
                start_of_gb(z);
                break;
            case '>':
                end_of_gb(z);
                break;
            }
            break;
        case C('x'):
            y = get_key();
            switch (y) {
            case C('c'):
                running = 0;
                break;
            case C('s'):
                rv = save(b);   /* Cannot save the command line */
                break;
            case C('f'):
                delete_gb(cl);
                cl_active = 1;
                op = 'n';       /* new_gb */
                break;
            case 'i':
                delete_gb(cl);
                cl_active = 1;
                op = 'i';       /* insert_file */
                break;
            case LEFT_KEY:
                if (b->prev != NULL)
                    b = b->prev;

                break;
            case RIGHT_KEY:
                if (b->next != NULL)
                    b = b->next;

                break;
            }
            break;
        case '\r':
        case '\n':
            if (cl_active) {
                switch (op) {
                case 's':
                    rv = search(b, cl);
                    break;
                case 'R':
                    rv = replace_region(b, cl);
                    break;
                case 'r':
                    start_of_gb(cl);
                    rv = rename_gb(b, (const char *) cl->a + cl->c);
                    break;
                case 'g':
                    rv = goto_row(b, cl);
                    break;
                case 'h':
                    rv = insert_hex(b, cl);
                    break;
                case 'n':
                    start_of_gb(cl);
                    rv = new_gb(&b, (const char *) cl->a + cl->c);
                    break;
                case 'i':
                    start_of_gb(cl);
                    rv = insert_file(b, (const char *) cl->a + cl->c);
                    break;
                case '$':
                    start_of_gb(cl);
                    rv = insert_shell_cmd(b, (const char *) cl->a + cl->c,
                                          &es);
                    if (!rv)
                        es_set = 1;

                    break;
                }
                cl_active = 0;
                op = ' ';
            } else {
                rv = insert_ch(z, '\n');
            }
            break;
        default:
            if (isprint(x) || x == '\t')
                rv = insert_ch(z, x);

            break;
        }
    }


  clean_up:
    phy_hl_off();
    phy_clear();
#ifdef _WIN32
    if (!SetConsoleMode(s.term_handle, term_orig))
        ret = 1;
#else
    if (tcsetattr(STDIN_FILENO, TCSANOW, &term_orig))
        ret = 1;
#endif

    free(s.vs_c);
    free(s.vs_n);

    free_gb_list(b);
    free_gb(p);
    free_gb(cl);

    return ret;
}

#undef z
