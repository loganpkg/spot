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

/*
 * Macros can collect any number of arguments, but only args 0 to 9
 * can be referenced. Arg 0 is the macro name.
 */
#define NUM_ARGS 10


#define INIT_BUF_SIZE 512

#define DEFAULT_LEFT_QUOTE "`"
#define DEFAULT_RIGHT_QUOTE "'"

/*
 * Do not change.
 * Diversion 0 continuously flushes to stdout.
 * Diversion -1 is index 10 which is continuously discarded.
 */
#define NUM_DIVS 11


/*
 * When there is no stack, the output will be the active diversion.
 * Otherwise, during argument collection, the output will be the store buffer.
 * (Definitions and argument strings in the store are referenced by the stack).
 */
#define output (m4->stack == NULL ? m4->div[m4->active_div] : m4->store)

#define m_def (m4->store->a + *(m4->str_start->a + m4->stack->m_i))
#define arg(n) (m4->store->a + *(m4->str_start->a + m4->stack->m_i + 1 + n))

#define num_args_collected (m4->str_start->i - (m4->stack->m_i + 2))

struct macro_call {
    Fptr mfp;
    size_t m_i;                 /* Index into str_start */
    size_t bracket_depth;       /* Depth of unquoted brackets */
    struct macro_call *next;    /* For nested macro calls */
};


typedef struct m4_info *M4ptr;

struct m4_info {
    int req_exit_val;           /* User requested exit value */
    struct ht *ht;
    /* There is only one input. Characters are stored in reverse order. */
    struct ibuf *input;
    struct obuf *token;
    /*
     * store:
     * def, macro name, arg 1, arg 2, def, macro name, arg 1, arg 2, ...
     * ^    ^           ^      ^      ^    ^           ^      ^
     * |    |           |      |      |    |           |      |
     * str_start
     * ^                              ^
     * |                              |
     * m_i                            m_i
     */
    struct obuf *store;
    struct sbuf *str_start;     /* Indices to the start of strings in store */
    struct macro_call *stack;
    struct obuf *tmp;           /* Used for substituting arguments */
    struct obuf *div[NUM_DIVS];
    size_t active_div;
    char *left_quote;
    char *right_quote;
    size_t quote_depth;
    /* Exit upon user related error. Turned off by default. */
    int error_exit;
    int trace;
    int help;                   /* Print help information for a macro */
};


struct macro_call *init_mc(void)
{
    struct macro_call *mc;

    if ((mc = calloc(1, sizeof(struct macro_call))) == NULL) {
        fprintf(stderr, "%s:%d: Error\n", __FILE__, __LINE__);
        return NULL;
    }

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
    p = m_def;

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
                if (x > num_args_collected) {
                    fprintf(stderr, "%s:%d: %s: Usage error: "
                            "Uncollected argument number %lu accessed\n",
                            __FILE__, __LINE__, arg(0), x);
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

    if (x_max != num_args_collected) {
        fprintf(stderr, "%s:%d: %s: Usage error: "
                "Too many arguments collected: Expected: %lu, Received: %lu\n",
                __FILE__, __LINE__, arg(0), x_max, num_args_collected);
        return USAGE_ERR;
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
            fprintf(stderr, "%s:%lu: %s: Failed\n", m4->input->nm,
                    m4->input->rn, arg(0));
    } else {
        ret = sub_args(m4);
    }

    /* Truncate */
    m4->str_start->i = m4->stack->m_i;
    m4->store->i = *(m4->str_start->a + m4->str_start->i);

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
        free_sbuf(m4->str_start);
        free_mc_stack(&m4->stack);
        free_obuf(m4->tmp);
        for (i = 0; i < NUM_DIVS; ++i)
            free_obuf(m4->div[i]);

        free(m4->left_quote);
        free(m4->right_quote);

        free(m4);
    }
}

