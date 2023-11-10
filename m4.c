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
 * An implementation of the m4 macro processor.
 *
 * Trust in the LORD with all your heart.
 *                       Proverbs 3:5 GNT
 */


#ifdef __linux__
/* For: snprintf */
#define _XOPEN_SOURCE 500
#endif

#include <ctype.h>
#include <limits.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "debug.h"
#include "gen.h"
#include "num.h"
#include "buf.h"
#include "eval.h"
#include "ht.h"
#include "fs.h"
#include "regex.h"


/* Number of buckets in hash table */
#define NUM_BUCKETS 1024

#define INIT_BUF_SIZE 512

/*
 * Do not change.
 * Diversion 0 continuously flushes to stdout.
 * Diversion -1 is index 10 which is continuously discarded.
 */
#define NUM_DIVS 11

/*
 * Do not change.
 * Macro name is index 0.
 * Only arguments 1 to 9 are allowed.
 */
#define NUM_ARGS 10


/*
 * When there is no stack, the output will be the active diversion.
 * Otherwise, during argument collection, the output will be the store buffer.
 * (Definitions and argument strings in the store are referenced by the stack).
 */
#define output (m4->stack == NULL ? m4->div[m4->active_div] : m4->store)

/* Arguments that have not been collected reference the empty string */
#define arg(n) (m4->store->a + m4->stack->arg_i[n])


struct macro_call {
    Fptr mfp;
    size_t def_i;               /* Index into store */
    /*
     * Indexes into store.
     * Name is arg 0 ($0), followed by collected args 1 to 9 ($1 to $9).
     * Uncollected args reference the empty string in store at index 0.
     */
    size_t arg_i[NUM_ARGS];
    size_t active_arg;          /* 0 is the macro name. First arg is 1. */
    size_t bracket_depth;       /* Depth of unquoted brackets */
    struct macro_call *next;    /* For nested macro calls */
};


typedef struct m4_info *M4ptr;

struct m4_info {
    int req_exit_val;           /* User requested exit value */
    struct ht *ht;
    int read_stdin;
    /* There is only one input. Characters are stored in reverse order. */
    struct ibuf *input;
    struct obuf *token;
    /* Used to read the next token to see if it is an open bracket */
    struct obuf *next_token;
    struct obuf *store;         /* Stores strings referenced by the stack */
    struct macro_call *stack;
    /* Pass through macro name to output (only use when called with no args) */
    int pass_through;
    struct obuf *tmp;           /* Used for substituting arguments */
    struct obuf *div[NUM_DIVS];
    size_t active_div;
    char left_quote[2];
    char right_quote[2];
    size_t quote_depth;
};


struct macro_call *init_mc(void)
{
    struct macro_call *mc;
    size_t i;

    if ((mc = malloc(sizeof(struct macro_call))) == NULL)
        mreturn(NULL);

    mc->mfp = NULL;
    mc->def_i = 0;
    for (i = 0; i < NUM_ARGS; ++i)
        mc->arg_i[i] = 0;       /* References the empty string in store */

    mc->active_arg = 0;
    mc->bracket_depth = 0;
    mc->next = NULL;
    mreturn(mc);
}

int stack_mc(struct macro_call **head)
{
    struct macro_call *mc;

    if ((mc = init_mc()) == NULL)
        mreturn(1);

    mc->next = *head;
    *head = mc;
    mreturn(0);
}

void pop_mc(struct macro_call **head)
{
    struct macro_call *mc;

    if (*head != NULL) {
        mc = (*head)->next;
        free(*head);
        *head = mc;
    }
}

void free_mc_stack(struct macro_call **head)
{
    if (head != NULL)
        while (*head != NULL)
            pop_mc(head);
}

