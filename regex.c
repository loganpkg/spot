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

/*
 * Regular expression module.
 *
 * "And now I give you a new commandment: love one another. As I have loved
 * you, so you must love one another. If you have love for one another, then
 * everyone will know that you are my disciples."
 *                                      John 13:34-35 GNT
 */

#include <ctype.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "regex.h"
#include "debug.h"
#include "num.h"
#include "buf.h"


#define NO_MATCH 2
#define CONCAT_CH '.'

#define set_cs(cs, i, u)   *((cs) + (i) * 32 + (u) / 8) |=   1 << (u) % 8
#define clear_cs(cs, i, u) *((cs) + (i) * 32 + (u) / 8) &= ~(1 << (u) % 8)

#define set_all_cs(cs, i) memset((cs) + (i) * 32, '\xFF', 32)

#define is_set_cs(cs, id, u) (*((cs) + ((id) - (UCHAR_MAX + 1)) * 32 \
                                          + (u) / 8) & 1 << (u) % 8)


/* Node in the NFA */
struct state {
    /*
     * Transition criteria for branches a and b, respectively.
     * 0 Unused (no link).
     * 'e' Epsilon. Instantaneous transition
     *         (can occur without reading any input).
     * '^' Start of line anchor (read status).
     * '$' End of line anchor (read status).
     * > UCHAR_MAX   Char set id.
     */
    size_t t_a;
    unsigned char t_b;          /* Will only ever be epsilon */
    size_t a;                   /* Index to branch a state */
    size_t b;                   /* Index to branch b state */
};

struct nfa {
    size_t start;               /* Index to start state */
    size_t end;                 /* Index to end state */
};

struct regex_info {
    unsigned char *cs;          /* Character sets */
    struct nfa sm;              /* State machine */
    struct state *sa;           /* State array */
    size_t sa_len;              /* State array length */
    unsigned char *sl;          /* (Active) state list */
    unsigned char *sl_next;     /* Next (active) state list */
    /*
     * List of states that have been transitioned from during a no-read
     * operation. Used to detect recursive loops.
     */
    unsigned char *sl_from;
};


