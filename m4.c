/*
 * Copyright (c) 2023, 2024 Logan Ryan McLintock
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


#include "toucanlib.h"


/* Number of buckets in hash table */
#define NUM_BUCKETS 1024

#define INIT_BUF_SIZE 512

/* Do not change */
#define DEFAULT_LEFT_QUOTE "`"
#define DEFAULT_RIGHT_QUOTE "'"

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
    struct obuf *store;         /* Stores strings referenced by the stack */
    struct macro_call *stack;
    struct obuf *tmp;           /* Used for substituting arguments */
    struct obuf *div[NUM_DIVS];
    size_t active_div;
    char *left_quote;
    char *right_quote;
    size_t quote_depth;
    /* Exit upon user related error. Turned off by default. */
    int error_exit;
    int help;                   /* Print help information for a macro */
};


struct macro_call *init_mc(void)
{
    struct macro_call *mc;
    size_t i;

    if ((mc = malloc(sizeof(struct macro_call))) == NULL) {
        fprintf(stderr, "%s:%d: Error\n", __FILE__, __LINE__);
        return NULL;
    }

    mc->mfp = NULL;
    mc->def_i = 0;
    for (i = 0; i < NUM_ARGS; ++i)
        mc->arg_i[i] = 0;       /* References the empty string in store */

    mc->active_arg = 0;
    mc->bracket_depth = 0;
    mc->next = NULL;
    return mc;
}

int stack_mc(struct macro_call **head)
{
    struct macro_call *mc;

    if ((mc = init_mc()) == NULL) {
        fprintf(stderr, "%s:%d: Error\n", __FILE__, __LINE__);
        return ERR;
    }

    mc->next = *head;
    *head = mc;
    return 0;
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
    size_t x, x_max;

    m4->tmp->i = 0;
    p = m4->store->a + m4->stack->def_i;

    x_max = 0;
    while (1) {
        ch = *p;
        if (ch != '$') {
            if (put_ch(m4->tmp, ch)) {
                fprintf(stderr, "%s:%d: Error\n", __FILE__, __LINE__);
                return ERR;
            }
        } else {
            next_ch = *(p + 1);
            if (isdigit(next_ch)) {
                x = next_ch - '0';
                /* Can only access args that were collected */
                if (x > m4->stack->active_arg) {
                    fprintf(stderr, "%s:%d: Usage error: "
                            "Uncollected argument accessed\n", __FILE__,
                            __LINE__);
                    return USAGE_ERR;
                }

                if (x > x_max)
                    x_max = x;

                if (put_str(m4->tmp, arg(x))) {
                    fprintf(stderr, "%s:%d: Error\n", __FILE__, __LINE__);
                    return ERR;
                }

                ++p;            /* Eat an extra char */
            } else {
                if (put_ch(m4->tmp, ch)) {
                    fprintf(stderr, "%s:%d: Error\n", __FILE__, __LINE__);
                    return ERR;
                }
            }
        }

        if (ch == '\0')
            break;

        ++p;
    }

    if (x_max != m4->stack->active_arg) {
        fprintf(stderr, "%s:%d: Error\n", __FILE__, __LINE__);
        return ERR;
    }

    if (unget_str(m4->input, m4->tmp->a)) {
        fprintf(stderr, "%s:%d: Error\n", __FILE__, __LINE__);
        return ERR;
    }

    return 0;
}

int end_macro(M4ptr m4)
{
    int ret;
    if (m4->stack->mfp != NULL) {
        if ((ret = (*m4->stack->mfp) (m4)))
            fprintf(stderr, "m4: %s: Failed\n",
                    m4->store->a + m4->stack->arg_i[0]);
    } else {
        ret = sub_args(m4);
    }

    /*
     * Truncate store.
     * Minus 1 for \0 char added at start of macro section.
     */
    m4->store->i = m4->stack->def_i - 1;

    /* Pop redirectes output to the next node (if any) */
    pop_mc(&m4->stack);

    return ret;
}

void free_m4(M4ptr m4)
{
    size_t i;

    if (m4 != NULL) {
        free_ht(m4->ht);
        free_ibuf(m4->input);
        free_obuf(m4->token);
        free_obuf(m4->store);
        free_mc_stack(&m4->stack);
        free_obuf(m4->tmp);
        for (i = 0; i < NUM_DIVS; ++i)
            free_obuf(m4->div[i]);

        free(m4->left_quote);
        free(m4->right_quote);

        free(m4);
    }
}

