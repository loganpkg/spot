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

#define DEFAULT_LEFT_COMMENT "#"
#define DEFAULT_RIGHT_COMMENT "\n"
#define DEFAULT_LEFT_QUOTE "`"
#define DEFAULT_RIGHT_QUOTE "'"

/*
 * Do not change.
 * Diversion 0 continuously flushes to stdout.
 * Diversion -1 is index 10 which is continuously discarded.
 */
#define NUM_DIVS 11
#define DIVERSION_NEGATIVE_1 10

/*
 * When there is no stack, the output will be the active diversion.
 * Otherwise, during argument collection, the output will be the store buffer.
 * (Definitions and argument strings in the store are referenced by the stack).
 */
#define output (m4->stack == NULL ? m4->div[m4->active_div] : m4->store)

#define num_args_collected (m4->str_start->i - (m4->stack->m_i + 2))

#define m_def (m4->store->a + *(m4->str_start->a + m4->stack->m_i))

/*
#define m_name (m4->store->a + *(m4->str_start->a + m4->stack->m_i + 1))

#define arg(n) ((n) <= num_args_collected ? \
    m4->store->a + *(m4->str_start->a + m4->stack->m_i + 1 + (n)) : "")
*/

#define arg(n) (m4->store->a + *(m4->str_start->a + m4->stack->m_i + 1 + (n)))


struct macro_call {
    Fptr mfp;                   /* Macro file pointer (built-ins) */
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
    struct macro_call *stack;   /* Head node of the macro call stack */
    size_t stack_depth;         /* For trace */
    Fptr tmp_mfp;               /* For passing back the defn of a built-in */
    /* Used for substituting arguments and for esyscmd and translit */
    struct obuf *tmp;
    struct obuf *wrap;          /* Used for m4wrap */
    struct obuf *div[NUM_DIVS];
    size_t active_div;
    char *left_comment;
    char *right_comment;
    size_t comment_on;
    char *left_quote;
    char *right_quote;
    size_t quote_depth;
    /*
     * Pass through the name of a built-in macro to output when called without
     * arguments. Otherwise an infinite loop would occur if the name was placed
     * back in the input.
     */
    int pass_through;
    /*
     * Delayed copy of the file pointer from inside the input. Used to detect
     * file pointer changes to generate #line directives.
     */
    FILE *sticky_fp;
    int line_direct;            /* Print #line directives for C preprocessor */
    int sys_val;                /* Return value of last syscmd or esyscmd */
    /* Exit upon user related error. Turned off by default. */
    int error_exit;             /* Exit upon the first error */
    int warn_to_error;          /* Treat warnings as errors */
    int trace;
    int help;                   /* Print help information for a macro */
};

/* Used for translit */
struct range {
    unsigned char on;
    unsigned char i;
    unsigned char stop;         /* Inclusive */
    unsigned char decr;
};

void set_range(char **str, struct range *r)
{
    char *p;

    p = *str;
    if (*p != '\0' && *(p + 1) == '-' && *(p + 2) != '\0') {
        /* Range */
        r->on = 1;
        r->i = *p;
        r->stop = *(p + 2);
        if (r->stop < r->i)
            r->decr = 1;
        else
            r->decr = 0;

        /* Eat */
        p += 3;
        *str = p;
    }
}

char read_range_ch(char **str, struct range *r)
{
    char *p;
    char ch;

    if (!r->on)
        set_range(str, r);

    p = *str;
    if (r->on) {
        ch = r->i;
        if (r->i == r->stop) {
            r->on = 0;
        } else {
            if (r->decr)
                --r->i;
            else
                ++r->i;
        }
    } else {
        ch = *p;
        if (ch != '\0')
            ++p;
    }

    *str = p;
    return ch;
}

struct macro_call *init_mc(void)
{
    struct macro_call *mc;

    if ((mc = calloc(1, sizeof(struct macro_call))) == NULL) {
        fprintf(stderr, "%s:%d: Error\n", __FILE__, __LINE__);
        return NULL;
    }

    return mc;
}

int stack_mc(struct macro_call **head, size_t *stack_depth)
{
    struct macro_call *mc;

    if ((mc = init_mc()) == NULL) {
        fprintf(stderr, "%s:%d: Error\n", __FILE__, __LINE__);
        return ERR;
    }

    mc->next = *head;
    *head = mc;
    ++*stack_depth;
    return 0;
}

void pop_mc(struct macro_call **head, size_t * stack_depth)
{
    struct macro_call *mc;

    if (*head != NULL) {
        mc = (*head)->next;
        free(*head);
        *head = mc;
        --*stack_depth;
    }
}

void free_mc_stack(struct macro_call **head)
{
    size_t sd;
    if (head != NULL)
        while (*head != NULL)
            pop_mc(head, &sd);
}