int sub_args(M4ptr m4)
{
    const char *p;
    char ch, next_ch;
    size_t x;

    m4->tmp->i = 0;
    p = m4->store->a + m4->stack->def_i;

    while (1) {
        ch = *p;
        if (ch != '$') {
            if (put_ch(m4->tmp, ch))
                mreturn(1);
        } else {
            next_ch = *(p + 1);
            if (isdigit(next_ch)) {
                x = next_ch - '0';
                /* Can only access args that were collected */
                if (put_str(m4->tmp, arg(x)))
                    mreturn(1);

                ++p;            /* Eat the digit */
            } else {
                if (put_ch(m4->tmp, ch))
                    mreturn(1);
            }
        }

        if (ch == '\0')
            break;

        ++p;
    }

    if (unget_str(m4->input, m4->tmp->a))
        mreturn(1);

    mreturn(0);
}

void free_m4(M4ptr m4)
{
    size_t i;

    if (m4 != NULL) {
        free_ht(m4->ht);
        free_ibuf(m4->input);
        free_obuf(m4->token);
        free_obuf(m4->next_token);
        free_obuf(m4->store);
        free_mc_stack(&m4->stack);
        free_obuf(m4->tmp);
        for (i = 0; i < NUM_DIVS; ++i)
            free_obuf(m4->div[i]);

        free(m4);
    }
}

M4ptr init_m4(void)
{
    M4ptr m4;
    size_t i;

    if ((m4 = malloc(sizeof(struct m4_info))) == NULL)
        mreturn(NULL);

    m4->req_exit_val = -1;
    m4->ht = NULL;
    m4->input = NULL;
    m4->token = NULL;
    m4->next_token = NULL;
    m4->store = NULL;
    m4->stack = NULL;
    /* Used for substituting arguments */
    m4->tmp = NULL;
    for (i = 0; i < NUM_DIVS; ++i)
        m4->div[i] = NULL;

    if ((m4->ht = init_ht(NUM_BUCKETS)) == NULL)
        mgoto(error);

    m4->read_stdin = 0;

    if ((m4->input = init_ibuf(INIT_BUF_SIZE)) == NULL)
        mgoto(error);

    if ((m4->token = init_obuf(INIT_BUF_SIZE)) == NULL)
        mgoto(error);

    if ((m4->next_token = init_obuf(INIT_BUF_SIZE)) == NULL)
        mgoto(error);

    if ((m4->store = init_obuf(INIT_BUF_SIZE)) == NULL)
        mgoto(error);

    /* Setup empty string for uncollected args to reference at index 0 */
    if (put_ch(m4->store, '\0'))
        mgoto(error);

    m4->pass_through = 0;

    if ((m4->tmp = init_obuf(INIT_BUF_SIZE)) == NULL)
        mgoto(error);

    for (i = 0; i < NUM_DIVS; ++i)
        if ((m4->div[i] = init_obuf(INIT_BUF_SIZE)) == NULL)
            mgoto(error);

    m4->active_div = 0;
    m4->left_quote[0] = '`';
    m4->left_quote[1] = '\0';
    m4->right_quote[0] = '\'';
    m4->right_quote[1] = '\0';
    m4->quote_depth = 0;

    mreturn(m4);

  error:
    free_m4(m4);
    mreturn(NULL);
}

int dump_stack(M4ptr m4)
{
    struct macro_call *mc;
    size_t i;

    /* Make sure store is \0 terminated */
    if (put_ch(m4->store, '\0'))
        return 1;

    mc = m4->stack;
    while (mc != NULL) {
        fprintf(stderr, "\nDef  : %s\n",
                mc->mfp == NULL ? m4->store->a + mc->def_i : "built-in");

        for (i = 0; i <= mc->active_arg; ++i)
            fprintf(stderr, "Arg %lu: %s\n", (unsigned long) i,
                    m4->store->a + mc->arg_i[i]);

        mc = mc->next;
    }

    /* Undo \0 termination */
    --m4->store->i;

    return 0;
}


/* ********** Built-in macros ********** */

