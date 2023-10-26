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

/* Regular expression module */

#include <ctype.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "debug.h"
#include "num.h"

#define CONCAT_CH '.'

#define set_cs(cs, i, u)   *((cs) + (i) * 32 + (u) / 8) |=   1 << (u) % 8
#define clear_cs(cs, i, u) *((cs) + (i) * 32 + (u) / 8) &= ~(1 << (u) % 8)

#define set_all_cs(cs, i) memset((cs) + (i) * 32, '\xFF', 32)


#define is_set_cs(cs, id, u) *((cs) + ((id) - (UCHAR_MAX + 1)) * 32 + (u) / 8) & 1 << (u) % 8


static int preprocess_regex(const char *regex_str,
                            unsigned char **char_sets, size_t **regex_nums,
                            size_t *rn_len)
{
    int ret = 1;
    size_t len, n, i, k;
    unsigned char *cs = NULL;
    size_t *rn = NULL;
    const unsigned char *p;
    int negate_set, j, add_concat;

    if (regex_str == NULL)
        mgoto(clean_up);

    len = strlen(regex_str);

    if (aof(len, UCHAR_MAX + 1, SIZE_MAX))
        mgoto(clean_up);

    if (mof(len, 32, SIZE_MAX))
        mgoto(clean_up);

    if ((cs = calloc(len * 32, 1)) == NULL)
        mgoto(clean_up);

    if (mof(len, sizeof(size_t), SIZE_MAX))
        mgoto(clean_up);

    /* Double to allow for added concat operators */
    if (mof(len, 2, SIZE_MAX))
        mgoto(clean_up);

    n = len * 2;

    if (mof(n, sizeof(size_t), SIZE_MAX))
        mgoto(clean_up);

    if ((rn = calloc(n, sizeof(size_t))) == NULL)
        mgoto(clean_up);

    p = (const unsigned char *) regex_str;
    i = 0;
    k = 0;
    add_concat = 0;
    while (*p != '\0') {
        if (*p == '\\') {
            ++p;                /* Eat backslash */
            if (*p == '\0')
                mgoto(clean_up);

            set_cs(cs, i, *p);

            if (add_concat)
                rn[k++] = CONCAT_CH;

            /* Record and move to next char set */
            rn[k++] = UCHAR_MAX + 1 + i++;
            add_concat = 1;
        } else if (*p == '[') {
            /* Gather set -- No escape sequences */
            ++p;                /* Eat opening square bracket */

            if (*p == '^') {
                negate_set = 1;
                ++p;            /* Eat caret */
            } else {
                negate_set = 0;
            }

            /* Take any char literally in this position */
            set_cs(cs, i, *p);
            ++p;                /* Eat first non-negation char */

            while (*p != ']') {
                if (*p == '-' && *(p + 1) != ']') {
                    /* Range */
                    for (j = *(p - 1); j <= *(p + 1); ++j)
                        set_cs(cs, i, j);

                    ++p;        /* Eat range separator */
                } else {
                    set_cs(cs, i, *p);
                }
                ++p;            /* Advance */
            }

            if (negate_set)
                for (j = 0; j < 32; ++j)
                    *(cs + i * 32 + j) = ~*(cs + i * 32 + j);   /* Flip bits */

            if (add_concat)
                rn[k++] = CONCAT_CH;

            rn[k++] = UCHAR_MAX + 1 + i++;
            add_concat = 1;
        } else {
            switch (*p) {
                /* Special characters outside of char sets */
            case '*':
            case '+':
            case '?':
                rn[k++] = *p;
                add_concat = 1;
                break;
            case '^':
            case '$':
                /*
                 * ^ and $ are not operators, they are transition criteria
                 * based on the read status.
                 */
                if (add_concat)
                    rn[k++] = CONCAT_CH;

                rn[k++] = *p;
                add_concat = 1;
                break;
            case '(':
                if (add_concat)
                    rn[k++] = CONCAT_CH;

                rn[k++] = *p;
                add_concat = 0;
                break;
            case ')':
                rn[k++] = *p;
                add_concat = 1;
                break;
            case '|':
                rn[k++] = *p;
                add_concat = 0;
                break;
            case '.':
                /* Any char except \n */
                set_all_cs(cs, i);
                clear_cs(cs, i, '\n');
                if (add_concat)
                    rn[k++] = CONCAT_CH;

                rn[k++] = UCHAR_MAX + 1 + i++;
                add_concat = 1;
                break;
            default:
                set_cs(cs, i, *p);
                if (add_concat)
                    rn[k++] = CONCAT_CH;

                rn[k++] = UCHAR_MAX + 1 + i++;
                add_concat = 1;
                break;
            };
        }
        ++p;                    /* Advance */
    }

    ret = 0;
    *char_sets = cs;
    *regex_nums = rn;
    *rn_len = k;
  clean_up:
    if (ret)
        free(cs);

    mreturn(0);
}