M4ptr init_m4(int read_stdin)
{
    M4ptr m4;
    size_t i;

    if ((m4 = calloc(1, sizeof(struct m4_info))) == NULL) {
        fprintf(stderr, "%s:%d: Error\n", __FILE__, __LINE__);
        return NULL;
    }

    m4->req_exit_val = -1;

    for (i = 0; i < NUM_DIVS; ++i)
        m4->div[i] = NULL;

    if ((m4->ht = init_ht(NUM_BUCKETS)) == NULL)
        mgoto(error);

    if ((m4->input = init_ibuf(INIT_BUF_SIZE, read_stdin)) == NULL)
        mgoto(error);

    if ((m4->token = init_obuf(INIT_BUF_SIZE)) == NULL)
        mgoto(error);

    if ((m4->store = init_obuf(INIT_BUF_SIZE)) == NULL)
        mgoto(error);

    if ((m4->str_start = init_sbuf(INIT_BUF_SIZE)) == NULL)
        mgoto(error);

    if ((m4->tmp = init_obuf(INIT_BUF_SIZE)) == NULL)
        mgoto(error);

    for (i = 0; i < NUM_DIVS; ++i)
        if ((m4->div[i] = init_obuf(INIT_BUF_SIZE)) == NULL)
            mgoto(error);

    if ((m4->left_quote = strdup(DEFAULT_LEFT_QUOTE)) == NULL)
        mgoto(error);

    if ((m4->right_quote = strdup(DEFAULT_RIGHT_QUOTE)) == NULL)
        mgoto(error);

    return m4;

  error:
    free_m4(m4);
    fprintf(stderr, "%s:%d: Error\n", __FILE__, __LINE__);
    return NULL;
}