M4ptr init_m4(void)
{
    M4ptr m4;
    size_t i;

    if ((m4 = malloc(sizeof(struct m4_info))) == NULL) {
        fprintf(stderr, "%s:%d: Error\n", __FILE__, __LINE__);
        return NULL;
    }

    m4->req_exit_val = -1;
    m4->ht = NULL;
    m4->input = NULL;
    m4->token = NULL;
    m4->store = NULL;
    m4->stack = NULL;
    /* Used for substituting arguments */
    m4->tmp = NULL;
    for (i = 0; i < NUM_DIVS; ++i)
        m4->div[i] = NULL;

    if ((m4->ht = init_ht(NUM_BUCKETS)) == NULL) {
        fprintf(stderr, "%s:%d: Error\n", __FILE__, __LINE__);
        goto error;
    }

    m4->read_stdin = 0;

    if ((m4->input = init_ibuf(INIT_BUF_SIZE)) == NULL) {
        fprintf(stderr, "%s:%d: Error\n", __FILE__, __LINE__);
        goto error;
    }

    if ((m4->token = init_obuf(INIT_BUF_SIZE)) == NULL) {
        fprintf(stderr, "%s:%d: Error\n", __FILE__, __LINE__);
        goto error;
    }

    if ((m4->store = init_obuf(INIT_BUF_SIZE)) == NULL) {
        fprintf(stderr, "%s:%d: Error\n", __FILE__, __LINE__);
        goto error;
    }

    /* Setup empty string for uncollected args to reference at index 0 */
    if (put_ch(m4->store, '\0')) {
        fprintf(stderr, "%s:%d: Error\n", __FILE__, __LINE__);
        goto error;
    }

    if ((m4->tmp = init_obuf(INIT_BUF_SIZE)) == NULL) {
        fprintf(stderr, "%s:%d: Error\n", __FILE__, __LINE__);
        goto error;
    }

    for (i = 0; i < NUM_DIVS; ++i)
        if ((m4->div[i] = init_obuf(INIT_BUF_SIZE)) == NULL) {
            fprintf(stderr, "%s:%d: Error\n", __FILE__, __LINE__);
            goto error;
        }

    m4->active_div = 0;

    if ((m4->left_quote = strdup(DEFAULT_LEFT_QUOTE)) == NULL) {
        fprintf(stderr, "%s:%d: Error\n", __FILE__, __LINE__);
        goto error;
    }

    if ((m4->right_quote = strdup(DEFAULT_RIGHT_QUOTE)) == NULL) {
        fprintf(stderr, "%s:%d: Error\n", __FILE__, __LINE__);
        goto error;
    }

    m4->quote_depth = 0;
    m4->error_exit = 0;

    return m4;

  error:
    free_m4(m4);
    fprintf(stderr, "%s:%d: Error\n", __FILE__, __LINE__);
    return NULL;
}

int dump_stack(M4ptr m4)
{
    struct macro_call *mc;
    size_t i;

    /* Make sure store is \0 terminated */
    if (put_ch(m4->store, '\0'))
        return ERR;

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

int validate_quote(const char *quote)
{
    size_t i;
    char ch;
    i = 0;
    while (1) {
        ch = *(quote + i);
        if (ch == '\0')
            break;

        /* All chars must be graph non-comma and non-parentheses */
        if (!isgraph(ch) || ch == ',' || ch == '(' || ch == ')') {
            fprintf(stderr, "%s:%d: Error\n", __FILE__, __LINE__);
            return ERR;
        }

        ++i;
    }
    /* Empty quote */
    if (!i) {
        fprintf(stderr, "%s:%d: Error\n", __FILE__, __LINE__);
        return ERR;
    }

    return 0;
}

#define usage(num_pars, par_desc) do {                                  \
    if (m4->help) {                                                     \
        fprintf(stderr, "%s: %s: %s\n", "built-in", esf(NM), par_desc); \
        return 0;                                                       \
    }                                                                   \
    if (m4->stack->active_arg != num_pars) {                            \
        fprintf(stderr, "%s:%d: Usage: %s: %s\n", __FILE__, __LINE__,   \
                esf(NM), par_desc);                                     \
        return USAGE_ERR;                                               \
    }                                                                   \
} while (0)


/* ********** Built-in macros ********** */

/* See README.md for the syntax of the built-in macros */

#define NM define
int NM(void *v)
{
    M4ptr m4 = (M4ptr) v;
    const char *p;
    char ch, next_ch;
    size_t x, i;
    char present[NUM_ARGS];

    usage(2, "macro_name, macro_def");

    /* Validate definition */
    memset(present, 'N', NUM_ARGS);
    present[0] = 'Y';           /* Macro name is always present */
    p = arg(2);

    while (1) {
        ch = *p;

        if (ch == '\0') {
            break;
        } else if (ch == '$') {
            next_ch = *(p + 1);
            if (isdigit(next_ch)) {
                x = next_ch - '0';

                present[x] = 'Y';

                ++p;            /* Eat an extra char */
            }
        }

        ++p;
    }

    /* Check for holes in argument references */
    for (i = 0; i < NUM_ARGS; ++i)
        if (i && present[i] == 'Y' && present[i - 1] == 'N') {
            fprintf(stderr,
                    "%s:%d:%s: Syntax error: Invalid macro definition: "
                    "Gaps in argument references\n", __FILE__, __LINE__,
                    esf(NM));
            return SYNTAX_ERR;
        }

    if (upsert(m4->ht, arg(1), arg(2), NULL)) {
        fprintf(stderr, "%s:%d: Error\n", __FILE__, __LINE__);
        return ERR;
    }

    return 0;
}