static int shunting_yard_regex(size_t *regex_nums, size_t rn_len,
                               size_t **regex_postfix, size_t *rp_len)
{
    int ret = 1;
    size_t *rp = NULL;
    unsigned char *op_stack = NULL, h;
    size_t op_i;
    size_t i, k, x;

    if (mof(rn_len, sizeof(size_t), SIZE_MAX))
        mgoto(clean_up);

    if ((rp = calloc(rn_len, sizeof(size_t))) == NULL)
        mgoto(clean_up);

    if ((op_stack = malloc(rn_len)) == NULL)
        mgoto(clean_up);

    op_i = 0;

    k = 0;
    for (i = 0; i < rn_len; ++i) {
        x = regex_nums[i];
        if (x > UCHAR_MAX || x == '^' || x == '$') {
            /* Operand -- char set id or read status */
            rp[k++] = x;
        } else {
            /* Operator */
            switch (x) {
            case '(':
                op_stack[op_i++] = x;
                break;
            case ')':
                while (1) {
                    if (!op_i)  /* Open bracket not found */
                        mgoto(clean_up);

                    h = op_stack[op_i - 1];
                    if (h == '(') {
                        --op_i;
                        break;
                    }
                    rp[k++] = h;
                    --op_i;
                }
                break;
            case '*':
            case '+':
            case '?':
                while (op_i) {
                    h = op_stack[op_i - 1];
                    if (h == '(' || h == '.' || h == '|')
                        break;

                    rp[k++] = h;
                    --op_i;
                }
                op_stack[op_i++] = x;
                break;
            case '.':
                /* Concat operator */
                while (op_i) {
                    h = op_stack[op_i - 1];
                    if (h == '(' || h == '|')
                        break;

                    rp[k++] = h;
                    --op_i;
                }
                op_stack[op_i++] = x;
                break;
            case '|':
                while (op_i) {
                    h = op_stack[op_i - 1];
                    if (h == '(')
                        break;

                    rp[k++] = h;
                    --op_i;
                }
                op_stack[op_i++] = x;
                break;
            default:
                /* Invalid operator */
                mgoto(clean_up);
            }
        }
    }

    /* Finish operator stack */
    while (op_i) {
        h = op_stack[op_i - 1];
        /* Should not be any unmatched brackets left */
        if (h == '(')
            mgoto(clean_up);

        rp[k++] = h;
        --op_i;
    }

    ret = 0;
    *regex_postfix = rp;
    *rp_len = k;
  clean_up:
    if (ret)
        free(rp);

    free(op_stack);
    mreturn(ret);
}

static void print_regex(unsigned char *char_sets,
                        size_t *regex_nums, size_t rn_len)
{
    size_t k, x;
    int j;

    printf("\nRegex:\n");

    for (k = 0; k < rn_len; ++k) {
        x = regex_nums[k];
        if (x == '^' || x == '$') {
            /* Read status */
            printf("R status: %c\n", (unsigned char) x);
        } else if (x <= UCHAR_MAX) {
            printf("Operator: %c\n", (unsigned char) x);
        } else {
            printf("Char set: ");
            for (j = 0; j <= UCHAR_MAX; ++j)
                if (is_set_cs(char_sets, x, j))
                    isgraph(j) ? printf("%c ", j) : printf("%02X ", j);

            putchar('\n');
        }
    }
}

/* Node in the NFA */
struct state {
    /* State type: 'S' start, 'E' end, '_' in the middle */
    unsigned char st;
    /*
     * Transition criteria for branches a and b, respectively.
     * 0 Unused (no link).
     * 'e' Epsilon. Instantaneous transition
     *         (can occur without reading any input).
     * '^' Start of line anchor (read status).
     * '$' End of line anchor (read status).
     * > UCHAR_MAX   char set id.
     */
    size_t t_a;
    size_t t_b;
    struct state *a;            /* Branch a */
    struct state *b;            /* Branch b */
};

struct nfa {
    struct state *start;
    struct state *end;
};

struct state *init_state(void)
{
    struct state *s;

    if ((s = malloc(sizeof(struct state))) == NULL)
        mreturn(NULL);

    s->st = '_';
    s->t_a = 0;
    s->t_b = 0;
    s->a = NULL;
    s->b = NULL;

    mreturn(s);
}

static int generate_nfa(size_t *regex_postfix, size_t rp_len)
{

    return 0;
}

int main(void)
{
    int ret = 1;
    const char *p = "^a+b*$";
    unsigned char *cs = NULL;
    size_t *rn = NULL, rn_len, *rp = NULL, rp_len;

    printf("%s\n", p);

    if (preprocess_regex(p, &cs, &rn, &rn_len))
        return 1;

    print_regex(cs, rn, rn_len);


    if (shunting_yard_regex(rn, rn_len, &rp, &rp_len))
        mgoto(clean_up);


    print_regex(cs, rp, rp_len);


    ret = 0;
  clean_up:

    free(cs);
    free(rn);

    mreturn(ret);
}