static int preprocess_regex(const char *regex_str, int nl_sen,
                            unsigned char **char_sets, size_t **regex_nums,
                            size_t *rn_len)
{
    int ret = 1;
    size_t len, n, i, k;
    unsigned char *cs = NULL;
    size_t *rn = NULL;
    const unsigned char *p;
    unsigned char u, h[2];
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
            /* Eat backslash */
            if ((u = *++p) == '\0')
                mgoto(clean_up);

            switch (u) {
            case 't':
                u = '\t';
                break;
            case 'n':
                u = '\n';
                break;
            case 'r':
                u = '\r';
                break;
            case '0':
                u = '\0';
                break;
            case 'x':
                /* Two digit hex literal */
                if ((h[0] = *++p) == '\0')
                    mgoto(clean_up);

                if ((h[1] = *++p) == '\0')
                    mgoto(clean_up);

                if (hex_to_val(h, &u))
                    mgoto(clean_up);

                break;
            }

            set_cs(cs, i, u);

            if (add_concat)
                rn[k++] = CONCAT_CH;

            /* Record and move to next char set */
            rn[k++] = UCHAR_MAX + 1 + i++;
            add_concat = 1;
        } else if (*p == '[') {
            /* Gather set -- No escape sequences */
            /* Eat opening square bracket */
            if (*++p == '\0')
                mgoto(clean_up);

            if (*p == '^') {
                negate_set = 1;
                /* Eat caret */
                if (*++p == '\0')
                    mgoto(clean_up);
            } else {
                negate_set = 0;
            }

            /* Take any char literally in this position */
            set_cs(cs, i, *p);
            /* Eat first non-negation char */
            if (*++p == '\0')
                mgoto(clean_up);

            while (*p != ']') {
                if (*p == '-' && *(p + 1) != ']') {
                    /* Range */
                    for (j = *(p - 1); j <= *(p + 1); ++j)
                        set_cs(cs, i, j);

                    /* Eat range separator */
                    if (*++p == '\0')
                        mgoto(clean_up);
                } else {
                    set_cs(cs, i, *p);
                }
                /* Advance */
                if (*++p == '\0')
                    mgoto(clean_up);
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
                /* Any char */
                set_all_cs(cs, i);

                /* Except \n when newline sensitive */
                if (nl_sen)
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
        /* Advance. \0 checked at top of while loop. */
        ++p;
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

    fprintf(stderr, "\nCharacter sets:");
    for (k = 0; k < rn_len; ++k)
        if ((x = regex_nums[k]) > UCHAR_MAX) {
            fprintf(stderr, "\n%lu: ", x);
            for (j = 0; j <= UCHAR_MAX; ++j)
                if (is_set_cs(char_sets, x, j))
                    isgraph(j) ? fprintf(stderr, "%c ",
                                         j) : fprintf(stderr, "%02X ", j);
        }

    fprintf(stderr, "\n\nRegex:\n");
    for (k = 0; k < rn_len; ++k)
        if ((x = regex_nums[k]) > UCHAR_MAX)
            fprintf(stderr, "%lu ", x);
        else
            fprintf(stderr, "%c ", (unsigned char) x);

    putc('\n', stderr);
}


static void print_nfa(struct regex_info ri)
{
    size_t i, t;

    fprintf(stderr, "\nNFA:\ngraph LR\n");
    for (i = 0; i < ri.sa_len; ++i) {
        if ((t = ri.sa[i].t_a)) {
            if (t > UCHAR_MAX)
                fprintf(stderr, "%lu -- %lu --> %lu\n", i, t, ri.sa[i].a);
            else
                fprintf(stderr, "%lu -- %c --> %lu\n", i,
                        (unsigned char) t, ri.sa[i].a);

            if ((t = ri.sa[i].t_b))     /* Will always be 'e' if not zero */
                fprintf(stderr, "%lu -- %c --> %lu\n", i,
                        (unsigned char) t, ri.sa[i].b);
        }
    }
    putc('\n', stderr);
}


static size_t get_s_i(int *reuse, size_t *s_i_reuse, size_t *s_i)
{
    /*
     * Gives the next state available, allowing for the reuse of a deleted
     * state (which is needed for concatenation).
     */
    size_t res;

    if (*reuse) {
        res = *s_i_reuse;
        *s_i_reuse = 0;
        *reuse = 0;
    } else {
        res = *s_i;
        ++*s_i;
    }
    mreturn(res);
}


#define head nfa_stack[n_i - 1]
#define head_p1 nfa_stack[n_i]
#define head_m1 nfa_stack[n_i - 2]


static int generate_nfa(size_t *regex_postfix, size_t rp_len,
                        struct nfa *state_machine,
                        struct state **state_array, size_t *sa_len)
{
    /* Thompson's construction */
    struct nfa *nfa_stack = NULL;
    struct state *sa = NULL;    /* State array */
    size_t n_i = 0;             /* NFA stack index */
    size_t x, k;
    int reuse = 0;              /* To reuse deleted states */
    size_t s_i_reuse = 0;       /* Index of deleted state */
    size_t s_i = 0;             /* Next available state index */
    size_t new_start, new_end;
    size_t s;

    if (mof(rp_len, sizeof(struct nfa), SIZE_MAX))
        mgoto(clean_up);

    if ((nfa_stack = calloc(rp_len, sizeof(struct nfa))) == NULL)
        mgoto(clean_up);

    /*
     * Most elements of the postfix notation result in the creation of two
     * states in the NFA. The exception is concatenation, which removes one
     * state.
     */
    if (mof(rp_len, 2, SIZE_MAX))
        mgoto(clean_up);

    s = rp_len * 2;

    if (mof(s, sizeof(struct state), SIZE_MAX))
        mgoto(clean_up);

    if ((sa = calloc(s, sizeof(struct state))) == NULL)
        mreturn(1);

    for (k = 0; k < rp_len; ++k) {
        x = *(regex_postfix + k);
        if (x > UCHAR_MAX || x == '^' || x == '$') {
            /* Operand */
            head_p1.start = get_s_i(&reuse, &s_i_reuse, &s_i);
            head_p1.end = get_s_i(&reuse, &s_i_reuse, &s_i);
            sa[head_p1.start].t_a = x;
            sa[head_p1.start].a = head_p1.end;
            ++n_i;
        } else {
            /* Operator */
            switch (x) {
            case '*':
                /* Zero or more. Has loop back and bypass. */
                if (!n_i)       /* Unary operator */
                    mgoto(clean_up);

                /* Loop back */
                sa[head.end].t_a = 'e';
                sa[head.end].a = head.start;

                /* Get new start and end states */
                new_start = get_s_i(&reuse, &s_i_reuse, &s_i);
                new_end = get_s_i(&reuse, &s_i_reuse, &s_i);

                /* Connect new start */
                sa[new_start].t_a = 'e';
                sa[new_start].a = head.start;

                /* Bypass */
                sa[new_start].t_b = 'e';
                sa[new_start].b = new_end;

                /* Connect new end */
                sa[head.end].t_b = 'e';
                sa[head.end].b = new_end;

                /* Update start and end of NFA */
                head.start = new_start;
                head.end = new_end;
                break;
            case '+':
                /* One or more. Like * but no bypass. */
                if (!n_i)       /* Unary operator */
                    mgoto(clean_up);

                /* Loop back */
                sa[head.end].t_a = 'e';
                sa[head.end].a = head.start;

                /* Get new start and end states */
                new_start = get_s_i(&reuse, &s_i_reuse, &s_i);
                new_end = get_s_i(&reuse, &s_i_reuse, &s_i);

                /* Connect new start */
                sa[new_start].t_a = 'e';
                sa[new_start].a = head.start;

                /* Connect new end */
                sa[head.end].t_b = 'e';
                sa[head.end].b = new_end;

                /* Update start and end of NFA */
                head.start = new_start;
                head.end = new_end;
                break;
            case '?':
                /* Zero or one. Like * but no loop back. */
                if (!n_i)       /* Unary operator */
                    mgoto(clean_up);

                /* Get new start and end states */
                new_start = get_s_i(&reuse, &s_i_reuse, &s_i);
                new_end = get_s_i(&reuse, &s_i_reuse, &s_i);

                /* Connect new start */
                sa[new_start].t_a = 'e';
                sa[new_start].a = head.start;

                /* Bypass */
                sa[new_start].t_b = 'e';
                sa[new_start].b = new_end;

                /* Connect new end */
                sa[head.end].t_a = 'e';
                sa[head.end].a = new_end;

                /* Update start and end of NFA */
                head.start = new_start;
                head.end = new_end;
                break;
            case '.':
                /* Concatenate. Joins two NFAs in series. Releases a state. */
                if (n_i < 2)    /* Binary operator */
                    mgoto(clean_up);

                /* Copy branches from start of 2nd NFA to end of 1st NFA */
                sa[head_m1.end].t_a = sa[head.start].t_a;
                sa[head_m1.end].a = sa[head.start].a;
                sa[head_m1.end].t_b = sa[head.start].t_b;
                sa[head_m1.end].b = sa[head.start].b;

                /* Release start of second NFA */
                sa[head.start].t_a = '\0';
                sa[head.start].t_b = '\0';
                reuse = 1;
                s_i_reuse = head.start;

                /* End of combined NFA is end of the second NFA */
                head_m1.end = head.end;
                --n_i;
                break;
            case '|':
                /* Alternate. Joins two NFAs in parallel. */
                if (n_i < 2)    /* Binary operator */
                    mgoto(clean_up);

                /* Get new start and end states */
                new_start = get_s_i(&reuse, &s_i_reuse, &s_i);
                new_end = get_s_i(&reuse, &s_i_reuse, &s_i);

                /* Connect new start */
                /* Top branch */
                sa[new_start].t_a = 'e';
                sa[new_start].a = head.start;
                /* Bottom branch */
                sa[new_start].t_b = 'e';
                sa[new_start].b = head_m1.start;

                /* Connect new end */
                /* Top branch */
                sa[head.end].t_a = 'e';
                sa[head.end].a = new_end;
                /* Bottom branch */
                sa[head_m1.end].t_a = 'e';
                sa[head_m1.end].a = new_end;

                /* Update start and end of NFA */
                head_m1.start = new_start;
                head_m1.end = new_end;

                --n_i;
                break;
            }
        }

    }
    if (n_i != 1)
        mgoto(clean_up);

    *state_machine = head;
    free(nfa_stack);
    *state_array = sa;
    *sa_len = s_i;
    mreturn(0);

  clean_up:
    if (nfa_stack != NULL)
        free(nfa_stack);

    if (sa != NULL)
        free(sa);

    mreturn(1);
}


#undef head
#undef head_p1
#undef head_m1


static void free_regex(struct regex_info ri)
{
    free(ri.cs);
    free(ri.sa);
    free(ri.sl);
    free(ri.sl_next);
    free(ri.sl_from);
}

static int compile_regex(const char *regex_str, int nl_sen,
                         struct regex_info *ri, int verbose)
{
    struct regex_info reg_i;
    size_t *rn = NULL, rn_len, *rp = NULL, rp_len;

    reg_i.cs = NULL;
    reg_i.sa = NULL;
    reg_i.sl = NULL;
    reg_i.sl_next = NULL;
    reg_i.sl_from = NULL;

    if (preprocess_regex(regex_str, nl_sen, &reg_i.cs, &rn, &rn_len))
        mgoto(clean_up);

    if (shunting_yard_regex(rn, rn_len, &rp, &rp_len))
        mgoto(clean_up);

    free(rn);
    rn = NULL;

    if (verbose)
        print_regex(reg_i.cs, rp, rp_len);

    if (generate_nfa(rp, rp_len, &reg_i.sm, &reg_i.sa, &reg_i.sa_len))
        mgoto(clean_up);

    free(rp);
    rp = NULL;

    if ((reg_i.sl = malloc(reg_i.sa_len)) == NULL)
        mgoto(clean_up);

    if ((reg_i.sl_next = malloc(reg_i.sa_len)) == NULL)
        mgoto(clean_up);

    if ((reg_i.sl_from = malloc(reg_i.sa_len)) == NULL)
        mgoto(clean_up);

    if (verbose)
        print_nfa(reg_i);

    *ri = reg_i;

    mreturn(0);

  clean_up:
    free(rn);
    free(rp);
    free_regex(reg_i);
    mreturn(1);
}


static int is_done(unsigned char *sl, size_t sa_len, size_t match_state,
                   const unsigned char *p,
                   const unsigned char **p_max_match)
{
    /* Check for a match */
    size_t i, active_count;

    if (sl[match_state])
        *p_max_match = p;

    /* Check for none left */
    active_count = 0;
    for (i = 0; i < sa_len; ++i)
        if (sl[i])
            ++active_count;

    if (!active_count)
        return 1;

    return 0;
}


static int run_nfa(struct regex_info ri, const char *mem,
                   size_t mem_len, int sol, int nl_sen, size_t *match_len)
{
    unsigned char *tmp;
    const unsigned char *p, *p_stop, *p_max_match = NULL;
    unsigned char u;
    size_t t, i, x;
    int run_again;
    int eol;

    memset(ri.sl, '\0', ri.sa_len);
    ri.sl[ri.sm.start] = 1;

    p = (unsigned char *) mem;
    p_stop = p + mem_len;       /* Exclusive */

    while (1) {
        if (p == p_stop) {
            eol = 1;
        } else if (nl_sen && *p == '\n') {
            eol = 1;
        } else {
            eol = 0;
        }

        /*
         * No read. Recursively follow epsilon transitions and read states.
         * No elimination if cannot move.
         */
        memset(ri.sl_from, '\0', ri.sa_len);
        do {
            /* Clear next state list */
            memset(ri.sl_next, '\0', ri.sa_len);
            run_again = 0;
            for (i = 0; i < ri.sa_len; ++i)
                if (ri.sl[i]) {
                    if ((t = ri.sa[i].t_a) == 'e' || (sol && t == '^')
                        || (eol && t == '$')) {
                        ri.sl_from[i] = 1;
                        x = ri.sa[i].a;
                        if (ri.sl_from[x])
                            mgoto(error);       /* Recursive loop */

                        ri.sl_next[x] = 1;
                        run_again = 1;
                        /* b is only used when it is an epsilon split */
                        if (ri.sa[i].t_b == 'e') {
                            x = ri.sa[i].b;
                            if (ri.sl_from[x])
                                mgoto(error);   /* Recursive loop */

                            ri.sl_next[x] = 1;
                        }
                    } else {
                        /* Cannot move, so pass through. No elimination. */
                        ri.sl_next[i] = 1;
                    }
                }

            /* Switch state lists */
            tmp = ri.sl;
            ri.sl = ri.sl_next;
            ri.sl_next = tmp;
        } while (run_again);

        if (is_done(ri.sl, ri.sa_len, ri.sm.end, p, &p_max_match))
            break;

        if (p == p_stop)
            break;

        if (nl_sen && eol)
            break;

        /* Read char */
        u = *p++;

        /* Read. Must move or be elimated. */

        /* Clear next state list */
        memset(ri.sl_next, '\0', ri.sa_len);

        /* b is only ever epsilon, so do not need to check */
        for (i = 0; i < ri.sa_len; ++i)
            if (ri.sl[i] && (t = ri.sa[i].t_a) > UCHAR_MAX
                && is_set_cs(ri.cs, t, u))
                ri.sl_next[ri.sa[i].a] = 1;

        /* Switch state lists */
        tmp = ri.sl;
        ri.sl = ri.sl_next;
        ri.sl_next = tmp;

        if (is_done(ri.sl, ri.sa_len, ri.sm.end, p, &p_max_match))
            break;
    }

    if (p_max_match != NULL) {
        *match_len = p_max_match - (unsigned char *) mem;
        mreturn(0);             /* Match */
    } else {
        mreturn(NO_MATCH);
    }
  error:
    mreturn(1);
}


static int regex_find(const char *mem, size_t mem_len,
                      struct regex_info ri, int sol, int nl_sen,
                      size_t *match_offset, size_t *match_len)
{
    size_t m_len;
    const char *start;
    const char *stop;           /* Exclusive */
    int r;

    start = mem;
    stop = start + mem_len;

    while (1) {
        /*
         * Update start of line indicator. sol is determined by argument value
         * when start == mem.
         */
        if (start != mem) {
            if (nl_sen && *(start - 1) == '\n')
                sol = 1;
            else
                sol = 0;
        }

        /* Still run on a length of zero */
        r = run_nfa(ri, start, stop - start, sol, nl_sen, &m_len);

        if (r == 1)
            mreturn(1);

        if (!r)
            break;

        if (start == stop)
            break;

        ++start;
    }

    if (!r) {
        *match_offset = start - mem;
        *match_len = m_len;
        mreturn(0);
    } else {
        mreturn(NO_MATCH);
    }
}


int regex_search(const char *mem, size_t mem_len,
                 const char *regex_find_str, int sol,
                 int nl_sen, size_t *match_offset, size_t *match_len)
{
    struct regex_info ri;
    int r;

    if (compile_regex(regex_find_str, nl_sen, &ri, 0))
        mreturn(1);

    r = regex_find(mem, mem_len, ri, sol, nl_sen, match_offset, match_len);

    free_regex(ri);

    mreturn(r);
}


int regex_replace(const char *mem, size_t mem_len,
                  const char *regex_find_str, const char *replace,
                  size_t replace_len, int nl_sen, char **res,
                  size_t *res_len, int verbose)
{
    int ret = 1;
    struct regex_info ri;
    int sol;
    struct obuf *b = NULL;
    int r;
    const char *m, *m_stop;
    size_t match_offset, match_len;
    char *nr = NULL, *q;        /* New replace */
    size_t nr_len;
    const char *p, *p_stop;
    unsigned char h[2], u;

    if (compile_regex(regex_find_str, nl_sen, &ri, verbose))
        mreturn(1);

    if ((b = init_obuf(mem_len)) == NULL)
        mgoto(clean_up);

    if ((nr = malloc(replace_len)) == NULL)
        mgoto(clean_up);

    p = replace;
    p_stop = p + replace_len;
    q = nr;
    nr_len = 0;
    while (p != p_stop) {
        if ((u = *p) == '\\') {
            ++p;                /* Eat backslash */
            if ((u = *p) == '\0')
                mgoto(clean_up);

            switch (u) {
            case 't':
                u = '\t';
                break;
            case 'n':
                u = '\n';
                break;
            case 'r':
                u = '\r';
                break;
            case '0':
                u = '\0';
                break;
            case 'x':
                /* Two digit hex literal */
                ++p;
                if ((h[0] = *p) == '\0')
                    mgoto(clean_up);

                ++p;
                if ((h[1] = *p) == '\0')
                    mgoto(clean_up);

                if (hex_to_val(h, &u))
                    mgoto(clean_up);

                break;
            }
        }
        *q = u;
        ++q;
        ++nr_len;

        ++p;
    }

    m = mem;
    m_stop = m + mem_len;
    sol = 1;

    while (1) {
        /* sol is always 1 at the start */
        if (m != mem) {
            /* Adjust start of line read state */
            if (nl_sen && *(m - 1) == '\n')
                sol = 1;
            else
                sol = 0;
        }

        r = regex_find(m, m_stop - m, ri, sol, nl_sen, &match_offset,
                       &match_len);
        if (!r) {
            /* Match */
            /* Add text before match */
            if (put_mem(b, m, match_offset))
                mgoto(clean_up);

            /* Add replacement text */
            if (put_mem(b, nr, nr_len))
                mgoto(clean_up);

            /* Advance */
            m += match_offset;
            m += match_len;

            /* Stop when the end is reached from advancing a match */
            if (m == m_stop)
                break;

            /*
             * Continue if the end is reached from jumping over a character
             * due to a zero length match.
             */
            if (!match_len) {
                /*
                 * Move forward 1 if a 0 length match,
                 * but pass through the jumped char.
                 */
                if (put_ch(b, *m))
                    mgoto(clean_up);

                ++m;
            }
        } else if (r == NO_MATCH) {
            break;
        } else {
            mgoto(clean_up);
        }
    }

    /* Add any remaining text */
    if (put_mem(b, m, m_stop - m))
        mgoto(clean_up);

    /* \0 terminate in case used as a string */
    if (put_ch(b, '\0'))
        mgoto(clean_up);

    ret = 0;

  clean_up:
    free_regex(ri);
    free(nr);
    if (ret) {
        free_obuf(b);
    } else {
        *res = b->a;
        *res_len = b->i - 1;    /* Not counting terminating \0 */
        /* Just free the wrapper struct */
        free(b);
    }

    mreturn(ret);
}