void dump_stack(M4ptr m4)
{
    struct macro_call *t;
    size_t i, num_arg, j;

    t = m4->stack;
    i = m4->str_start->i;

    fprintf(stderr, "Stack dump:\n");

    while (t != NULL) {
        num_arg = i - (t->m_i + 2);
        fprintf(stderr, "%s macro:\n",
                t->mfp == NULL ? "User-defined" : "Built-in");
        fprintf(stderr, "Bracket depth: %lu\n", t->bracket_depth);
        fprintf(stderr, "Def: %s\n",
                m4->store->a + *(m4->str_start->a + t->m_i));
        fprintf(stderr, "Macro: %s\n",
                m4->store->a + *(m4->str_start->a + t->m_i + 1));
        for (j = 1; j <= num_arg; ++j)
            fprintf(stderr, "Arg %lu: %s\n", j,
                    m4->store->a + *(m4->str_start->a + t->m_i + 1 + j));

        i = t->m_i;
        t = t->next;
    }
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
        fprintf(stderr, "%s: %s%s\n", "built-in", esf(NM), par_desc);   \
        return 0;                                                       \
    }                                                                   \
    if (num_args_collected != num_pars) {                               \
        fprintf(stderr, "%s:%lu [%s:%d]: Usage: %s%s\n", m4->input->nm, \
            m4->input->rn, __FILE__, __LINE__, esf(NM), par_desc);      \
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

    usage(2, "(`macro_name', `macro_def')");

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

    usage(1, "(`macro_name')");

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

    usage(2, "(left_quote, right_quote)");

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

    usage(1, "(div_num)");

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

    usage(1, "(div_num_or_filename)");

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
    int append = 0;
    char ch;

    usage(3, "(div_num, filename, append)");

    if (!strcmp(arg(3), "1"))
        append = 1;

    ch = *arg(1);


    /* Cannot write diversions 0 and -1 */
    if (strlen(arg(1)) == 1 && isdigit(ch) && ch != '0') {
        if (write_obuf(m4->div[ch - '0'], arg(2), append)) {
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

    usage(1, "(filename)");

    if (unget_file(&m4->input, arg(1))) {
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

    return delete_to_nl(&m4->input);
}

#undef NM
#define NM tnl

int NM(void *v)
{
    M4ptr m4 = (M4ptr) v;
    char *p, *q, ch;

    usage(1, "(str)");

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
    int nl_sen = 0;             /* Newline sensitive (off) */
    int verbose = 0;            /* Prints information about the regex */

    usage(5, "(text, regex_find, replace, newline_sensitive, verbose)");

    if (!strcmp(arg(4), "1"))
        nl_sen = 1;             /* Newline sensitive on */

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

    usage(1, "(dir_name)");

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

    usage(3, "(`macro_name', `when_defined', `when_undefined')");

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
    char *par_desc;
    size_t i;

    par_desc =
        "(switch, case_a, `when_a', case_b, `when_b', ... , `default')";

    if (m4->help) {
        fprintf(stderr, "%s: %s%s\n", "built-in", esf(NM), par_desc);
        return 0;
    }

    if (!(num_args_collected >= 4 && num_args_collected % 2 == 0)) {
        fprintf(stderr, "%s:%lu [%s:%d]: Usage: %s%s\n", m4->input->nm,
                m4->input->rn, __FILE__, __LINE__, esf(NM), par_desc);
        return USAGE_ERR;
    }

    for (i = 2; i <= num_args_collected - 2; i += 2)
        if (!strcmp(arg(1), arg(i))) {
            if (unget_str(m4->input, arg(i + 1))) {
                fprintf(stderr, "%s:%d: Error\n", __FILE__, __LINE__);
                return ERR;
            }
            return 0;
        }

    /* Default */
    if (unget_str(m4->input, arg(num_args_collected))) {
        fprintf(stderr, "%s:%d: Error\n", __FILE__, __LINE__);
        return ERR;
    }

    return 0;
}

#undef NM
#define NM dumpdef

int NM(void *v)
{
    M4ptr m4 = (M4ptr) v;
    int ret;
    char *par_desc;
    size_t i;
    struct entry *e;

    par_desc = "(`macro_name', ...)";

    if (m4->help) {
        fprintf(stderr, "%s: %s%s\n", "built-in", esf(NM), par_desc);
        return 0;
    }

    if (num_args_collected < 1) {
        fprintf(stderr, "%s:%lu [%s:%d]: Usage: %s%s\n", m4->input->nm,
                m4->input->rn, __FILE__, __LINE__, esf(NM), par_desc);
        return USAGE_ERR;
    }

    m4->help = 1;

    for (i = 1; i <= num_args_collected; ++i) {
        if (*arg(i) == '\0') {
            fprintf(stderr, "%s:%d:%s: Usage error: "
                    "Argument is empty string\n", __FILE__, __LINE__,
                    esf(NM));
            m4->help = 0;
            return USAGE_ERR;
        }

        e = lookup(m4->ht, arg(i));
        if (e == NULL) {
            fprintf(stderr, "Undefined: %s\n", arg(i));
        } else {
            if (e->func_p == NULL) {
                fprintf(stderr, "User-def: %s: %s\n", e->name, e->def);
            } else {
                if ((ret = (*e->func_p) (m4))) {
                    m4->help = 0;
                    return ret;
                }
            }
        }
    }

    m4->help = 0;

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

    usage(1, "(error_message)");

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

    usage(1, "(number)");

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

    usage(2, "(arithmetic_expression, verbose)");

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

    if (unget_str(m4->input, m_def)) {
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

    usage(1, "(shell_command)");

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

    usage(1, "(exit_value)");

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
#define NM traceon

int NM(void *v)
{
    M4ptr m4 = (M4ptr) v;

    usage(0, "");

    m4->trace = 1;

    return 0;
}

#undef NM
#define NM traceoff

int NM(void *v)
{
    M4ptr m4 = (M4ptr) v;

    usage(0, "");

    m4->trace = 0;

    return 0;
}

#undef NM
#define NM recrm

int NM(void *v)
{
    M4ptr m4 = (M4ptr) v;

    usage(1, "(file_path)");

    if (*arg(1) == '\0') {
        fprintf(stderr, "%s:%d:%s: Usage error: "
                "Argument is empty string\n", __FILE__, __LINE__, esf(NM));
        return USAGE_ERR;
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

    if ((m4 = init_m4(argc > 1 ? 0 : 1)) == NULL)
        mgoto(error);

    if (argc > 1)
        for (i = argc - 1; i >= 1; --i)
            if (unget_file(&m4->input, *(argv + i)))
                mgoto(error);

/* Load built-in macros */
#define load_bi(m) if (upsert(m4->ht, #m, NULL, &m)) \
    mgoto(error)

    load_bi(define);
    load_bi(undefine);
    load_bi(changequote);
    load_bi(divert);
    load_bi(undivert);
    load_bi(writediv);
    load_bi(divnum);
    load_bi(include);
    load_bi(dnl);
    load_bi(tnl);
    load_bi(regexrep);
    load_bi(lsdir);
    load_bi(ifdef);
    load_bi(ifelse);
    load_bi(dumpdef);
    load_bi(dumpdefall);
    load_bi(errprint);
    load_bi(incr);
    load_bi(evalmath);
    load_bi(esyscmd);
    load_bi(sysval);
    load_bi(m4exit);
    load_bi(errok);
    load_bi(errexit);
    load_bi(traceon);
    load_bi(traceoff);
    load_bi(recrm);

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

        if (flush_obuf(m4->div[0]))
            mgoto(error);

        m4->div[NUM_ARGS]->i = 0;

        r = eat_str_if_match(&m4->input, m4->left_quote);
        if (r == ERR)
            mgoto(error);

        if (r == MATCH) {
            if (m4->quote_depth && put_str(output, m4->left_quote))
                mgoto(error);

            ++m4->quote_depth;
            /* As might have multiple quotes in a row */
            goto top;
        }

        r = eat_str_if_match(&m4->input, m4->right_quote);
        if (r == ERR)
            mgoto(error);

        if (r == MATCH) {
            if (m4->quote_depth != 1 && put_str(output, m4->right_quote))
                mgoto(error);

            if (m4->quote_depth)
                --m4->quote_depth;

            /* As might have multiple quotes in a row */
            goto top;
        }

        /* Not a quote, so read a token */
        r = get_word(&m4->input, m4->token);
        if (r == ERR)
            mgoto(error);
        else if (r == EOF)
            break;

        if (m4->quote_depth) {
            /* Quoted */
            if (put_str(output, m4->token->a))
                mgoto(error);
        } else if (m4->stack != NULL && m4->stack->bracket_depth == 1
                   && !strcmp(m4->token->a, ",")) {
            if (put_ch(output, '\0'))
                mgoto(error);

            if (add_s(m4->str_start, m4->store->i))
                mgoto(error);

            /* Argument separator */
            if (eat_whitespace(&m4->input))
                mgoto(error);
        } else if (m4->stack != NULL && m4->stack->bracket_depth == 1
                   && !strcmp(m4->token->a, ")")) {
            /* End of argument collection */
            if (put_ch(output, '\0'))
                mgoto(error);

            if ((mrv = end_macro(m4))) {
                ret = mrv;      /* Save for exit time */
                if ((mrv == ERR || m4->error_exit)) {
                    fprintf(stderr, "%s:%lu [%s:%d]: Error\n",
                            m4->input->nm, m4->input->rn, __FILE__,
                            __LINE__);
                    goto error;
                }
            }
        } else if (m4->stack != NULL && !strcmp(m4->token->a, "(")) {
            /* Nested unquoted open bracket */
            if (put_str(output, m4->token->a))
                mgoto(error);

            ++m4->stack->bracket_depth;
        } else if (m4->stack != NULL && m4->stack->bracket_depth > 1
                   && !strcmp(m4->token->a, ")")) {
            /* Nested unquoted close bracket */
            if (put_str(output, m4->token->a))
                mgoto(error);

            --m4->stack->bracket_depth;
        } else {
            e = lookup(m4->ht, m4->token->a);
            if (e == NULL) {
                /* Not a macro, so pass through */
                if (put_str(output, m4->token->a))
                    mgoto(error);
            } else {
                /*  Macro */

                if (stack_mc(&m4->stack))
                    mgoto(error);

                m4->stack->bracket_depth = 1;
                m4->stack->mfp = e->func_p;

                m4->stack->m_i = m4->str_start->i;

                if (add_s(m4->str_start, m4->store->i))
                    mgoto(error);

                if (e->def != NULL && put_str(m4->store, e->def))
                    mgoto(error);

                if (put_ch(m4->store, '\0'))
                    mgoto(error);

                if (add_s(m4->str_start, m4->store->i))
                    mgoto(error);

                if (put_str(m4->store, e->name))
                    mgoto(error);

                if (put_ch(m4->store, '\0'))
                    mgoto(error);

                if (m4->trace)
                    fprintf(stderr, "Trace: %s:%lu: %s\n", m4->input->nm,
                            m4->input->rn, e->name);

                /* See if called with or without brackets */
                if ((r = eat_str_if_match(&m4->input, "(")) == ERR)
                    mgoto(error);

                if (r == NO_MATCH) {
                    /* Called without arguments */
                    if ((mrv = end_macro(m4))) {
                        ret = mrv;      /* Save for exit time */
                        if (mrv == ERR || m4->error_exit)
                            mgoto(error);
                    }
                } else {
                    if (add_s(m4->str_start, m4->store->i))
                        mgoto(error);

                    /* Ready to collect arg 1 */
                    if (eat_whitespace(&m4->input))
                        mgoto(error);
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
    if (ret) {
        fprintf(stderr, "Error mode: %s\n",
                m4->error_exit ? "Error exit" : "Error OK");
        fprintf(stderr, "Left quote: %s\n", m4->left_quote);
        fprintf(stderr, "Right quote: %s\n", m4->right_quote);
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