int define(void *v)
{
    /*@ define(macro_name, macro_def) */
    M4ptr m4 = (M4ptr) v;

    if (m4->stack->active_arg == 0) {
        /* Called without arguments, so set pass through indicator */
        m4->pass_through = 1;
        mreturn(0);
    }

    if (m4->stack->active_arg != 2)     /* Invalid number of arguments */
        mreturn(1);

    if (upsert(m4->ht, arg(1), arg(2), NULL))
        mreturn(1);

    mreturn(0);
}

int undefine(void *v)
{
    /*@ undefine(`macro_name') */
    M4ptr m4 = (M4ptr) v;

    if (m4->stack->active_arg == 0) {
        m4->pass_through = 1;
        mreturn(0);
    }

    if (m4->stack->active_arg != 1)     /* Invalid number of arguments */
        mreturn(1);

    if (delete_entry(m4->ht, arg(1)))
        mreturn(1);

    mreturn(0);
}

int changequote(void *v)
{
    /*@ changequote(left_quote, right_quote) */
    M4ptr m4 = (M4ptr) v;
    char l_ch, r_ch;

    if (m4->stack->active_arg == 0) {
        /* Called without arguments, so restore the defaults */
        m4->left_quote[0] = '`';
        m4->right_quote[0] = '\'';
        mreturn(0);
    }
    if (m4->stack->active_arg != 2)
        mreturn(1);

    l_ch = *arg(1);
    r_ch = *arg(2);

    if (!isgraph(l_ch) || !isgraph(r_ch) || l_ch == r_ch
        || strlen(arg(1)) != 1 || strlen(arg(2)) != 1)
        mreturn(1);

    m4->left_quote[0] = l_ch;
    m4->right_quote[0] = r_ch;

    mreturn(0);
}

int divert(void *v)
{
    /*@ divert or divert(div_num) */
    M4ptr m4 = (M4ptr) v;

    if (m4->stack->active_arg == 0) {
        m4->active_div = 0;
        mreturn(0);
    }
    if (m4->stack->active_arg != 1)
        mreturn(1);

    if (!strcmp(arg(1), "-1")) {
        m4->active_div = 10;
        mreturn(0);
    }
    if (strlen(arg(1)) == 1 && isdigit(*arg(1))) {
        m4->active_div = *arg(1) - '0';
        mreturn(0);
    }
    mreturn(1);
}

int undivert(void *v)
{
    /*@ undivert or undivert(div_num, filename, ...) */
    M4ptr m4 = (M4ptr) v;
    char ch;
    size_t i, x;

    if (m4->stack->active_arg == 0) {
        if (m4->active_div != 0)
            mreturn(1);

        for (i = 1; i < NUM_DIVS - 1; ++i)
            if (put_obuf(m4->div[m4->active_div], m4->div[i]))
                mreturn(1);

        mreturn(0);
    }
    for (i = 1; i <= m4->stack->active_arg; ++i) {
        ch = *arg(i);
        if (ch == '\0') {
            mreturn(1);
        } else if (isdigit(ch) && strlen(arg(i)) == 1
                   && (x = ch - '0') != m4->active_div) {
            if (put_obuf(m4->div[m4->active_div], m4->div[x]))
                mreturn(1);
        } else {
            /*
             * Assume a filename. Outputs directly to the active diversion,
             * even during argument collection.
             */
            if (put_file(m4->div[m4->active_div], arg(1)))
                mreturn(1);
        }
    }
    mreturn(0);
}

int writediv(void *v)
{
    /*@ writediv(div_num, filename) */
    M4ptr m4 = (M4ptr) v;
    char ch;

    if (m4->stack->active_arg == 0) {
        m4->pass_through = 1;
        mreturn(0);
    }

    if (m4->stack->active_arg != 2)
        mreturn(1);

    ch = *arg(1);
    /* Cannot write diversions 0 and -1 */
    if (strlen(arg(1)) == 1 && isdigit(ch) && ch != '0') {
        if (write_obuf(m4->div[ch - '0'], arg(2)))
            mreturn(1);
    } else {
        mreturn(1);
    }

    mreturn(0);
}