int sub_args(M4ptr m4)
{
    const char *p;
    char ch, next_ch;
    size_t x, i;
    char num[NUM_BUF_SIZE];
    int r;
    int all_args_accessed = 0;
    char accessed[NUM_ARGS];
    memset(accessed, 'N', NUM_ARGS);


    m4->tmp->i = 0;
    p = m_def;

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
                /* $0 is the macro name. $1 to $9 are the collected args. */
                x = next_ch - '0';
                accessed[x] = 'Y';
                /* Can only access args that were collected */
                if (x > num_args_collected) {
                    fprintf(stderr, "%s:%d: %s: Usage warning: "
                            "Uncollected argument number %lu accessed\n",
                            __FILE__, __LINE__, arg(0), (unsigned long) x);
                    if (m4->warn_to_error)
                        return USAGE_ERR;
                }
                if (put_str(m4->tmp, arg(x))) {
                    fprintf(stderr, "%s:%d: Error\n", __FILE__, __LINE__);
                    return ERR;
                }
                ++p;            /* Eat an extra char */
            } else if (next_ch == '#') {
                /* $# is the number arguments collected */
                r = snprintf(num, NUM_BUF_SIZE, "%lu",
                             (unsigned long) num_args_collected);
                if (r < 0 || r >= NUM_BUF_SIZE) {
                    fprintf(stderr, "%s:%d: Error\n", __FILE__, __LINE__);
                    return ERR;
                }
                if (put_str(m4->tmp, num)) {
                    fprintf(stderr, "%s:%d: Error\n", __FILE__, __LINE__);
                    return ERR;
                }
                ++p;            /* Eat an extra char */
            } else if (next_ch == '*' || next_ch == '@') {
                /*
                 * $* is all arguments, comma separated.
                 * $@ is the same, but the individual arguments are quoted.
                 */
                all_args_accessed = 1;
                for (i = 1; i <= num_args_collected; ++i) {
                    if (next_ch == '@' && put_str(m4->tmp, m4->left_quote)) {
                        fprintf(stderr, "%s:%d: Error\n", __FILE__,
                                __LINE__);
                        return ERR;
                    }
                    if (put_str(m4->tmp, arg(i))) {
                        fprintf(stderr, "%s:%d: Error\n", __FILE__,
                                __LINE__);
                        return ERR;
                    }
                    if (next_ch == '@'
                        && put_str(m4->tmp, m4->right_quote)) {
                        fprintf(stderr, "%s:%d: Error\n", __FILE__,
                                __LINE__);
                        return ERR;
                    }
                    if (i != num_args_collected && put_ch(m4->tmp, ',')) {
                        fprintf(stderr, "%s:%d: Error\n", __FILE__,
                                __LINE__);
                        return ERR;
                    }
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

    if (!all_args_accessed)
        for (i = 1; i <= num_args_collected; ++i)
            if ((i < NUM_ARGS && accessed[i] == 'N') || i >= NUM_ARGS) {
                fprintf(stderr, "%s:%d: %s: Usage warning: "
                        "Collected argument number %lu not accessed\n",
                        __FILE__, __LINE__, arg(0), (unsigned long) i);
                if (m4->warn_to_error)
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
    char *nm;
    if (m4->stack->mfp != NULL) {
        if ((ret = (*m4->stack->mfp) (m4)))
            fprintf(stderr, "%s:%lu: %s: Failed\n", m4->input->nm,
                    (unsigned long) m4->input->rn, arg(0));
    } else {
        ret = sub_args(m4);
    }

    /* Truncate */
    m4->str_start->i = m4->stack->m_i;
    m4->store->i = *(m4->str_start->a + m4->str_start->i);

    /* Store the marco name */
    nm = arg(0);

    /* Pop redirectes output to the next node (if any) */
    pop_mc(&m4->stack, &m4->stack_depth);

    if (m4->pass_through) {
        if (put_str(output, nm))
            return ERR;

        m4->pass_through = 0;
    }

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
        free_obuf(m4->wrap);
        for (i = 0; i < NUM_DIVS; ++i)
            free_obuf(m4->div[i]);

        free(m4->left_comment);
        free(m4->right_comment);
        free(m4->left_quote);
        free(m4->right_quote);

        free(m4);
    }
}

M4ptr init_m4(void)
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

    if ((m4->token = init_obuf(INIT_BUF_SIZE)) == NULL)
        mgoto(error);

    if ((m4->store = init_obuf(INIT_BUF_SIZE)) == NULL)
        mgoto(error);

    if ((m4->str_start = init_sbuf(INIT_BUF_SIZE)) == NULL)
        mgoto(error);

    if ((m4->tmp = init_obuf(INIT_BUF_SIZE)) == NULL)
        mgoto(error);

    if ((m4->wrap = init_obuf(INIT_BUF_SIZE)) == NULL)
        mgoto(error);

    for (i = 0; i < NUM_DIVS; ++i)
        if ((m4->div[i] = init_obuf(INIT_BUF_SIZE)) == NULL)
            mgoto(error);

    if ((m4->left_comment = strdup(DEFAULT_LEFT_COMMENT)) == NULL)
        mgoto(error);

    if ((m4->right_comment = strdup(DEFAULT_RIGHT_COMMENT)) == NULL)
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
        fprintf(stderr, "Bracket depth: %lu\n",
                (unsigned long) t->bracket_depth);
        fprintf(stderr, "Def: %s\n",
                m4->store->a + *(m4->str_start->a + t->m_i));
        fprintf(stderr, "Macro: %s\n",
                m4->store->a + *(m4->str_start->a + t->m_i + 1));
        for (j = 1; j <= num_arg; ++j)
            fprintf(stderr, "Arg %lu: %s\n", (unsigned long) j,
                    m4->store->a + *(m4->str_start->a + t->m_i + 1 + j));

        i = t->m_i;
        t = t->next;
    }
}

int validate_quote_or_comment(M4ptr m4, const char *quote_or_comment)
{
    size_t i;
    char ch;

    i = 0;
    while (1) {
        ch = *(quote_or_comment + i);
        if (ch == '\0')
            break;

        /* All chars should be graph non-comma and non-parentheses */
        if (!isgraph(ch) || ch == ',' || ch == '(' || ch == ')') {
            fprintf(stderr, "%s:%lu [%s:%d]: %s: Usage warning: "
                    "All characters in a quote or comment string "
                    "should be graph non-comma and non-parentheses: %s\n",
                    m4->input->nm, (unsigned long) m4->input->rn, __FILE__,
                    __LINE__, arg(0), arg(1));
            if (m4->warn_to_error)
                return USAGE_ERR;

            break;
        }

        ++i;
    }

    return 0;
}

int validate_def(const char *def)
{
    const char *p;
    char ch, next_ch;
    size_t x, i;
    char present[NUM_ARGS];
    memset(present, 'N', NUM_ARGS);
    present[0] = 'Y';           /* Macro name is always present */

    if (def == NULL)
        return 0;               /* OK */

    p = def;
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
        if (i && present[i] == 'Y' && present[i - 1] == 'N')
            return 1;

    return 0;
}

int add_macro(M4ptr m4, char *macro_name, char *macro_def, int push_hist)
{
    /*
     * Adds a user-defined macro.
     * Need to check macro name fully as it might have been passed in on the
     * command line.
     */
    char *p, ch;

    p = macro_name;
    /* Check first character */
    if (!isalpha(*p) && *p != '_') {
        fprintf(stderr,
                "%s:%d:%s: Syntax error: "
                "Invalid macro name\n", __FILE__, __LINE__, macro_name);
        return SYNTAX_ERR;
    }
    /* Check remaining characters */
    ++p;
    while ((ch = *p) != '\0') {
        if (!isalnum(ch)) {
            fprintf(stderr,
                    "%s:%d:%s: Syntax error: "
                    "Invalid macro name\n", __FILE__, __LINE__,
                    macro_name);
            return SYNTAX_ERR;
        }
        ++p;
    }

    if (*macro_def == '\0' && m4->tmp_mfp != NULL) {
        /* Passed back built-in macro function pointer from defn */
        if (upsert(m4->ht, macro_name, NULL, m4->tmp_mfp, push_hist)) {
            fprintf(stderr, "%s:%d: Error\n", __FILE__, __LINE__);
            return ERR;
        }
        m4->tmp_mfp = NULL;
    } else {
        /* User-defined text macro */
        if (validate_def(macro_def)) {
            fprintf(stderr,
                    "%s:%d:%s: Syntax warning: "
                    "Macro definition has gaps in argument references\n",
                    __FILE__, __LINE__, macro_name);
            if (m4->warn_to_error)
                return SYNTAX_ERR;
        }

        if (upsert(m4->ht, macro_name, macro_def, NULL, push_hist)) {
            fprintf(stderr, "%s:%d: Error\n", __FILE__, __LINE__);
            return ERR;
        }
    }

    return 0;
}

int output_line_directive(M4ptr m4)
{
    /*
     * #line directives are tricky. They need to be printed whenever the
     * underlying file pointer in the input changes. However, they can only be
     * printed when the output is at the start of the line. So this then
     * becomes related to the flushing of diversion 0. The easy solution is to
     * only flush upon newline, so that an empty buffer indicates the
     * start of the line.
     * The #line directives also state what is to come. So they need to give
     * information about the next read, not the current state. Furthermore,
     * the default right comment token is \n. So \n might be intercepted by
     * the comment checking, and thus, it is not a good idea to use the input
     * as a flush trigger.
     * Hence, #line directives need to be processed (this function called)
     * after the input is read, but before the output is written.
     */
    char num[NUM_BUF_SIZE];
    int r;

    /* Output at start of line and the file pointer has changed */
    if (m4->line_direct
        && (!output->i || *(output->a + output->i - 1) == '\n')
        && m4->sticky_fp != m4->input->fp) {
        r = snprintf(num, NUM_BUF_SIZE, "%lu",
                     (unsigned long) m4->input->rn);
        if (r < 0 || r >= NUM_BUF_SIZE)
            mgoto(error);

        if (put_str(output, "#line "))
            mgoto(error);

        if (put_str(output, num))
            mgoto(error);

        if (put_str(output, " \""))
            mgoto(error);

        if (put_str(output, m4->input->nm))
            mgoto(error);

        if (put_str(output, "\"\n"))
            mgoto(error);

        m4->sticky_fp = m4->input->fp;
    }

    return 0;

  error:
    return ERR;
}


#define print_help if (m4->help) {          \
        fprintf(stderr, "%s\n", PAR_DESC);  \
        return 0;                           \
}

