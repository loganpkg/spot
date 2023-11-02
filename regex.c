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

/* Regular expression module */

#include <ctype.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "debug.h"
#include "num.h"

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


static int preprocess_regex(const char *regex_str, int nl_sen,
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


static int validate_postfix(size_t *regex_postfix, size_t rp_len)
{
    size_t i, x, y;

    if (!rp_len)
        mreturn(1);

    for (i = 0; i < rp_len - 1; ++i) {
        x = regex_postfix[i];
        y = regex_postfix[i + 1];
        switch (x) {
        case '*':
        case '+':
        case '?':
        case '^':
        case '$':
            switch (y) {
                /* Operators with loop back */
            case '*':
            case '+':
                mreturn(1);     /* Recursive loop */
            }
            break;
        }
    }
    mreturn(0);                 /* OK */
}


void print_regex(unsigned char *char_sets,
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


size_t get_s_i(int *reuse, size_t *s_i_reuse, size_t *s_i)
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
     * Number of states in final NFA will be rp_len, but double it to give some
     * working space.
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

                /* Copy branches from start of second NFA to end of first NFA */
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


static int run_nfa(struct nfa state_machine, struct state *state_array,
                   size_t sa_len, unsigned char *sl,
                   unsigned char *sl_next, unsigned char *char_sets,
                   const char *mem, size_t mem_len, int sol, int nl_sen,
                   size_t *match_len)
{
    unsigned char *tmp;
    const unsigned char *p, *p_stop, *p_max_match = NULL;
    unsigned char u;
    size_t t, i;
    int run_again;
    int eol;

    memset(sl, '\0', sa_len);
    sl[state_machine.start] = 1;

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
        do {
            /* Clear next state list */
            memset(sl_next, '\0', sa_len);
            run_again = 0;
            for (i = 0; i < sa_len; ++i)
                if (sl[i]) {
                    if ((t = state_array[i].t_a) == 'e'
                        || (sol && t == '^')
                        || (eol && t == '$')) {
                        sl_next[state_array[i].a] = 1;
                        run_again = 1;
                        /* b is only used when it is an epsilon split */
                        if (state_array[i].t_b == 'e')
                            sl_next[state_array[i].b] = 1;
                    } else {
                        /* Cannot move, so pass through. No elimination. */
                        sl_next[i] = 1;
                    }
                }

            /* Switch state lists */
            tmp = sl;
            sl = sl_next;
            sl_next = tmp;
        } while (run_again);

        if (is_done(sl, sa_len, state_machine.end, p, &p_max_match))
            break;

        if (p == p_stop)
            break;

        if (nl_sen && eol)
            break;

        /* Read char */
        u = *p++;

        /* Read. Must move or be elimated. */

        /* Clear next state list */
        memset(sl_next, '\0', sa_len);

        /* b is only ever epsilon, so do not need to check */
        for (i = 0; i < sa_len; ++i)
            if (sl[i] && (t = state_array[i].t_a) > UCHAR_MAX
                && is_set_cs(char_sets, t, u))
                sl_next[state_array[i].a] = 1;

        /* Switch state lists */
        tmp = sl;
        sl = sl_next;
        sl_next = tmp;

        if (is_done(sl, sa_len, state_machine.end, p, &p_max_match))
            break;
    }

    if (p_max_match != NULL) {
        *match_len = p_max_match - (unsigned char *) mem;
        mreturn(0);             /* Match */
    } else {
        mreturn(NO_MATCH);
    }
}


int regex_search(const char *regex_str, const char *mem, size_t mem_len,
                 int nl_sen, size_t *match_offset, size_t *match_len)
{
    int ret = 1;                /* Failure */
    unsigned char *cs = NULL;
    size_t *rn = NULL, rn_len, *rp = NULL, rp_len;
    struct nfa sm;
    struct state *sa = NULL;
    size_t sa_len;
    unsigned char *sl = NULL;   /* (Active) state list */
    unsigned char *sl_next = NULL;      /* Next (active) state list */
    size_t m_len;
    const char *start;
    size_t len;
    int sol;                    /* Start of line */
    int r;

    if (preprocess_regex(regex_str, nl_sen, &cs, &rn, &rn_len))
        return 1;

    if (shunting_yard_regex(rn, rn_len, &rp, &rp_len))
        mgoto(clean_up);

    print_regex(cs, rp, rp_len);

    if (validate_postfix(rp, rp_len))
        mgoto(clean_up);

    if (generate_nfa(rp, rp_len, &sm, &sa, &sa_len))
        mgoto(clean_up);

    if ((sl = malloc(sa_len)) == NULL)
        mgoto(clean_up);

    if ((sl_next = malloc(sa_len)) == NULL)
        mgoto(clean_up);

    start = mem;
    len = mem_len;
    sol = 1;

    do {
        /* Still run on a len of zero */
        r = run_nfa(sm, sa, sa_len, sl, sl_next, cs, start, len, sol,
                    nl_sen, &m_len);
        if (!r)
            break;

        if (!len)
            break;

        ++start;
        --len;

        if (*start == '\n' && len && nl_sen) {
            /* Eat */
            ++start;
            --len;
            sol = 1;
        }
    } while (1);

    if (!r) {
        *match_offset = start - mem;
        *match_len = m_len;
        ret = 0;
    } else {
        ret = NO_MATCH;
    }

  clean_up:
    free(cs);
    free(rn);
    free(rp);
    free(sa);
    free(sl);
    free(sl_next);

    mreturn(ret);
}


int main(void)
{
    const char *regex = "a+";
    const char *str = "kkkaaaabbbb";
    size_t match_offset, match_len;
    int r;

    printf("%s\n%s\n", regex, str);

    r = regex_search(regex, str, strlen(str), 1, &match_offset,
                     &match_len);

    if (!r) {
        printf("MATCH:\n");
        fwrite(str + match_offset, 1, match_len, stdout);
    } else if (r == NO_MATCH) {
        printf("NO MATCH\n");
    } else {
        printf("ERROR\n");
    }

    mreturn(r);
}