int divnum(void *v)
{
    /*@ divnum */
    M4ptr m4 = (M4ptr) v;
    char ch;

    if (m4->stack->active_arg != 0)
        mreturn(1);

    if (m4->active_div == 10) {
        if (unget_str(m4->input, "-1"))
            mreturn(1);

        mreturn(0);
    }

    ch = '0' + m4->active_div;
    if (unget_ch(m4->input, ch))
        mreturn(1);

    mreturn(0);
}

int include(void *v)
{
    /*@ include(filename) */
    M4ptr m4 = (M4ptr) v;

    if (m4->stack->active_arg == 0) {
        m4->pass_through = 1;
        mreturn(0);
    }
    if (m4->stack->active_arg != 1)
        mreturn(1);

    if (unget_file(m4->input, arg(1)))
        mreturn(1);

    mreturn(0);
}

int dnl(void *v)
{
    /*@ dnl */
    /* Delete to NewLine (inclusive) */
    M4ptr m4 = (M4ptr) v;

    if (m4->stack->active_arg != 0)
        mreturn(1);

    mreturn(delete_to_nl(m4->input, m4->read_stdin));
}

int tnl(void *v)
{
    /*@ tnl(str) */
    /* Trim NewLine chars at the end of the first argument */
    M4ptr m4 = (M4ptr) v;
    char *p, *q, ch;

    if (m4->stack->active_arg == 0) {
        m4->pass_through = 1;
        mreturn(0);
    }
    if (m4->stack->active_arg != 1)
        mreturn(1);

    p = arg(1);
    q = NULL;
    while ((ch = *p) != '\0') {
        if (ch == '\n') {
            if (q == NULL)
                q = p;
        } else {
            q = NULL;
        }

        ++p;
    }
    if (q != NULL)
        *q = '\0';

    if (unget_str(m4->input, arg(1)))
        mreturn(1);

    mreturn(0);
}

int regexrep(void *v)
{
    /*@ regexrep(text, regex_find, replace[, nl_insensitive[, verbose]]) */
    M4ptr m4 = (M4ptr) v;
    char *res;
    size_t res_len;
    int nl_sen = 1;             /* Newline sensitive is the default */
    int verbose = 0;            /* Prints information about the regex */

    if (m4->stack->active_arg == 0) {
        m4->pass_through = 1;
        mreturn(0);
    }
    if (m4->stack->active_arg != 3 && m4->stack->active_arg != 4
        && m4->stack->active_arg != 5)
        mreturn(1);

    if (m4->stack->active_arg == 4 && !strcmp(arg(4), "1"))
        nl_sen = 0;             /* Newline insensitive */

    if (m4->stack->active_arg == 5 && !strcmp(arg(5), "1"))
        verbose = 1;

    if (regex_replace(arg(1), strlen(arg(1)), arg(2),
                      arg(3), strlen(arg(3)), nl_sen, &res, &res_len,
                      verbose))
        mreturn(1);

    if (unget_str(m4->input, res))
        mreturn(1);

    mreturn(0);
}

int ifdef(void *v)
{
    /*@ ifdef(`macro_name', `when_defined', `when_undefined') */
    M4ptr m4 = (M4ptr) v;
    struct entry *e;

    if (m4->stack->active_arg == 0) {
        m4->pass_through = 1;
        mreturn(0);
    }
    if (m4->stack->active_arg != 2 && m4->stack->active_arg != 3)
        mreturn(1);

    e = lookup(m4->ht, arg(1));
    if (e != NULL) {
        if (unget_str(m4->input, arg(2)))
            mreturn(1);
    } else {
        if (m4->stack->active_arg == 3 && unget_str(m4->input, arg(3)))
            mreturn(1);
    }
    mreturn(0);
}