#define allow_pass_through if (!num_args_collected) {   \
    m4->pass_through = 1;                               \
    return 0;                                           \
}

#define max_pars(n) if (num_args_collected > n) {       \
    fprintf(stderr, "%s:%lu [%s:%d]: Usage warning: "   \
        "Unused arguments collected: %s%s\n",           \
        m4->input->nm, (unsigned long) m4->input->rn,   \
        __FILE__, __LINE__, arg(0), PAR_DESC);          \
    if (m4->warn_to_error)                              \
        return USAGE_ERR;                               \
}                                                       \

#define min_pars(n) if (num_args_collected < n) {       \
    fprintf(stderr, "%s:%lu [%s:%d]: Usage Error: "     \
        "Required arguments not collected: %s%s\n",     \
        m4->input->nm, (unsigned long) m4->input->rn,   \
        __FILE__, __LINE__, arg(0), PAR_DESC);          \
    return USAGE_ERR;                                   \
}                                                       \


/* ********** Built-in macros ********** */

/* See README.md for the syntax of the built-in macros */

#define NM define
#define PAR_DESC "(macro_name, macro_def)"

int econc(m4_, NM) (void *v) {
    M4ptr m4 = (M4ptr) v;

    print_help;
    allow_pass_through;
    max_pars(2);
    min_pars(2);

    return add_macro(m4, arg(1), arg(2), 0);

    return 0;
}

#undef NM
#undef PAR_DESC
#define NM pushdef
#define PAR_DESC "(macro_name, macro_def)"

int econc(m4_, NM) (void *v) {
    M4ptr m4 = (M4ptr) v;

    print_help;
    allow_pass_through;
    max_pars(2);
    min_pars(2);

    return add_macro(m4, arg(1), arg(2), 1);

    return 0;
}

#undef NM
#undef PAR_DESC
#define NM undefine
#define PAR_DESC "(macro_name)"

int econc(m4_, NM) (void *v) {
    M4ptr m4 = (M4ptr) v;

    print_help;
    allow_pass_through;
    max_pars(1);
    min_pars(1);

    if (delete_entry(m4->ht, arg(1), 0)) {
        fprintf(stderr, "%s:%lu [%s:%d]: %s: Usage warning: "
                "Macro does not exist: %s\n", m4->input->nm,
                (unsigned long) m4->input->rn, __FILE__, __LINE__, arg(0),
                arg(1));
        if (m4->warn_to_error)
            return USAGE_ERR;
    }

    return 0;
}

#undef NM
#undef PAR_DESC
#define NM popdef
#define PAR_DESC "(macro_name)"

int econc(m4_, NM) (void *v) {
    M4ptr m4 = (M4ptr) v;

    print_help;
    allow_pass_through;
    max_pars(1);
    min_pars(1);

    /* Pop history */
    if (delete_entry(m4->ht, arg(1), 1)) {
        fprintf(stderr, "%s:%lu [%s:%d]: %s: Usage warning: "
                "Macro does not exist: %s\n", m4->input->nm,
                (unsigned long) m4->input->rn, __FILE__, __LINE__, arg(0),
                arg(1));
        if (m4->warn_to_error)
            return USAGE_ERR;
    }

    return 0;
}

#undef NM
#undef PAR_DESC
#define NM changecom
#define PAR_DESC "[(left_comment[, right_comment])]"

int econc(m4_, NM) (void *v) {
    M4ptr m4 = (M4ptr) v;
    char *lc, *rc;
    char *tmp_lc = NULL, *tmp_rc = NULL;

    print_help;
    max_pars(2);

    if (!num_args_collected) {
        /* Disable comments */
        free(m4->left_comment);
        m4->left_comment = NULL;
        free(m4->right_comment);
        m4->right_comment = NULL;
        return 0;
    }

    if (num_args_collected >= 1) {
        if (*arg(1) == '\0') {
            fprintf(stderr, "%s:%lu [%s:%d]: %s: Usage error: "
                    "Empty left comment\n", m4->input->nm,
                    (unsigned long) m4->input->rn, __FILE__, __LINE__,
                    arg(0));
            return USAGE_ERR;
        }
        if (validate_quote_or_comment(m4, arg(1))) {
            fprintf(stderr, "%s:%lu [%s:%d]: %s: Usage warning: "
                    "Poor choice of left comment: %s\n", m4->input->nm,
                    (unsigned long) m4->input->rn, __FILE__, __LINE__,
                    arg(0), arg(1));
            if (m4->warn_to_error)
                return USAGE_ERR;
        }

        lc = arg(1);
    }
    if (num_args_collected >= 2) {
        if (*arg(2) == '\0') {
            fprintf(stderr, "%s:%lu [%s:%d]: %s: Usage error: "
                    "Empty right comment\n", m4->input->nm,
                    (unsigned long) m4->input->rn, __FILE__, __LINE__,
                    arg(0));
            return USAGE_ERR;
        }
        if (validate_quote_or_comment(m4, arg(2))) {
            fprintf(stderr, "%s:%lu [%s:%d]: %s: Usage warning: "
                    "Poor choice of right comment: %s\n", m4->input->nm,
                    (unsigned long) m4->input->rn, __FILE__, __LINE__,
                    arg(0), arg(2));
            if (m4->warn_to_error)
                return USAGE_ERR;
        }
        rc = arg(2);
    } else {
        rc = DEFAULT_RIGHT_COMMENT;
    }

    /* Comments should not be the same */
    if (!strcmp(lc, rc)) {
        fprintf(stderr, "%s:%lu [%s:%d]: %s: Usage warning: "
                "Left and right comments should not be the same\n",
                m4->input->nm, (unsigned long) m4->input->rn, __FILE__,
                __LINE__, arg(0));
        if (m4->warn_to_error)
            return USAGE_ERR;
    }

    if ((tmp_lc = strdup(lc)) == NULL) {
        fprintf(stderr, "%s:%d: Error\n", __FILE__, __LINE__);
        return ERR;
    }

    if ((tmp_rc = strdup(rc)) == NULL) {
        fprintf(stderr, "%s:%d: Error\n", __FILE__, __LINE__);
        free(tmp_lc);
        return ERR;
    }

    free(m4->left_comment);
    m4->left_comment = tmp_lc;
    free(m4->right_comment);
    m4->right_comment = tmp_rc;

    return 0;
}

#undef NM
#undef PAR_DESC
#define NM changequote
#define PAR_DESC "[(left_quote, right_quote)]"