#undef NM
#define NM undefine

int NM(void *v)
{
    M4ptr m4 = (M4ptr) v;

    usage(1, "quoted_macro_name");

    if (delete_entry(m4->ht, arg(1))) {
        fprintf(stderr, "%s:%d: Error\n", __FILE__, __LINE__);
        return ERR;
    }

    return 0;
}

#undef NM
#define NM changequote

int NM(void *v)
{
    M4ptr m4 = (M4ptr) v;

    usage(2, "left_quote, right_quote");

    if (validate_quote(arg(1)) || validate_quote(arg(2))) {
        fprintf(stderr, "%s:%d: Error\n", __FILE__, __LINE__);
        return ERR;
    }

    /* Quotes cannot be the same */
    if (!strcmp(arg(1), arg(2))) {
        fprintf(stderr, "%s:%d: Error\n", __FILE__, __LINE__);
        return ERR;
    }

    free(m4->left_quote);
    m4->left_quote = NULL;
    free(m4->right_quote);
    m4->right_quote = NULL;

    if ((m4->left_quote = strdup(arg(1))) == NULL) {
        fprintf(stderr, "%s:%d: Error\n", __FILE__, __LINE__);
        return ERR;
    }

    if ((m4->right_quote = strdup(arg(2))) == NULL) {
        fprintf(stderr, "%s:%d: Error\n", __FILE__, __LINE__);
        return ERR;
    }

    return 0;
}

#undef NM
#define NM divert

int NM(void *v)
{
    M4ptr m4 = (M4ptr) v;

    usage(1, "div_num");

    if (!strcmp(arg(1), "-1")) {
        m4->active_div = 10;
        return 0;
    }
    if (strlen(arg(1)) == 1 && isdigit(*arg(1))) {
        m4->active_div = *arg(1) - '0';
        return 0;
    }

    fprintf(stderr, "%s:%d: Error\n", __FILE__, __LINE__);
    return ERR;
}

#undef NM
#define NM undivert

int NM(void *v)
{
    M4ptr m4 = (M4ptr) v;
    char ch;
    size_t x;

    usage(1, "div_num_or_filename");

    ch = *arg(1);
    if (ch == '\0') {
        fprintf(stderr, "%s:%d:%s: Usage error: "
                "Argument is empty string\n", __FILE__, __LINE__, esf(NM));
        return USAGE_ERR;
    } else if (isdigit(ch) && strlen(arg(1)) == 1
               && (x = ch - '0') != m4->active_div) {
        if (put_obuf(m4->div[m4->active_div], m4->div[x])) {
            fprintf(stderr, "%s:%d: Error\n", __FILE__, __LINE__);
            return ERR;
        }
    } else {
        /*
         * Assume a filename. Outputs directly to the active diversion,
         * even during argument collection.
         */
        if (put_file(m4->div[m4->active_div], arg(1))) {
            fprintf(stderr, "%s:%d: Error\n", __FILE__, __LINE__);
            return ERR;
        }
    }

    return 0;
}

#undef NM
#define NM writediv

int NM(void *v)
{
    M4ptr m4 = (M4ptr) v;
    char ch;

    usage(2, "div_num, filename");

    ch = *arg(1);
    /* Cannot write diversions 0 and -1 */
    if (strlen(arg(1)) == 1 && isdigit(ch) && ch != '0') {
        if (write_obuf(m4->div[ch - '0'], arg(2))) {
            fprintf(stderr, "%s:%d: Error\n", __FILE__, __LINE__);
            return ERR;
        }
    } else {
        fprintf(stderr, "%s:%d: Error\n", __FILE__, __LINE__);
        return ERR;
    }

    return 0;
}

#undef NM
#define NM divnum

int NM(void *v)
{
    M4ptr m4 = (M4ptr) v;
    char ch;

    usage(0, "");

    if (m4->active_div == 10) {
        if (unget_str(m4->input, "-1")) {
            fprintf(stderr, "%s:%d: Error\n", __FILE__, __LINE__);
            return ERR;
        }

        return 0;
    }

    ch = '0' + m4->active_div;
    if (unget_ch(m4->input, ch)) {
        fprintf(stderr, "%s:%d: Error\n", __FILE__, __LINE__);
        return ERR;
    }

    return 0;
}

#undef NM
#define NM include

int NM(void *v)
{
    M4ptr m4 = (M4ptr) v;

    usage(1, "filename");

    if (unget_file(m4->input, arg(1))) {
        fprintf(stderr, "%s:%d: Error\n", __FILE__, __LINE__);
        return ERR;
    }

    return 0;
}

#undef NM
#define NM dnl

int NM(void *v)
{
    M4ptr m4 = (M4ptr) v;

    usage(0, "");

    return delete_to_nl(m4->input, m4->read_stdin);
}