int ifelse(void *v)
{
    /*@ ifelse(A, B, `when_same', `when_different') */
    M4ptr m4 = (M4ptr) v;

    if (m4->stack->active_arg == 0) {
        m4->pass_through = 1;
        mreturn(0);
    }
    if (m4->stack->active_arg != 3 && m4->stack->active_arg != 4)
        mreturn(1);

    if (!strcmp(arg(1), arg(2))) {
        if (unget_str(m4->input, arg(3)))
            mreturn(1);
    } else {
        if (m4->stack->active_arg == 4 && unget_str(m4->input, arg(4)))
            mreturn(1);
    }
    mreturn(0);
}

int dumpdef(void *v)
{
    /*@ dumpdef or dumpdef(`macro_name', ...) */
    M4ptr m4 = (M4ptr) v;
    size_t i;
    struct entry *e;

    if (m4->stack->active_arg == 0) {
        /* Dump all macro definitions */
        for (i = 0; i < NUM_BUCKETS; ++i) {
            e = m4->ht->b[i];
            while (e != NULL) {
                fprintf(stderr, "%s: %s\n", e->name,
                        e->func_p == NULL ? e->def : "built-in");
                e = e->next;
            }
        }
        mreturn(0);
    }
    for (i = 1; i <= m4->stack->active_arg; ++i) {
        if (*arg(i) == '\0')
            mreturn(1);

        e = lookup(m4->ht, arg(i));
        if (e == NULL)
            fprintf(stderr, "%s: undefined\n", arg(i));
        else
            fprintf(stderr, "%s: %s\n", e->name,
                    e->func_p == NULL ? e->def : "built-in");
    }
    mreturn(0);
}

int errprint(void *v)
{
    /*@ errprint(error_message) */
    M4ptr m4 = (M4ptr) v;

    if (m4->stack->active_arg == 0) {
        m4->pass_through = 1;
        mreturn(0);
    }
    if (m4->stack->active_arg != 1)
        mreturn(1);

    fprintf(stderr, "%s\n", arg(1));
    mreturn(0);
}

int incr(void *v)
{
    /*@ incr(number) */
    M4ptr m4 = (M4ptr) v;
    size_t x;
    char num[NUM_BUF_SIZE];
    int r;

    if (m4->stack->active_arg == 0) {
        m4->pass_through = 1;
        mreturn(0);
    }
    if (m4->stack->active_arg != 1)
        mreturn(1);

    if (str_to_size_t(arg(1), &x))
        mreturn(1);

    ++x;

    r = snprintf(num, NUM_BUF_SIZE, "%lu", (unsigned long) x);
    if (r < 0 || r >= NUM_BUF_SIZE)
        mreturn(1);

    if (unget_str(m4->input, num))
        mreturn(1);

    mreturn(0);
}

int eval_math(void *v)
{
    /*@ eval(math[, verbose]) */
    M4ptr m4 = (M4ptr) v;
    long x;
    char num[NUM_BUF_SIZE];
    int r;
    int verbose = 0;

    if (m4->stack->active_arg == 0) {
        m4->pass_through = 1;
        mreturn(0);
    }
    if (m4->stack->active_arg != 1 && m4->stack->active_arg != 2)
        mreturn(1);

    if (m4->stack->active_arg == 2 && !strcmp(arg(2), "1"))
        verbose = 1;

    if (eval_str(arg(1), &x, verbose))
        mreturn(1);

    r = snprintf(num, NUM_BUF_SIZE, "%ld", x);
    if (r < 0 || r >= NUM_BUF_SIZE)
        mreturn(1);

    if (unget_str(m4->input, num))
        mreturn(1);

    mreturn(0);
}

int sysval(void *v)
{
    /*@ sysval */
    M4ptr m4 = (M4ptr) v;

    if (m4->stack->active_arg != 0)
        mreturn(1);

    if (unget_str(m4->input, m4->store->a + m4->stack->def_i))
        mreturn(1);

    mreturn(0);
}