int econc(m4_, NM) (void *v) {
    M4ptr m4 = (M4ptr) v;
    char *lq, *rq;
    char *tmp_lq = NULL, *tmp_rq = NULL;

    print_help;
    max_pars(2);

    if (num_args_collected >= 2) {
        if (*arg(1) == '\0') {
            fprintf(stderr, "%s:%lu [%s:%d]: %s: Usage error: "
                    "Empty left quote\n", m4->input->nm,
                    (unsigned long) m4->input->rn, __FILE__, __LINE__,
                    arg(0));
            return USAGE_ERR;
        }
        if (validate_quote_or_comment(m4, arg(1))) {
            fprintf(stderr, "%s:%lu [%s:%d]: %s: Usage warning: "
                    "Poor choice of left quote: %s\n", m4->input->nm,
                    (unsigned long) m4->input->rn, __FILE__, __LINE__,
                    arg(0), arg(1));
            if (m4->warn_to_error)
                return USAGE_ERR;
        }
        if (*arg(2) == '\0') {
            fprintf(stderr, "%s:%lu [%s:%d]: %s: Usage error: "
                    "Empty right quote\n", m4->input->nm,
                    (unsigned long) m4->input->rn, __FILE__, __LINE__,
                    arg(0));
            return USAGE_ERR;
        }
        if (validate_quote_or_comment(m4, arg(2))) {
            fprintf(stderr, "%s:%lu [%s:%d]: %s: Usage warning: "
                    "Poor choice of right quote: %s\n", m4->input->nm,
                    (unsigned long) m4->input->rn, __FILE__, __LINE__,
                    arg(0), arg(2));
            if (m4->warn_to_error)
                return USAGE_ERR;
        }

        /* Quotes should not be the same */
        if (!strcmp(arg(1), arg(2))) {
            fprintf(stderr, "%s:%lu [%s:%d]: %s: Usage warning: "
                    "Left and right quotes should not be the same\n",
                    m4->input->nm, (unsigned long) m4->input->rn, __FILE__,
                    __LINE__, arg(0));
            if (m4->warn_to_error)
                return USAGE_ERR;
        }

        lq = arg(1);
        rq = arg(2);
    } else {
        lq = DEFAULT_LEFT_QUOTE;
        rq = DEFAULT_RIGHT_QUOTE;
    }

    if ((tmp_lq = strdup(lq)) == NULL) {
        fprintf(stderr, "%s:%d: Error\n", __FILE__, __LINE__);
        return ERR;
    }

    if ((tmp_rq = strdup(rq)) == NULL) {
        fprintf(stderr, "%s:%d: Error\n", __FILE__, __LINE__);
        free(tmp_lq);
        return ERR;
    }

    free(m4->left_quote);
    m4->left_quote = tmp_lq;
    free(m4->right_quote);
    m4->right_quote = tmp_rq;

    return 0;
}

#undef NM
#undef PAR_DESC
#define NM shift
#define PAR_DESC "(arg1[, ... ])"

int econc(m4_, NM) (void *v) {
    M4ptr m4 = (M4ptr) v;
    size_t i;

    print_help;

    if (!num_args_collected) {
        m4->pass_through = 1;
        return 0;
    }

    /*
     * $@ comma separated quoted args, except for the first.
     * Needs to be done in reverse as ungetting.
     */
    for (i = num_args_collected; i >= 2; --i) {
        if (unget_str(m4->input, m4->right_quote)) {
            fprintf(stderr, "%s:%d: Error\n", __FILE__, __LINE__);
            return ERR;
        }
        if (unget_str(m4->input, arg(i))) {
            fprintf(stderr, "%s:%d: Error\n", __FILE__, __LINE__);
            return ERR;
        }
        if (unget_str(m4->input, m4->left_quote)) {
            fprintf(stderr, "%s:%d: Error\n", __FILE__, __LINE__);
            return ERR;
        }
        if (i != 2 && unget_ch(m4->input, ',')) {
            fprintf(stderr, "%s:%d: Error\n", __FILE__, __LINE__);
            return ERR;
        }
    }

    return 0;
}

#undef NM
#undef PAR_DESC
#define NM divert
#define PAR_DESC "[(div_num)]"

int econc(m4_, NM) (void *v) {
    M4ptr m4 = (M4ptr) v;

    print_help;
    max_pars(1);

    if (num_args_collected >= 1) {
        if (!strcmp(arg(1), "-1")) {
            m4->active_div = 10;
            return 0;
        }
        if (strlen(arg(1)) == 1 && isdigit(*arg(1))) {
            m4->active_div = *arg(1) - '0';
            return 0;
        }
    } else {
        m4->active_div = 0;
        return 0;
    }

    fprintf(stderr, "%s:%d: Error\n", __FILE__, __LINE__);
    return ERR;
}

#undef NM
#undef PAR_DESC
#define NM undivert
#define PAR_DESC "[(div_num_or_filename)]"

int econc(m4_, NM) (void *v) {
    M4ptr m4 = (M4ptr) v;
    char ch, *p;
    size_t x, i;

    print_help;

    if (num_args_collected) {
        for (i = 1; i <= num_args_collected; ++i) {
            ch = *arg(i);
            if (ch == '\0') {
                fprintf(stderr, "%s:%d:%s: Usage error: "
                        "Argument is empty string\n", __FILE__, __LINE__,
                        arg(0));
                return USAGE_ERR;
            } else if (isdigit(ch) && strlen(arg(i)) == 1
                       && (x = ch - '0') != m4->active_div) {
                if (put_obuf(m4->div[m4->active_div], m4->div[x])) {
                    fprintf(stderr, "%s:%d: Error\n", __FILE__, __LINE__);
                    return ERR;
                }
            } else {
                p = arg(i);
                while (isdigit(*p++));
                if (*p == '\0') {
                fprintf(stderr, "%s:%d:%s: Usage error: "
                        "Invalid diversion number\n", __FILE__, __LINE__,
                        arg(0));
                return USAGE_ERR;
                }
                /*
                 * Assume a filename. Outputs directly to the active diversion,
                 * even during argument collection.
                 */
                if (put_file(m4->div[m4->active_div], arg(i))) {
                    fprintf(stderr, "%s:%d: Error\n", __FILE__, __LINE__);
                    return ERR;
                }
            }
        }
    } else {
        /* No args, so undivert all into the current diversion */
        for (i = 0; i < NUM_DIVS - 1; ++i)
            if (i != m4->active_div
                && put_obuf(m4->div[m4->active_div], m4->div[i])) {
                fprintf(stderr, "%s:%d: Error\n", __FILE__, __LINE__);
                return ERR;
            }
    }

    return 0;
}

#undef NM
#undef PAR_DESC
#define NM writediv
#define PAR_DESC "(div_num, filename[, append])"