#undef NM
#define NM tnl

int NM(void *v)
{
    M4ptr m4 = (M4ptr) v;
    char *p, *q, ch;

    usage(1, "str");

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

    if (unget_str(m4->input, arg(1))) {
        fprintf(stderr, "%s:%d: Error\n", __FILE__, __LINE__);
        return ERR;
    }

    return 0;
}

#undef NM
#define NM regexrep

int NM(void *v)
{
    M4ptr m4 = (M4ptr) v;
    int ret = ERR;
    char *res;
    size_t res_len;
    int nl_sen = 1;             /* Newline sensitive is the default */
    int verbose = 0;            /* Prints information about the regex */

    usage(5, "text, regex_find, replace, nl_insensitive, verbose");

    if (!strcmp(arg(4), "1"))
        nl_sen = 0;             /* Newline insensitive */

    if (!strcmp(arg(5), "1"))
        verbose = 1;

    if ((ret = regex_replace(arg(1), strlen(arg(1)), arg(2),
                             arg(3), strlen(arg(3)), nl_sen, &res,
                             &res_len, verbose)))
        return ret;

    if (unget_str(m4->input, res)) {
        free(res);
        fprintf(stderr, "%s:%d: Error\n", __FILE__, __LINE__);
        return ERR;
    }

    free(res);

    return 0;
}

#undef NM
#define NM lsdir

int NM(void *v)
{
    M4ptr m4 = (M4ptr) v;
    char *res;

    usage(1, "dir_name");

    if ((res = ls_dir(arg(1))) == NULL) {
        fprintf(stderr, "%s:%d: Error\n", __FILE__, __LINE__);
        return ERR;
    }

    if (unget_str(m4->input, res)) {
        free(res);
        fprintf(stderr, "%s:%d: Error\n", __FILE__, __LINE__);
        return ERR;
    }

    free(res);

    return 0;
}

#undef NM
#define NM ifdef

int NM(void *v)
{
    M4ptr m4 = (M4ptr) v;
    struct entry *e;

    usage(3,
          "quoted_macro_name, quoted_when_defined, quoted_when_undefined");

    e = lookup(m4->ht, arg(1));
    if (e != NULL) {
        if (unget_str(m4->input, arg(2))) {
            fprintf(stderr, "%s:%d: Error\n", __FILE__, __LINE__);
            return ERR;
        }
    } else {
        if (unget_str(m4->input, arg(3))) {
            fprintf(stderr, "%s:%d: Error\n", __FILE__, __LINE__);
            return ERR;
        }
    }
    return 0;
}

#undef NM
#define NM ifelse

int NM(void *v)
{
    M4ptr m4 = (M4ptr) v;

    usage(4, "A, B, quoted_when_same, quoted_when_different");

    if (!strcmp(arg(1), arg(2))) {
        if (unget_str(m4->input, arg(3))) {
            fprintf(stderr, "%s:%d: Error\n", __FILE__, __LINE__);
            return ERR;
        }
    } else {
        if (unget_str(m4->input, arg(4))) {
            fprintf(stderr, "%s:%d: Error\n", __FILE__, __LINE__);
            return ERR;
        }
    }
    return 0;
}

#undef NM
#define NM dumpdef

int NM(void *v)
{
    M4ptr m4 = (M4ptr) v;
    int ret;
    struct entry *e;

    usage(1, "quoted_macro_name");

    if (*arg(1) == '\0') {
        fprintf(stderr, "%s:%d:%s: Usage error: "
                "Argument is empty string\n", __FILE__, __LINE__, esf(NM));
        return ERR;
    }

    e = lookup(m4->ht, arg(1));
    if (e == NULL) {
        fprintf(stderr, "Undefined: %s\n", arg(1));
    } else {
        if (e->func_p == NULL) {
            fprintf(stderr, "User-def: %s: %s\n", e->name, e->def);
        } else {
            m4->help = 1;
            if ((ret = (*e->func_p) (m4))) {
                m4->help = 0;
                return ret;
            }
            m4->help = 0;
        }
    }

    return 0;
}

#undef NM
#define NM dumpdefall

int NM(void *v)
{
    M4ptr m4 = (M4ptr) v;
    int ret;
    size_t i;
    struct entry *e;

    usage(0, "");

    /* Dump all macro definitions */
    m4->help = 1;
    for (i = 0; i < NUM_BUCKETS; ++i) {
        e = m4->ht->b[i];
        while (e != NULL) {
            if (e->func_p == NULL) {
                fprintf(stderr, "User-def: %s: %s\n", e->name, e->def);
            } else {
                if ((ret = (*e->func_p) (m4))) {
                    m4->help = 0;
                    return ret;
                }
            }
            e = e->next;
        }
    }

    m4->help = 0;

    return 0;
}

#undef NM
#define NM errprint

int NM(void *v)
{
    M4ptr m4 = (M4ptr) v;

    usage(1, "error_message");

    fprintf(stderr, "%s\n", arg(1));
    return 0;
}