int esyscmd(void *v)
{
    /*@ esyscmd(shell_command) */
    M4ptr m4 = (M4ptr) v;
    struct entry *e;
    FILE *fp;
    int x, st, r;
    char num[NUM_BUF_SIZE];

    if (m4->stack->active_arg == 0) {
        m4->pass_through = 1;
        mreturn(0);
    }
    if (m4->stack->active_arg != 1)
        mreturn(1);

    if ((fp = popen(arg(1), "r")) == NULL)
        mreturn(1);

    m4->tmp->i = 0;
    while ((x = getc(fp)) != EOF) {
        if (x != '\0' && x != '\r' && put_ch(m4->tmp, x)) {
            pclose(fp);
            mreturn(1);
        }
    }
    if (ferror(fp) || !feof(fp)) {
        pclose(fp);
        mreturn(1);
    }
    if ((st = pclose(fp)) == -1)
        mreturn(1);

#ifndef _WIN32
    if (!WIFEXITED(st))
        mreturn(1);
#endif

    if (put_ch(m4->tmp, '\0'))
        mreturn(1);

    if (unget_str(m4->input, m4->tmp->a))
        mreturn(1);

    e = lookup(m4->ht, "sysval");

    if (e != NULL && e->func_p != NULL) {
        /* Still a built-in macro */

#ifndef _WIN32
        st = WEXITSTATUS(st);
#endif

        r = snprintf(num, NUM_BUF_SIZE, "%lu", (unsigned long) st);
        if (r < 0 || r >= NUM_BUF_SIZE)
            mreturn(1);

        if (upsert(m4->ht, "sysval", num, &sysval))
            mreturn(1);
    }

    mreturn(0);
}

int m4exit(void *v)
{
    /*@ m4exit or m4exit(exit_value) */
    M4ptr m4 = (M4ptr) v;
    size_t x;

    if (m4->stack->active_arg == 0) {
        m4->req_exit_val = 0;
        mreturn(0);
    }
    if (m4->stack->active_arg != 1)
        mreturn(1);

    if (str_to_size_t(arg(1), &x))
        mreturn(1);

    if (x > UCHAR_MAX)
        mreturn(1);

    m4->req_exit_val = x;

    mreturn(0);
}

int recrm(void *v)
{
    /*@ recrm(path) */
    /* Recursively removes a path if it exists */
    M4ptr m4 = (M4ptr) v;

    if (m4->stack->active_arg == 0) {
        m4->pass_through = 1;
        mreturn(0);
    }
    if (m4->stack->active_arg != 1)
        mreturn(1);

    if (*arg(1) == '\0')
        mreturn(1);

    if (rec_rm(arg(1)))
        mreturn(1);

    mreturn(0);
}

int end_macro(M4ptr m4)
{
    if (m4->stack->mfp != NULL) {
        if ((*m4->stack->mfp) (m4)) {
            fprintf(stderr, "m4: %s: Failed\n",
                    m4->store->a + m4->stack->arg_i[0]);
            mreturn(1);
        }
    } else {
        if (sub_args(m4))
            mreturn(1);
    }

    /*
     * Truncate store.
     * Minus 1 for \0 char added at start of macro section.
     */
    m4->store->i = m4->stack->def_i - 1;
    /*
     * Pop before pass_through, so that output redirectes to the next
     * node (if any).
     */
    pop_mc(&m4->stack);

    if (m4->pass_through) {
        /*
         * Only use when macro was called without arguments, otherwise
         * token will no longer contain the macro name.
         */
        if (put_str(output, m4->token->a))
            mreturn(1);

        m4->pass_through = 0;
    }
    mreturn(0);
}