int econc(m4_, NM) (void *v) {
    M4ptr m4 = (M4ptr) v;
    int append = 0;
    char ch;

    print_help;
    allow_pass_through;
    max_pars(3);
    min_pars(2);

    if (num_args_collected >= 3 && !strcmp(arg(3), "1"))
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
#undef PAR_DESC
#define NM divnum
#define PAR_DESC ""

int econc(m4_, NM) (void *v) {
    M4ptr m4 = (M4ptr) v;
    char ch;

    print_help;
    max_pars(0);

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
#undef PAR_DESC
#define NM maketemp
#define PAR_DESC "(templateXXXXXX)"

int econc(m4_, NM) (void *v) {
    M4ptr m4 = (M4ptr) v;
    char *temp_fn = NULL;
    int r;

    print_help;
    allow_pass_through;
    max_pars(1);
    min_pars(1);

    r = make_temp(arg(1), &temp_fn);

    if (r)
        return r;

    if (unget_str(m4->input, temp_fn)) {
        fprintf(stderr, "%s:%d: Error\n", __FILE__, __LINE__);
        free(temp_fn);
        return ERR;
    }

    free(temp_fn);

    return 0;
}

#undef NM
#undef PAR_DESC
#define NM mkstemp
#define PAR_DESC "(templateXXXXXX)"

int econc(m4_, NM) (void *v) {
    M4ptr m4 = (M4ptr) v;
    char *temp_fn = NULL;
    int r;

    print_help;
    allow_pass_through;
    max_pars(1);
    min_pars(1);

    r = make_stemp(arg(1), &temp_fn);

    if (r)
        return ERR_CONTINUE;

    if (unget_str(m4->input, temp_fn)) {
        fprintf(stderr, "%s:%d: Error\n", __FILE__, __LINE__);
        free(temp_fn);
        return ERR;
    }

    free(temp_fn);

    return 0;
}

#undef NM
#undef PAR_DESC
#define NM include
#define PAR_DESC "(filename)"

int econc(m4_, NM) (void *v) {
    M4ptr m4 = (M4ptr) v;

    print_help;
    allow_pass_through;
    max_pars(1);
    min_pars(1);

    if (unget_file(&m4->input, arg(1))) {
        fprintf(stderr, "%s:%d: Error\n", __FILE__, __LINE__);
        return ERR;
    }

    return 0;
}

#undef NM
#undef PAR_DESC
#define NM sinclude
#define PAR_DESC "(filename)"

int econc(m4_, NM) (void *v) {
    M4ptr m4 = (M4ptr) v;
    FILE *fp;

    /* Silent include */

    print_help;
    allow_pass_through;
    max_pars(1);
    min_pars(1);

    if ((fp = fopen(arg(1), "rb")) == NULL)
        return 0;               /* No error, no warning */

    if (unget_stream(&m4->input, fp, arg(1))) {
        fprintf(stderr, "%s:%d: Error\n", __FILE__, __LINE__);
        fclose(fp);
        return ERR;
    }

    return 0;
}

#undef NM
#undef PAR_DESC
#define NM dnl
#define PAR_DESC ""

int econc(m4_, NM) (void *v) {
    M4ptr m4 = (M4ptr) v;

    print_help;
    max_pars(0);

    return delete_to_nl(&m4->input);
}

#undef NM
#undef PAR_DESC
#define NM tnl
#define PAR_DESC "(str)"

int econc(m4_, NM) (void *v) {
    M4ptr m4 = (M4ptr) v;
    char *p, *q, ch;

    print_help;
    allow_pass_through;
    max_pars(1);
    min_pars(1);

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
#undef PAR_DESC
#define NM regexrep
#define PAR_DESC "(text, regex_find, replace[, newline_insensitive, verbose])"

int econc(m4_, NM) (void *v) {
    M4ptr m4 = (M4ptr) v;
    int ret = ERR;
    char *res;
    size_t res_len;
    int nl_sen = 1;             /* Newline sensitive (on) */
    int verbose = 0;            /* Prints information about the regex */

    print_help;
    allow_pass_through;
    max_pars(5);
    min_pars(3);

    if (!strcmp(arg(4), "1"))
        nl_sen = 0;             /* Newline sensitive off. Insensitive. */

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
#undef PAR_DESC
#define NM lsdir
#define PAR_DESC "[(dir_name)]"

int econc(m4_, NM) (void *v) {
    M4ptr m4 = (M4ptr) v;
    char *res;

    print_help;
    allow_pass_through;
    max_pars(1);

    if ((res = ls_dir(num_args_collected ? arg(1) : ".")) == NULL) {
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
#undef PAR_DESC
#define NM ifdef
#define PAR_DESC "(macro_name, when_defined[, when_undefined])"

int econc(m4_, NM) (void *v) {
    M4ptr m4 = (M4ptr) v;
    struct entry *e;

    print_help;
    allow_pass_through;
    max_pars(3);
    min_pars(2);

    e = lookup(m4->ht, arg(1));
    if (e != NULL) {
        if (unget_str(m4->input, arg(2))) {
            fprintf(stderr, "%s:%d: Error\n", __FILE__, __LINE__);
            return ERR;
        }
    } else if (num_args_collected >= 3) {
        if (*arg(3) != '\0' && unget_str(m4->input, arg(3))) {
            fprintf(stderr, "%s:%d: Error\n", __FILE__, __LINE__);
            return ERR;
        }
    }
    return 0;
}

#undef NM
#undef PAR_DESC
#define NM ifelse
#define PAR_DESC "(switch, case_a, when_a[, case_b, when_b, ... ][, default])"

int econc(m4_, NM) (void *v) {
    M4ptr m4 = (M4ptr) v;
    size_t i;

    print_help;
    if (!num_args_collected) {
        m4->pass_through = 1;
        return 0;
    }

    if (num_args_collected < 3) {
        fprintf(stderr, "%s:%lu [%s:%d]: Usage: %s%s\n", m4->input->nm,
                (unsigned long) m4->input->rn, __FILE__, __LINE__, arg(0),
                PAR_DESC);
        return USAGE_ERR;
    }

    for (i = 2; i <= num_args_collected - 1; i += 2)
        if (!strcmp(arg(1), arg(i))) {
            if (unget_str(m4->input, arg(i + 1))) {
                fprintf(stderr, "%s:%d: Error\n", __FILE__, __LINE__);
                return ERR;
            }
            return 0;
        }

    /* Default */
    if (num_args_collected > 3 && num_args_collected % 2 == 0
        && unget_str(m4->input, arg(num_args_collected))) {
        fprintf(stderr, "%s:%d: Error\n", __FILE__, __LINE__);
        return ERR;
    }

    return 0;
}

#undef NM
#undef PAR_DESC
#define NM defn
#define PAR_DESC "(macro_name)"

int econc(m4_, NM) (void *v) {
    M4ptr m4 = (M4ptr) v;
    struct entry *e = NULL;
    size_t i;

    print_help;
    allow_pass_through;
    min_pars(1);

    for (i = num_args_collected; i >= 1; --i) {
        /* Reverse order because ungetting */
        e = lookup(m4->ht, arg(i));
        if (e != NULL && e->func_p == NULL) {
            /* User-defined text macro */
            if (unget_str(m4->input, m4->right_quote)) {
                fprintf(stderr, "%s:%d: Error\n", __FILE__, __LINE__);
                return ERR;
            }
            if (unget_str(m4->input, e->def)) {
                fprintf(stderr, "%s:%d: Error\n", __FILE__, __LINE__);
                return ERR;
            }
            if (unget_str(m4->input, m4->left_quote)) {
                fprintf(stderr, "%s:%d: Error\n", __FILE__, __LINE__);
                return ERR;
            }
        }
    }

    if (num_args_collected == 1 && e != NULL && e->func_p != NULL) {
        /*
         * Built-in macro. The definition is the function pointer.
         * Look next in the stack to see if defn was called as the second
         * argument to define or pushdef (or renames of these). If so,
         * temporarily save the function pointer for after the stack is
         * popped. Otherwise, do nothing.
         *
         * The m_i difference needs to be 4 to line up with the second argument
         * of the next macro call in the stack.
         * def macro_name arg1 arg2 def ...
         * ^                        ^
         * |                        |
         * +------ Diff is 4 -------+
         */

        if (m4->stack->next != NULL
            && (m4->stack->next->mfp == &m4_define
                || m4->stack->next->mfp == &m4_pushdef)
            && m4->stack->m_i - m4->stack->next->m_i == 4)
            m4->tmp_mfp = e->func_p;
    }

    return 0;
}

#undef NM
#undef PAR_DESC
#define NM dumpdef
#define PAR_DESC "[(macro_name[, ... ])]"

int econc(m4_, NM) (void *v) {
    M4ptr m4 = (M4ptr) v;
    int ret;
    size_t i;
    struct entry *e;

    print_help;

    m4->help = 1;

    if (num_args_collected) {
        for (i = 1; i <= num_args_collected; ++i) {
            if (*arg(i) == '\0') {
                fprintf(stderr, "%s:%d:%s: Usage error: "
                        "Argument is empty string\n", __FILE__, __LINE__,
                        arg(0));
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
                    fprintf(stderr, "Built-in: %s", e->name);
                    if ((ret = (*e->func_p) (m4))) {
                        m4->help = 0;
                        return ret;
                    }
                }
            }
        }
    } else {
        /* Dump all macro definitions */
        for (i = 0; i < NUM_BUCKETS; ++i) {
            e = m4->ht->b[i];
            while (e != NULL) {
                if (e->func_p == NULL) {
                    fprintf(stderr, "User-def: %s: %s\n", e->name, e->def);
                } else {
                    fprintf(stderr, "Built-in: %s", e->name);
                    if ((ret = (*e->func_p) (m4))) {
                        m4->help = 0;
                        return ret;
                    }
                }
                e = e->next;
            }
        }
    }

    m4->help = 0;

    return 0;
}

#undef NM
#undef PAR_DESC
#define NM m4wrap
#define PAR_DESC "(code_to_include_at_end)"

int econc(m4_, NM) (void *v) {
    M4ptr m4 = (M4ptr) v;

    print_help;
    allow_pass_through;
    max_pars(1);
    min_pars(1);

    if (put_str(m4->wrap, arg(1))) {
        fprintf(stderr, "%s:%d: Error\n", __FILE__, __LINE__);
        return ERR;
    }

    return 0;
}

#undef NM
#undef PAR_DESC
#define NM errprint
#define PAR_DESC "(error_message)"

int econc(m4_, NM) (void *v) {
    M4ptr m4 = (M4ptr) v;

    print_help;
    allow_pass_through;
    max_pars(1);
    min_pars(1);

    fprintf(stderr, "%s\n", arg(1));
    return 0;
}

#undef NM
#undef PAR_DESC
#define NM len
#define PAR_DESC "(str)"

int econc(m4_, NM) (void *v) {
    M4ptr m4 = (M4ptr) v;
    char num[NUM_BUF_SIZE];
    int r;

    print_help;
    allow_pass_through;
    max_pars(1);
    min_pars(1);

    r = snprintf(num, NUM_BUF_SIZE, "%lu", (unsigned long) strlen(arg(1)));
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
#undef PAR_DESC
#define NM substr
#define PAR_DESC "(str, start_index[, size])"

int econc(m4_, NM) (void *v) {
    M4ptr m4 = (M4ptr) v;
    size_t len, x, y;

    print_help;
    allow_pass_through;
    max_pars(3);
    min_pars(2);

    len = strlen(arg(1));

    if (str_to_size_t(arg(2), &x)) {
        fprintf(stderr, "%s:%d:%s: Usage error: "
                "Invalid number\n", __FILE__, __LINE__, arg(0));
        return USAGE_ERR;
    }
    if (num_args_collected >= 3) {
        if (str_to_size_t(arg(3), &y)) {
            fprintf(stderr, "%s:%d:%s: Usage error: "
                    "Invalid number\n", __FILE__, __LINE__, arg(0));
            return USAGE_ERR;
        }
        if (aof(x, y, SIZE_MAX)) {
            fprintf(stderr, "%s:%d:%s: User overflow error\n", __FILE__,
                    __LINE__, arg(0));
            return USER_OVERFLOW_ERR;
        }
        /* Truncate string */
        if (x + y < len) {
            *(arg(1) + x + y) = '\0';
        } else if (x + y > len) {
            fprintf(stderr, "%s:%lu [%s:%d]: %s: Usage warning: "
                    "Substring is out of bounds\n", m4->input->nm,
                    (unsigned long) m4->input->rn, __FILE__, __LINE__,
                    arg(0));
            if (m4->warn_to_error)
                return USAGE_ERR;
        }
    }
    if (x < len) {
        if (unget_str(m4->input, arg(1) + x)) {
            fprintf(stderr, "%s:%d: Error\n", __FILE__, __LINE__);
            return ERR;
        }
    } else {
        fprintf(stderr, "%s:%lu [%s:%d]: %s: Usage warning: "
                "Index is out of bounds\n", m4->input->nm,
                (unsigned long) m4->input->rn, __FILE__, __LINE__, arg(0));
        if (m4->warn_to_error)
            return USAGE_ERR;
    }
    return 0;
}

#undef NM
#undef PAR_DESC
#define NM index
#define PAR_DESC "(big_str, small_str)"

int econc(m4_, NM) (void *v) {
    M4ptr m4 = (M4ptr) v;
    char *p;
    char num[NUM_BUF_SIZE];
    int r;

    print_help;
    allow_pass_through;
    max_pars(2);
    min_pars(2);

    p = quick_search(arg(1), strlen(arg(1)), arg(2), strlen(arg(2)));

    if (p != NULL) {
        r = snprintf(num, NUM_BUF_SIZE, "%lu",
                     (unsigned long) (p - arg(1)));
        if (r < 0 || r >= NUM_BUF_SIZE) {
            fprintf(stderr, "%s:%d: Error\n", __FILE__, __LINE__);
            return ERR;
        }

        if (unget_str(m4->input, num)) {
            fprintf(stderr, "%s:%d: Error\n", __FILE__, __LINE__);
            return ERR;
        }
    } else {
        if (unget_str(m4->input, "-1")) {
            fprintf(stderr, "%s:%d: Error\n", __FILE__, __LINE__);
            return ERR;
        }
    }

    return 0;
}

#undef NM
#undef PAR_DESC
#define NM translit
#define PAR_DESC "(str, from_chars, to_chars)"

int econc(m4_, NM) (void *v) {
    M4ptr m4 = (M4ptr) v;
    int map[UCHAR_MAX + 1] = { '\0' };
    char *f_str, *t_str;
    struct range f_r, t_r;
    unsigned char f_ch, t_ch;
    char *p;
    unsigned char uch;
    int x;

    print_help;
    allow_pass_through;
    max_pars(3);
    min_pars(3);

    memset(&f_r, '\0', sizeof(struct range));
    memset(&t_r, '\0', sizeof(struct range));

    f_str = arg(2);             /* From */
    t_str = arg(3);             /* To */

    /* Create mapping */
    while (1) {
        f_ch = read_range_ch(&f_str, &f_r);
        t_ch = read_range_ch(&t_str, &t_r);

        if (!f_ch) {
            if (t_ch) {
                fprintf(stderr, "%s:%lu [%s:%d]: %s: Syntax warning: "
                        "TO component of mapping exceeds FROM component\n",
                        m4->input->nm, (unsigned long) m4->input->rn,
                        __FILE__, __LINE__, arg(0));
                if (m4->warn_to_error)
                    return SYNTAX_ERR;
            }
            break;
        }

        /* First match stays */
        if (!map[f_ch]) {
            if (t_ch == '\0')
                map[f_ch] = -1; /* Delete */
            else
                map[f_ch] = t_ch;
        }
    }

    /* Apply mapping */
    m4->tmp->i = 0;
    p = arg(1);
    while ((uch = *p++)) {
        x = map[uch];
        if (!x)
            x = uch;

        if (x != -1 && put_ch(m4->tmp, x)) {
            fprintf(stderr, "%s:%d: Error\n", __FILE__, __LINE__);
            return ERR;
        }
    }
    if (put_ch(m4->tmp, '\0')) {
        fprintf(stderr, "%s:%d: Error\n", __FILE__, __LINE__);
        return ERR;
    }

    if (unget_str(m4->input, m4->tmp->a)) {
        fprintf(stderr, "%s:%d: Error\n", __FILE__, __LINE__);
        return ERR;
    }

    return 0;
}

#undef NM
#undef PAR_DESC
#define NM incr
#define PAR_DESC "(number)"

int econc(m4_, NM) (void *v) {
    M4ptr m4 = (M4ptr) v;
    char *p;
    int neg = 0;
    size_t x;
    char num[NUM_BUF_SIZE];
    int r;

    print_help;
    allow_pass_through;
    max_pars(1);
    min_pars(1);

    p = arg(1);
    if (*p == '-') {
        neg = 1;
        ++p;
    }

    if (str_to_size_t(p, &x)) {
        fprintf(stderr, "%s:%d:%s: Usage error: "
                "Invalid number\n", __FILE__, __LINE__, arg(0));
        return USAGE_ERR;
    }

    if (neg && x) {
        --x;
    } else {
        if (x == SIZE_MAX) {
            fprintf(stderr, "%s:%d:%s: User overflow error\n", __FILE__,
                    __LINE__, arg(0));
            return USER_OVERFLOW_ERR;
        }
        ++x;
    }

    if (!x)
        neg = 0;

    r = snprintf(num, NUM_BUF_SIZE, "%lu", (unsigned long) x);
    if (r < 0 || r >= NUM_BUF_SIZE) {
        fprintf(stderr, "%s:%d: Error\n", __FILE__, __LINE__);
        return ERR;
    }

    if (unget_str(m4->input, num)) {
        fprintf(stderr, "%s:%d: Error\n", __FILE__, __LINE__);
        return ERR;
    }

    if (neg && unget_ch(m4->input, '-')) {
        fprintf(stderr, "%s:%d: Error\n", __FILE__, __LINE__);
        return ERR;
    }

    return 0;
}

#undef NM
#undef PAR_DESC
#define NM decr
#define PAR_DESC "(number)"

int econc(m4_, NM) (void *v) {
    M4ptr m4 = (M4ptr) v;
    char *p;
    int neg = 0;
    size_t x;
    char num[NUM_BUF_SIZE];
    int r;

    print_help;
    allow_pass_through;
    max_pars(1);
    min_pars(1);

    p = arg(1);
    if (*p == '-') {
        neg = 1;
        ++p;
    }

    if (str_to_size_t(p, &x)) {
        fprintf(stderr, "%s:%d:%s: Usage error: "
                "Invalid number\n", __FILE__, __LINE__, arg(0));
        return USAGE_ERR;
    }

    if (!neg && x) {
        --x;
    } else {
        if (x == SIZE_MAX) {
            fprintf(stderr, "%s:%d:%s: User overflow error\n", __FILE__,
                    __LINE__, arg(0));
            return USER_OVERFLOW_ERR;
        }
        if (!x)
            neg = 1;

        ++x;
    }

    r = snprintf(num, NUM_BUF_SIZE, "%lu", (unsigned long) x);
    if (r < 0 || r >= NUM_BUF_SIZE) {
        fprintf(stderr, "%s:%d: Error\n", __FILE__, __LINE__);
        return ERR;
    }

    if (unget_str(m4->input, num)) {
        fprintf(stderr, "%s:%d: Error\n", __FILE__, __LINE__);
        return ERR;
    }

    if (neg && unget_ch(m4->input, '-')) {
        fprintf(stderr, "%s:%d: Error\n", __FILE__, __LINE__);
        return ERR;
    }

    return 0;
}

#undef NM
#undef PAR_DESC
#define NM eval
#define PAR_DESC "(arithmetic_expression[, base, pad, verbose])"

int econc(m4_, NM) (void *v) {
    M4ptr m4 = (M4ptr) v;
    int ret = ERR;
    long x;
    unsigned int base = 10, pad = 0;
    char *num_str = NULL;
    int verbose = 0;

    print_help;
    allow_pass_through;
    max_pars(4);
    min_pars(1);

    if (num_args_collected >= 2 && str_to_uint(arg(2), &base)) {
        fprintf(stderr, "%s:%d: Error\n", __FILE__, __LINE__);
        return ERR;
    }

    if (num_args_collected >= 3 && str_to_uint(arg(3), &pad)) {
        fprintf(stderr, "%s:%d: Error\n", __FILE__, __LINE__);
        return ERR;
    }

    if (num_args_collected >= 4 && !strcmp(arg(4), "1"))
        verbose = 1;

    if ((ret = eval_str(arg(1), &x, verbose)))
        return ret;

    if ((num_str = ltostr(x, base, pad)) == NULL) {
        fprintf(stderr, "%s:%d: Error\n", __FILE__, __LINE__);
        return ERR;
    }

    if (unget_str(m4->input, num_str)) {
        fprintf(stderr, "%s:%d: Error\n", __FILE__, __LINE__);
        return ERR;
    }

    free(num_str);

    return 0;
}

#undef NM
#undef PAR_DESC
#define NM sysval
#define PAR_DESC ""

int econc(m4_, NM) (void *v) {
    M4ptr m4 = (M4ptr) v;
    char num[NUM_BUF_SIZE];
    int r;

    print_help;
    max_pars(0);

    r = snprintf(num, NUM_BUF_SIZE, "%d", m4->sys_val);
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
#undef PAR_DESC
#define NM syscmd
#define PAR_DESC "(shell_command)"

int econc(m4_, NM) (void *v) {
    M4ptr m4 = (M4ptr) v;
    int st;

    print_help;
    allow_pass_through;
    max_pars(1);
    min_pars(1);

    st = system(arg(1));

#ifndef _WIN32
    if (!WIFEXITED(st)) {
        fprintf(stderr, "%s:%d: Error\n", __FILE__, __LINE__);
        return ERR;
    }
#endif

#ifndef _WIN32
    st = WEXITSTATUS(st);
#endif
    m4->sys_val = st;

    return 0;
}

#undef NM
#undef PAR_DESC
#define NM esyscmd
#define PAR_DESC "(shell_command)"

int econc(m4_, NM) (void *v) {
    M4ptr m4 = (M4ptr) v;
    FILE *fp;
    int x, st;

    print_help;
    allow_pass_through;
    max_pars(1);
    min_pars(1);

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
#ifndef _WIN32
    st = WEXITSTATUS(st);
#endif
    m4->sys_val = st;

    return 0;
}

#undef NM
#undef PAR_DESC
#define NM m4exit
#define PAR_DESC "[(exit_value)]"

int econc(m4_, NM) (void *v) {
    M4ptr m4 = (M4ptr) v;
    size_t x = 0;

    print_help;
    max_pars(1);

    if (num_args_collected) {
        if (str_to_size_t(arg(1), &x)) {
            fprintf(stderr, "%s:%d: Error\n", __FILE__, __LINE__);
            return ERR;
        }

        if (x > UCHAR_MAX) {
            fprintf(stderr, "%s:%d: Error\n", __FILE__, __LINE__);
            return ERR;
        }
    }

    m4->req_exit_val = x;

    return 0;
}

#undef NM
#undef PAR_DESC
#define NM errok
#define PAR_DESC ""

int econc(m4_, NM) (void *v) {
    M4ptr m4 = (M4ptr) v;

    print_help;
    max_pars(0);

    m4->error_exit = 0;

    return 0;
}

#undef NM
#undef PAR_DESC
#define NM errexit
#define PAR_DESC ""

int econc(m4_, NM) (void *v) {
    M4ptr m4 = (M4ptr) v;

    print_help;
    max_pars(0);

    m4->error_exit = 1;

    return 0;
}

#undef NM
#undef PAR_DESC
#define NM warnerr
#define PAR_DESC ""

int econc(m4_, NM) (void *v) {
    M4ptr m4 = (M4ptr) v;

    print_help;
    max_pars(0);

    m4->warn_to_error = 1;

    return 0;
}

#undef NM
#undef PAR_DESC
#define NM warnok
#define PAR_DESC ""

int econc(m4_, NM) (void *v) {
    M4ptr m4 = (M4ptr) v;

    print_help;
    max_pars(0);

    m4->warn_to_error = 0;

    return 0;
}

#undef NM
#undef PAR_DESC
#define NM traceon
#define PAR_DESC ""

int econc(m4_, NM) (void *v) {
    M4ptr m4 = (M4ptr) v;

    print_help;
    max_pars(0);

    m4->trace = 1;

    return 0;
}

#undef NM
#undef PAR_DESC
#define NM traceoff
#define PAR_DESC ""

int econc(m4_, NM) (void *v) {
    M4ptr m4 = (M4ptr) v;

    print_help;
    max_pars(0);

    m4->trace = 0;

    return 0;
}

#undef NM
#undef PAR_DESC
#define NM recrm
#define PAR_DESC "(file_path)"

int econc(m4_, NM) (void *v) {
    M4ptr m4 = (M4ptr) v;

    print_help;
    allow_pass_through;
    max_pars(1);
    min_pars(1);

    if (*arg(1) == '\0') {
        fprintf(stderr, "%s:%d:%s: Usage error: "
                "Argument is empty string\n", __FILE__, __LINE__, arg(0));
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
     * m4exit built-in macro. However, a requested value of zero be overwritten
     * if another error occurs.
     */
    int ret = 0;                /* Success so far */
    int mrv = 0;                /* Macro return value */
    /*
     * Can only request a positive return value.
     * -1 indicates that no request has been made.
     */
    int req_exit_val = -1;
    M4ptr m4 = NULL;
    struct entry *e;            /* Used for macro lookups */
    int i, r;
    char *p;
    int no_file = 1;            /* No files specified on the command line */

    if (sane_io())
        mgoto(error);

    if (setlocale(LC_ALL, "") == NULL)
        mgoto(error);

    if ((m4 = init_m4()) == NULL)
        mgoto(error);

/* Load built-in macros */
#define load_bi(m) if (upsert(m4->ht, #m, NULL, & m4_ ## m, 0)) \
    mgoto(error)

    load_bi(define);
    load_bi(pushdef);
    load_bi(undefine);
    load_bi(popdef);
    load_bi(changecom);
    load_bi(changequote);
    load_bi(shift);
    load_bi(divert);
    load_bi(undivert);
    load_bi(writediv);
    load_bi(divnum);
    load_bi(maketemp);
    load_bi(mkstemp);
    load_bi(include);
    load_bi(sinclude);
    load_bi(dnl);
    load_bi(tnl);
    load_bi(regexrep);
    load_bi(lsdir);
    load_bi(ifdef);
    load_bi(ifelse);
    load_bi(defn);
    load_bi(dumpdef);
    load_bi(m4wrap);
    load_bi(errprint);
    load_bi(len);
    load_bi(substr);
    load_bi(index);
    load_bi(translit);
    load_bi(incr);
    load_bi(decr);
    load_bi(eval);
    load_bi(syscmd);
    load_bi(esyscmd);
    load_bi(sysval);
    load_bi(m4exit);
    load_bi(errok);
    load_bi(errexit);
    load_bi(warnerr);
    load_bi(warnok);
    load_bi(traceon);
    load_bi(traceoff);
    load_bi(recrm);


#define program_usage "m4 [-s] [-D macro_name[=macro_def]] ... " \
    "[-U macro_name] ... file ..."

    /* Process command line arguments */
    for (i = 1; i < argc; ++i) {
        if (!strcmp(*(argv + i), "-s")) {
            m4->line_direct = 1;
        } else if (!strcmp(*(argv + i), "-D")) {
            if (i + 1 == argc) {
                fprintf(stderr, "[%s:%d]: Error: Usage: %s\n", __FILE__,
                        __LINE__, program_usage);
                ret = USAGE_ERR;
                goto error;
            }
            p = strchr(*(argv + i + 1), '=');
            if (p != NULL)
                *p = '\0';

            if (add_macro
                (m4, *(argv + i + 1), p == NULL ? NULL : p + 1, 0))
                mgoto(error);

            ++i;
        } else if (!strcmp(*(argv + i), "-U")) {
            if (i + 1 == argc) {
                fprintf(stderr, "[%s:%d]: Error: Usage: %s\n", __FILE__,
                        __LINE__, program_usage);
                ret = USAGE_ERR;
                goto error;
            }
            if (delete_entry(m4->ht, *(argv + i + 1), 0)) {
                fprintf(stderr,
                        "[%s:%d]: Usage warning: Macro does not exist: %s\n",
                        __FILE__, __LINE__, *(argv + i + 1));
                if (m4->warn_to_error) {
                    ret = USAGE_ERR;
                    goto error;
                }
            }

            ++i;
        } else if (!strcmp(*(argv + i), "-")) {
            if (append_stream(&m4->input, stdin, "stdin"))
                mgoto(error);

            no_file = 0;
        } else {
            if (append_file(&m4->input, *(argv + i)))
                mgoto(error);

            no_file = 0;
        }
    }

    if (no_file && append_stream(&m4->input, stdin, "stdin"))
        mgoto(error);


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

        /*
         * Only flush upon newline, so that an empty buffer represents
         * start of line.
         */
        if (m4->div[0]->i && *(m4->div[0]->a + m4->div[0]->i - 1) == '\n'
            && flush_obuf(m4->div[0]))
            mgoto(error);

        /* Clear diversion -1 */
        m4->div[DIVERSION_NEGATIVE_1]->i = 0;

        if (m4->left_comment != NULL && m4->right_comment != NULL) {
            if (!m4->comment_on) {
                r = eat_str_if_match(&m4->input, m4->left_comment);
                if (r == ERR)
                    mgoto(error);

                if (output_line_directive(m4))
                    mgoto(error);

                if (r == MATCH) {
                    if (put_str(output, m4->left_comment))
                        mgoto(error);

                    m4->comment_on = 1;

                    /* As might have a right comment immediately afterwards */
                    goto top;
                }
            } else {
                r = eat_str_if_match(&m4->input, m4->right_comment);
                if (r == ERR)
                    mgoto(error);

                if (output_line_directive(m4))
                    mgoto(error);

                if (r == MATCH) {
                    if (put_str(output, m4->right_comment))
                        mgoto(error);

                    m4->comment_on = 0;

                    /* As might have a left comment immediately afterwards */
                    goto top;
                }
            }
        }

        r = eat_str_if_match(&m4->input, m4->left_quote);
        if (r == ERR)
            mgoto(error);

        if (output_line_directive(m4))
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

        if (output_line_directive(m4))
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
        r = get_word(&m4->input, m4->token, 0);
        if (r == ERR)
            mgoto(error);
        else if (r == EOF) {
            if (m4->wrap->i) {
                if (put_ch(m4->wrap, '\0'))
                    mgoto(error);

                if (unget_str(m4->input, m4->wrap->a))
                    mgoto(error);

                m4->wrap->i = 0;

                goto top;
            }
            break;
        }

        if (output_line_directive(m4))
            mgoto(error);

        if (m4->comment_on || m4->quote_depth) {
            /* In a comment, or quoted, so pass through */
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
                            m4->input->nm, (unsigned long) m4->input->rn,
                            __FILE__, __LINE__);
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
            e = NULL;
            /* Short circuit */
            if (isalpha(*m4->token->a) || *m4->token->a == '_')
                e = lookup(m4->ht, m4->token->a);

            if (e == NULL) {
                /* Not a macro */
                /* Pass through */
                if (put_str(output, m4->token->a))
                    mgoto(error);
            } else {
                /*  Macro */

                if (stack_mc(&m4->stack, &m4->stack_depth))
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
                    fprintf(stderr, "Trace: %s:%lu: %s: Stack depth: %lu\n", m4->input->nm,
                            (unsigned long) m4->input->rn, e->name, (unsigned long) m4->stack_depth);

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

    /*
     * A requested exit value of zero will be overwritten if there has been
     * an error.
     */
    if (req_exit_val != -1 && req_exit_val != 0)
        return req_exit_val;

    return ret;

  error:
    if (!ret)
        ret = ERR;

    goto clean_up;
}