#undef NM
#define NM incr

int NM(void *v)
{
    M4ptr m4 = (M4ptr) v;
    size_t x;
    char num[NUM_BUF_SIZE];
    int r;

    usage(1, "number");

    if (str_to_size_t(arg(1), &x)) {
        fprintf(stderr, "%s:%d: Error\n", __FILE__, __LINE__);
        return ERR;
    }

    ++x;

    r = snprintf(num, NUM_BUF_SIZE, "%lu", (unsigned long) x);
    if (r < 0 || r >= NUM_BUF_SIZE) {
        fprintf(stderr, "%s:%d: Error\n", __FILE__, __LINE__);
        return ERR;
    }

    if (unget_str(m4->input, num)) {
        fprintf(stderr, "%s:%d: Error\n", __FILE__, __LINE__);
        return ERR;
    }

    return 0;
}

#undef NM
#define NM evalmath

int NM(void *v)
{
    M4ptr m4 = (M4ptr) v;
    int ret = ERR;
    long x;
    char num[NUM_BUF_SIZE];
    int r;
    int verbose = 0;

    usage(2, "arithmetic_expression, verbose");

    if (!strcmp(arg(2), "1"))
        verbose = 1;

    if ((ret = eval_str(arg(1), &x, verbose)))
        return ret;

    r = snprintf(num, NUM_BUF_SIZE, "%ld", x);
    if (r < 0 || r >= NUM_BUF_SIZE) {
        fprintf(stderr, "%s:%d: Error\n", __FILE__, __LINE__);
        return ERR;
    }

    if (unget_str(m4->input, num)) {
        fprintf(stderr, "%s:%d: Error\n", __FILE__, __LINE__);
        return ERR;
    }

    return 0;
}

#undef NM
#define NM sysval

int NM(void *v)
{
    M4ptr m4 = (M4ptr) v;

    usage(0, "");

    if (unget_str(m4->input, m4->store->a + m4->stack->def_i)) {
        fprintf(stderr, "%s:%d: Error\n", __FILE__, __LINE__);
        return ERR;
    }

    return 0;
}

#undef NM
#define NM esyscmd

int NM(void *v)
{
    M4ptr m4 = (M4ptr) v;
    struct entry *e;
    FILE *fp;
    int x, st, r;
    char num[NUM_BUF_SIZE];

    usage(1, "shell_command");

    if ((fp = popen(arg(1), "r")) == NULL) {
        fprintf(stderr, "%s:%d: Error\n", __FILE__, __LINE__);
        return ERR;
    }

    m4->tmp->i = 0;
    while ((x = getc(fp)) != EOF) {
        if (x != '\0' && x != '\r' && put_ch(m4->tmp, x)) {
            pclose(fp);
            fprintf(stderr, "%s:%d: Error\n", __FILE__, __LINE__);
            return ERR;
        }
    }
    if (ferror(fp) || !feof(fp)) {
        pclose(fp);
        fprintf(stderr, "%s:%d: Error\n", __FILE__, __LINE__);
        return ERR;
    }
    if ((st = pclose(fp)) == -1) {
        fprintf(stderr, "%s:%d: Error\n", __FILE__, __LINE__);
        return ERR;
    }
#ifndef _WIN32
    if (!WIFEXITED(st)) {
        fprintf(stderr, "%s:%d: Error\n", __FILE__, __LINE__);
        return ERR;
    }
#endif

    if (put_ch(m4->tmp, '\0')) {
        fprintf(stderr, "%s:%d: Error\n", __FILE__, __LINE__);
        return ERR;
    }

    if (unget_str(m4->input, m4->tmp->a)) {
        fprintf(stderr, "%s:%d: Error\n", __FILE__, __LINE__);
        return ERR;
    }

    e = lookup(m4->ht, "sysval");

    if (e != NULL && e->func_p != NULL) {
        /* Still a built-in macro */

#ifndef _WIN32
        st = WEXITSTATUS(st);
#endif

        r = snprintf(num, NUM_BUF_SIZE, "%lu", (unsigned long) st);
        if (r < 0 || r >= NUM_BUF_SIZE) {
            fprintf(stderr, "%s:%d: Error\n", __FILE__, __LINE__);
            return ERR;
        }

        if (upsert(m4->ht, "sysval", num, &sysval)) {
            fprintf(stderr, "%s:%d: Error\n", __FILE__, __LINE__);
            return ERR;
        }
    }

    return 0;
}

#undef NM
#define NM m4exit

int NM(void *v)
{
    M4ptr m4 = (M4ptr) v;
    size_t x;

    usage(1, "exit_value");

    if (str_to_size_t(arg(1), &x)) {
        fprintf(stderr, "%s:%d: Error\n", __FILE__, __LINE__);
        return ERR;
    }

    if (x > UCHAR_MAX) {
        fprintf(stderr, "%s:%d: Error\n", __FILE__, __LINE__);
        return ERR;
    }

    m4->req_exit_val = x;

    return 0;
}