int main(int argc, char **argv)
{
    /*
     * ret is the automatic return value. 1 for error, 0 for success.
     * The user can request a specific exit value in the first argument of the
     * m4exit built-in macro. However, the return value will still be 1 if an
     * error occurs.
     */
    int ret = 1;                /* Defaults to error */

    /*
     * Can only request a positive return value.
     * -1 indicates that no request has been made.
     */
    int req_exit_val = -1;
    M4ptr m4;
    struct entry *e;            /* Used for macro lookups */
    int i, r;

    if (sane_io())
        mreturn(1);

    if ((m4 = init_m4()) == NULL)
        mgoto(clean_up);

    /* Load built-in macros */
    if (upsert(m4->ht, "define", NULL, &define))
        mgoto(clean_up);

    if (upsert(m4->ht, "undefine", NULL, &undefine))
        mgoto(clean_up);

    if (upsert(m4->ht, "changequote", NULL, &changequote))
        mgoto(clean_up);

    if (upsert(m4->ht, "divert", NULL, &divert))
        mgoto(clean_up);

    if (upsert(m4->ht, "undivert", NULL, &undivert))
        mgoto(clean_up);

    if (upsert(m4->ht, "writediv", NULL, &writediv))
        mgoto(clean_up);

    if (upsert(m4->ht, "divnum", NULL, &divnum))
        mgoto(clean_up);

    if (upsert(m4->ht, "include", NULL, &include))
        mgoto(clean_up);

    if (upsert(m4->ht, "dnl", NULL, &dnl))
        mgoto(clean_up);

    if (upsert(m4->ht, "tnl", NULL, &tnl))
        mgoto(clean_up);

    if (upsert(m4->ht, "regexrep", NULL, &regexrep))
        mgoto(clean_up);

    if (upsert(m4->ht, "ifdef", NULL, &ifdef))
        mgoto(clean_up);

    if (upsert(m4->ht, "ifelse", NULL, &ifelse))
        mgoto(clean_up);

    if (upsert(m4->ht, "dumpdef", NULL, &dumpdef))
        mgoto(clean_up);

    if (upsert(m4->ht, "errprint", NULL, &errprint))
        mgoto(clean_up);

    if (upsert(m4->ht, "incr", NULL, &incr))
        mgoto(clean_up);

    if (upsert(m4->ht, "eval", NULL, &eval_math))
        mgoto(clean_up);

    if (upsert(m4->ht, "esyscmd", NULL, &esyscmd))
        mgoto(clean_up);

    if (upsert(m4->ht, "sysval", NULL, &sysval))
        mgoto(clean_up);

    if (upsert(m4->ht, "m4exit", NULL, &m4exit))
        mgoto(clean_up);

    if (upsert(m4->ht, "recrm", NULL, &recrm))
        mgoto(clean_up);


    if (argc > 1) {
        for (i = argc - 1; i >= 1; --i)
            if (unget_file(m4->input, *(argv + i)))
                mgoto(clean_up);
    } else {
        m4->read_stdin = 1;
    }

    /*
     * Loops until the input is exhausted, or the user requests to exit,
     * or an error occurs.
     */
    while (1) {
        /* Need to copy as used after free of m4 */
        if ((req_exit_val = m4->req_exit_val) != -1)
            break;

        if (flush_obuf(m4->div[0]))
            mgoto(clean_up);

        m4->div[NUM_ARGS]->i = 0;

        r = get_word(m4->input, m4->token, m4->read_stdin);
        if (r == ERR)
            mgoto(clean_up);
        else if (r == EOF)
            break;

        if (!strcmp(m4->token->a, m4->left_quote)) {
            if (m4->quote_depth && put_str(output, m4->token->a))
                mgoto(clean_up);

            ++m4->quote_depth;
        } else if (!strcmp(m4->token->a, m4->right_quote)) {
            if (m4->quote_depth != 1 && put_str(output, m4->token->a))
                mgoto(clean_up);

            if (m4->quote_depth)
                --m4->quote_depth;
        } else if (m4->quote_depth) {
            /* Quoted */
            if (put_str(output, m4->token->a))
                mgoto(clean_up);
        } else if (m4->stack != NULL && m4->stack->bracket_depth == 1
                   && !strcmp(m4->token->a, ",")) {
            if (put_ch(output, '\0'))
                mgoto(clean_up);

            ++m4->stack->active_arg;
            if (m4->stack->active_arg == NUM_ARGS) {
                fprintf(stderr, "m4: %s: Too many arguments\n",
                        m4->store->a + m4->stack->arg_i[0]);
                goto clean_up;
            }

            m4->stack->arg_i[m4->stack->active_arg] = m4->store->i;

            /* Argument separator */
            if (eat_whitespace(m4->input, m4->read_stdin))
                mgoto(clean_up);

        } else if (m4->stack != NULL && m4->stack->bracket_depth == 1
                   && !strcmp(m4->token->a, ")")) {
            /* End of argument collection */
            if (put_ch(output, '\0'))
                mgoto(clean_up);

            if (end_macro(m4))
                mgoto(clean_up);
        } else if (m4->stack != NULL && !strcmp(m4->token->a, "(")) {
            /* Nested unquoted open bracket */
            if (put_str(output, m4->token->a))
                mgoto(clean_up);

            ++m4->stack->bracket_depth;
        } else if (m4->stack != NULL && m4->stack->bracket_depth > 1
                   && !strcmp(m4->token->a, ")")) {
            /* Nested unquoted close bracket */
            if (put_str(output, m4->token->a))
                mgoto(clean_up);

            --m4->stack->bracket_depth;
        } else {
            e = lookup(m4->ht, m4->token->a);
            if (e == NULL) {
                /* Not a macro, so pass through */
                if (put_str(output, m4->token->a))
                    mgoto(clean_up);
            } else {
                /*  Macro */
                /* See if called with or without brackets */
                r = get_word(m4->input, m4->next_token, m4->read_stdin);
                if (r == ERR)
                    mgoto(clean_up);

                if (stack_mc(&m4->stack))
                    mgoto(clean_up);

                /*
                 * Place a \0 char at the start of the new macro section in
                 * the store. This is to terminate the partially collected,
                 * in-progress, argument of the previous macro, so that
                 * dump_stack reports the correct information.
                 */
                if (put_ch(m4->store, '\0'))
                    mgoto(clean_up);

                m4->stack->bracket_depth = 1;
                m4->stack->mfp = e->func_p;
                /* One more than the start of the macro section in store */
                m4->stack->def_i = m4->store->i;
                if (e->def != NULL && put_str(m4->store, e->def))
                    mgoto(clean_up);

                if (put_ch(m4->store, '\0'))
                    mgoto(clean_up);

                m4->stack->arg_i[m4->stack->active_arg] = m4->store->i;
                if (put_str(m4->store, e->name))
                    mgoto(clean_up);

                if (put_ch(m4->store, '\0'))
                    mgoto(clean_up);

                if (r == EOF || strcmp(m4->next_token->a, "(")) {
                    /* Called without arguments */
                    if (r != EOF
                        && unget_str(m4->input, m4->next_token->a))
                        mgoto(clean_up);

                    if (end_macro(m4))
                        mgoto(clean_up);
                } else {
                    ++m4->stack->active_arg;
                    m4->stack->arg_i[m4->stack->active_arg] = m4->store->i;

                    /* Ready to collect arg 1 */
                    if (eat_whitespace(m4->input, m4->read_stdin))
                        mgoto(clean_up);
                }
            }
        }
    }

    ret = 0;                    /* Success so far */

    if (req_exit_val == -1) {
        /* Check */
        if (m4->stack != NULL) {
            fprintf(stderr, "m4: Stack not completed\n");
            ret = 1;
        }
        if (m4->quote_depth) {
            fprintf(stderr, "m4: Quotes not balanced\n");
            ret = 1;
        }
    }

    for (i = 0; i < NUM_DIVS - 1; ++i)
        flush_obuf(m4->div[i]);

  clean_up:
    if (ret)
        dump_stack(m4);

    free_m4(m4);

    if (req_exit_val != -1 && !ret)
        mreturn(req_exit_val);

    mreturn(ret);
}