#undef NM
#define NM errok

int NM(void *v)
{
    M4ptr m4 = (M4ptr) v;

    usage(0, "");

    m4->error_exit = 0;

    return 0;
}

#undef NM
#define NM errexit

int NM(void *v)
{
    M4ptr m4 = (M4ptr) v;

    usage(0, "");

    m4->error_exit = 1;

    return 0;
}

#undef NM
#define NM recrm

int NM(void *v)
{
    M4ptr m4 = (M4ptr) v;

    usage(1, "path");

    if (*arg(1) == '\0') {
        fprintf(stderr, "%s:%d: Error\n", __FILE__, __LINE__);
        return ERR;
    }

    if (rec_rm(arg(1))) {
        fprintf(stderr, "%s:%d: Error\n", __FILE__, __LINE__);
        return ERR;
    }

    return 0;
}

#undef NM

int main(int argc, char **argv)
{
    /*
     * ret is the automatic return value. 1 for error, 0 for success.
     * The user can request a specific exit value in the first argument of the
     * m4exit built-in macro. However, the return value will still be 1 if an
     * error occurs.
     */
    int ret = 0;                /* Success so far */
    int mrv = 0;                /* Macro return value */
    /*
     * Can only request a positive return value.
     * -1 indicates that no request has been made.
     */
    int req_exit_val = -1;
    M4ptr m4;
    struct entry *e;            /* Used for macro lookups */
    int i, r;

    if (sane_io()) {
        fprintf(stderr, "%s:%d: Error\n", __FILE__, __LINE__);
        return ERR;
    }

    if ((m4 = init_m4()) == NULL) {

        fprintf(stderr, "%s:%d: Error\n", __FILE__, __LINE__);
        goto error;
    }

    /* Load built-in macros */
    if (upsert(m4->ht, "define", NULL, &define)) {

        fprintf(stderr, "%s:%d: Error\n", __FILE__, __LINE__);
        goto error;
    }

    if (upsert(m4->ht, "undefine", NULL, &undefine)) {

        fprintf(stderr, "%s:%d: Error\n", __FILE__, __LINE__);
        goto error;
    }

    if (upsert(m4->ht, "changequote", NULL, &changequote)) {

        fprintf(stderr, "%s:%d: Error\n", __FILE__, __LINE__);
        goto error;
    }

    if (upsert(m4->ht, "divert", NULL, &divert)) {

        fprintf(stderr, "%s:%d: Error\n", __FILE__, __LINE__);
        goto error;
    }

    if (upsert(m4->ht, "undivert", NULL, &undivert)) {

        fprintf(stderr, "%s:%d: Error\n", __FILE__, __LINE__);
        goto error;
    }

    if (upsert(m4->ht, "writediv", NULL, &writediv)) {

        fprintf(stderr, "%s:%d: Error\n", __FILE__, __LINE__);
        goto error;
    }

    if (upsert(m4->ht, "divnum", NULL, &divnum)) {

        fprintf(stderr, "%s:%d: Error\n", __FILE__, __LINE__);
        goto error;
    }

    if (upsert(m4->ht, "include", NULL, &include)) {

        fprintf(stderr, "%s:%d: Error\n", __FILE__, __LINE__);
        goto error;
    }

    if (upsert(m4->ht, "dnl", NULL, &dnl)) {

        fprintf(stderr, "%s:%d: Error\n", __FILE__, __LINE__);
        goto error;
    }

    if (upsert(m4->ht, "tnl", NULL, &tnl)) {

        fprintf(stderr, "%s:%d: Error\n", __FILE__, __LINE__);
        goto error;
    }

    if (upsert(m4->ht, "regexrep", NULL, &regexrep)) {

        fprintf(stderr, "%s:%d: Error\n", __FILE__, __LINE__);
        goto error;
    }

    if (upsert(m4->ht, "lsdir", NULL, &lsdir)) {

        fprintf(stderr, "%s:%d: Error\n", __FILE__, __LINE__);
        goto error;
    }

    if (upsert(m4->ht, "ifdef", NULL, &ifdef)) {

        fprintf(stderr, "%s:%d: Error\n", __FILE__, __LINE__);
        goto error;
    }

    if (upsert(m4->ht, "ifelse", NULL, &ifelse)) {

        fprintf(stderr, "%s:%d: Error\n", __FILE__, __LINE__);
        goto error;
    }

    if (upsert(m4->ht, "dumpdef", NULL, &dumpdef)) {

        fprintf(stderr, "%s:%d: Error\n", __FILE__, __LINE__);
        goto error;
    }

    if (upsert(m4->ht, "dumpdefall", NULL, &dumpdefall)) {

        fprintf(stderr, "%s:%d: Error\n", __FILE__, __LINE__);
        goto error;
    }

    if (upsert(m4->ht, "errprint", NULL, &errprint)) {

        fprintf(stderr, "%s:%d: Error\n", __FILE__, __LINE__);
        goto error;
    }

    if (upsert(m4->ht, "incr", NULL, &incr)) {

        fprintf(stderr, "%s:%d: Error\n", __FILE__, __LINE__);
        goto error;
    }

    if (upsert(m4->ht, "evalmath", NULL, &evalmath)) {

        fprintf(stderr, "%s:%d: Error\n", __FILE__, __LINE__);
        goto error;
    }

    if (upsert(m4->ht, "esyscmd", NULL, &esyscmd)) {

        fprintf(stderr, "%s:%d: Error\n", __FILE__, __LINE__);
        goto error;
    }

    if (upsert(m4->ht, "sysval", NULL, &sysval)) {

        fprintf(stderr, "%s:%d: Error\n", __FILE__, __LINE__);
        goto error;
    }

    if (upsert(m4->ht, "m4exit", NULL, &m4exit)) {

        fprintf(stderr, "%s:%d: Error\n", __FILE__, __LINE__);
        goto error;
    }

    if (upsert(m4->ht, "errok", NULL, &errok)) {

        fprintf(stderr, "%s:%d: Error\n", __FILE__, __LINE__);
        goto error;
    }

    if (upsert(m4->ht, "errexit", NULL, &errexit)) {

        fprintf(stderr, "%s:%d: Error\n", __FILE__, __LINE__);
        goto error;
    }

    if (upsert(m4->ht, "recrm", NULL, &recrm)) {

        fprintf(stderr, "%s:%d: Error\n", __FILE__, __LINE__);
        goto error;
    }


    if (argc > 1) {
        for (i = argc - 1; i >= 1; --i)
            if (unget_file(m4->input, *(argv + i))) {

                fprintf(stderr, "%s:%d: Error\n", __FILE__, __LINE__);
                goto error;
            }
    } else {
        m4->read_stdin = 1;
    }

    /*
     * Loops until the input is exhausted, or the user requests to exit,
     * or an error occurs.
     */
    while (1) {
      top:

        /* Need to copy as used after free of m4 */
        if ((req_exit_val = m4->req_exit_val) != -1)
            break;

        /* Need to clear macro return value */
        mrv = 0;

        if (flush_obuf(m4->div[0])) {

            fprintf(stderr, "%s:%d: Error\n", __FILE__, __LINE__);
            goto error;
        }

        m4->div[NUM_ARGS]->i = 0;

        r = eat_str_if_match(m4->input, m4->left_quote, m4->read_stdin);
        if (r == ERR) {

            fprintf(stderr, "%s:%d: Error\n", __FILE__, __LINE__);
            goto error;
        }

        if (r == MATCH) {
            if (m4->quote_depth && put_str(output, m4->left_quote)) {

                fprintf(stderr, "%s:%d: Error\n", __FILE__, __LINE__);
                goto error;
            }

            ++m4->quote_depth;
            /* As might have multiple quotes in a row */
            goto top;
        }

        r = eat_str_if_match(m4->input, m4->right_quote, m4->read_stdin);
        if (r == ERR) {

            fprintf(stderr, "%s:%d: Error\n", __FILE__, __LINE__);
            goto error;
        }

        if (r == MATCH) {
            if (m4->quote_depth != 1 && put_str(output, m4->right_quote)) {

                fprintf(stderr, "%s:%d: Error\n", __FILE__, __LINE__);
                goto error;
            }

            if (m4->quote_depth)
                --m4->quote_depth;

            /* As might have multiple quotes in a row */
            goto top;
        }

        /* Not a quote, so read a token */
        r = get_word(m4->input, m4->token, m4->read_stdin);
        if (r == ERR) {
            fprintf(stderr, "%s:%d: Error\n", __FILE__, __LINE__);
            goto error;
        } else if (r == EOF) {
            break;
        }

        if (m4->quote_depth) {
            /* Quoted */
            if (put_str(output, m4->token->a)) {

                fprintf(stderr, "%s:%d: Error\n", __FILE__, __LINE__);
                goto error;
            }
        } else if (m4->stack != NULL && m4->stack->bracket_depth == 1
                   && !strcmp(m4->token->a, ",")) {
            if (put_ch(output, '\0')) {

                fprintf(stderr, "%s:%d: Error\n", __FILE__, __LINE__);
                goto error;
            }

            ++m4->stack->active_arg;
            if (m4->stack->active_arg == NUM_ARGS) {
                fprintf(stderr, "m4: %s: Too many arguments\n",
                        m4->store->a + m4->stack->arg_i[0]);
                goto error;
            }

            m4->stack->arg_i[m4->stack->active_arg] = m4->store->i;

            /* Argument separator */
            if (eat_whitespace(m4->input, m4->read_stdin)) {

                fprintf(stderr, "%s:%d: Error\n", __FILE__, __LINE__);
                goto error;
            }

        } else if (m4->stack != NULL && m4->stack->bracket_depth == 1
                   && !strcmp(m4->token->a, ")")) {
            /* End of argument collection */
            if (put_ch(output, '\0')) {

                fprintf(stderr, "%s:%d: Error\n", __FILE__, __LINE__);
                goto error;
            }

            if ((mrv = end_macro(m4))) {
                ret = mrv;      /* Save for exit time */
                if ((mrv == ERR || m4->error_exit)) {
                    fprintf(stderr, "%s:%d: Error\n", __FILE__, __LINE__);
                    goto error;
                }
            }
        } else if (m4->stack != NULL && !strcmp(m4->token->a, "(")) {
            /* Nested unquoted open bracket */
            if (put_str(output, m4->token->a)) {

                fprintf(stderr, "%s:%d: Error\n", __FILE__, __LINE__);
                goto error;
            }

            ++m4->stack->bracket_depth;
        } else if (m4->stack != NULL && m4->stack->bracket_depth > 1
                   && !strcmp(m4->token->a, ")")) {
            /* Nested unquoted close bracket */
            if (put_str(output, m4->token->a)) {

                fprintf(stderr, "%s:%d: Error\n", __FILE__, __LINE__);
                goto error;
            }

            --m4->stack->bracket_depth;
        } else {
            e = lookup(m4->ht, m4->token->a);
            if (e == NULL) {
                /* Not a macro, so pass through */
                if (put_str(output, m4->token->a)) {

                    fprintf(stderr, "%s:%d: Error\n", __FILE__, __LINE__);
                    goto error;
                }
            } else {
                /*  Macro */

                if (stack_mc(&m4->stack)) {

                    fprintf(stderr, "%s:%d: Error\n", __FILE__, __LINE__);
                    goto error;
                }

                /*
                 * Place a \0 char at the start of the new macro section in
                 * the store. This is to terminate the partially collected,
                 * in-progress, argument of the previous macro, so that
                 * dump_stack reports the correct information.
                 */
                if (put_ch(m4->store, '\0')) {

                    fprintf(stderr, "%s:%d: Error\n", __FILE__, __LINE__);
                    goto error;
                }

                m4->stack->bracket_depth = 1;
                m4->stack->mfp = e->func_p;
                /* One more than the start of the macro section in store */
                m4->stack->def_i = m4->store->i;
                if (e->def != NULL && put_str(m4->store, e->def)) {

                    fprintf(stderr, "%s:%d: Error\n", __FILE__, __LINE__);
                    goto error;
                }

                if (put_ch(m4->store, '\0')) {

                    fprintf(stderr, "%s:%d: Error\n", __FILE__, __LINE__);
                    goto error;
                }

                m4->stack->arg_i[m4->stack->active_arg] = m4->store->i;
                if (put_str(m4->store, e->name)) {

                    fprintf(stderr, "%s:%d: Error\n", __FILE__, __LINE__);
                    goto error;
                }

                if (put_ch(m4->store, '\0')) {

                    fprintf(stderr, "%s:%d: Error\n", __FILE__, __LINE__);
                    goto error;
                }

                /* See if called with or without brackets */
                if ((r =
                     eat_str_if_match(m4->input, "(",
                                      m4->read_stdin)) == ERR) {

                    fprintf(stderr, "%s:%d: Error\n", __FILE__, __LINE__);
                    goto error;
                }

                if (r == NO_MATCH) {
                    /* Called without arguments */
                    if ((mrv = end_macro(m4))) {
                        ret = mrv;      /* Save for exit time */
                        if (mrv == ERR || m4->error_exit) {
                            fprintf(stderr, "%s:%d: Error\n", __FILE__,
                                    __LINE__);
                            goto error;
                        }
                    }
                } else {
                    ++m4->stack->active_arg;
                    m4->stack->arg_i[m4->stack->active_arg] = m4->store->i;

                    /* Ready to collect arg 1 */
                    if (eat_whitespace(m4->input, m4->read_stdin)) {

                        fprintf(stderr, "%s:%d: Error\n", __FILE__,
                                __LINE__);
                        goto error;
                    }
                }
            }
        }
    }

    if (req_exit_val == -1) {
        /* Check */
        if (m4->stack != NULL) {
            fprintf(stderr, "m4: Stack not completed\n");
            ret = ERR;
        }
        if (m4->quote_depth) {
            fprintf(stderr, "m4: Quotes not balanced\n");
            ret = ERR;
        }
    }

    for (i = 0; i < NUM_DIVS - 1; ++i)
        flush_obuf(m4->div[i]);

  clean_up:
    if (ret == ERR) {
        fprintf(stderr, "error_exit: %d\n", m4->error_exit);
        fprintf(stderr, "left_quote: %s\n", m4->left_quote);
        fprintf(stderr, "right_quote: %s\n", m4->right_quote);
        dump_stack(m4);
    }

    free_m4(m4);

    if (req_exit_val != -1 && !ret)
        return req_exit_val;

    return ret;

  error:
    if (!ret)
        ret = ERR;

    goto clean_up;
}
