/*
 * Copyright (c) 2024 Logan Ryan McLintock
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

/* toco_regex */

/*
 *        +---+
 *        |   |
 *        |   |
 *  +-----+   +-----+
 *  |               |
 *  +-----+   +-----+
 *        |   |
 *        |   |
 *        |   |
 *        |   |
 *        |   |
 *        +---+
 *
 * "And now I give you a new commandment: love one another. As I have loved
 * you, so you must love one another. If you have love for one another, then
 * everyone will know that you are my disciples."
 *                                      John 13:34-35 GNT
 */


#include "toucanlib.h"

#define INIT_NUM_NODES 100
#define INIT_OPERAND_STACK_NUM 100


/* Lookup in storage */
#define lk(n) (*(ns->a + (n)))

/* Sets link_type to END_NODE */
#define clear_node(n) memset(ns->a + (n), '\0', sizeof(struct nfa_node))


#define copy_node(dst, src) memcpy(ns->a + (dst), ns->a + (src),    \
    sizeof(struct nfa_node))


/* operator: */
#define LEFT_PAREN 0
#define RIGHT_PAREN 1
#define ONE_OR_MORE 2
#define ZERO_OR_ONE 3
#define ZERO_OR_MORE 4
#define CONCAT 5
#define SOL_ANCHOR 6
#define EOL_ANCHOR 7
#define OR 8

#define NO_OPERATOR 9


/*
 * link1 is only used when both links are epsilon.
 * link_type:
 */
#define END_NODE 0
#define BOTH_EPSILON 1
#define EPSILON 2
#define SOL_READ_STATUS 3
#define EOL_READ_STATUS 4
#define CHAR_SET 5


struct operator_detail {
    unsigned char precedence;
    char associativity;
    char *str;
};

struct regex_item {
    unsigned char *char_set;
    unsigned char operator;
    struct regex_item *next;
};

struct nfa_node {
    unsigned char *char_set;
    char link_type;
    size_t link0;
    size_t link1;
};

struct nfa_storage {
    struct nfa_node *a;
    size_t i;
    size_t n;                   /* Number of elements, not bytes */
    size_t reuse;               /* For reusing deleted nodes */
    unsigned char reuse_set;
};

/* Each NFA fragment is itself a valid NFA */
struct nfa {
    size_t start;
    size_t end;
};

struct operand_stack {
    struct nfa *a;
    size_t i;
    size_t n;                   /* Number of elements, not bytes */
};

struct regex {
    char *find_esc;
    size_t find_esc_size;
    int *find_eof;
    struct regex_item *ri;
    struct nfa_storage *ns;
    size_t nfa_start;
    size_t nfa_end;
    int nl_ins;                 /* Newline insensitive matching */
    unsigned char *state;
    unsigned char *state_next;
};

struct operator_detail op_detail[] = {
    { 4, '_', "(" },            /* LEFT_PAREN */
    { 4, '_', ")" },            /* RIGHT_PAREN */
    { 3, 'L', "+" },            /* ONE_OR_MORE */
    { 3, 'L', "?" },            /* ZERO_OR_ONE */
    { 3, 'L', "*" },            /* ZERO_OR_MORE */
    { 2, 'L', "." },            /* CONCAT */
    { 1, 'R', "^" },            /* SOL_ANCHOR */
    { 1, 'L', "$" },            /* EOL_ANCHOR */
    { 0, 'L', "|" }             /* OR */
};


/* ********** Helper functions ********** */

static struct regex_item *init_regex_item(void)
{
    return calloc(1, sizeof(struct regex_item));
}

static void free_regex_chain(struct regex_item *head)
{
    struct regex_item *t;
    while (head != NULL) {
        t = head->next;
        free(head->char_set);
        free(head);
        head = t;
    }
}

static struct nfa_storage *init_nfa_storage(void)
{
    struct nfa_storage *t = NULL;

    if ((t = calloc(1, sizeof(struct nfa_storage))) == NULL)
        mgoto(error);

    if (sizeof(struct nfa_node)
        && INIT_NUM_NODES > SIZE_MAX / sizeof(struct nfa_node))
        mgoto(error);

    if ((t->a = calloc(INIT_NUM_NODES, sizeof(struct nfa_node))) == NULL)
        mgoto(error);

    t->n = INIT_NUM_NODES;

    return t;

  error:
    if (t != NULL) {
        free(t->a);
        free(t);
    }

    return NULL;
}

static void free_nfa_storage(struct nfa_storage *ns)
{
    if (ns != NULL) {
        free(ns->a);
        free(ns);
    }
}

static int issue_node(struct nfa_storage *ns, size_t *node)
{
    struct nfa_node *t;
    size_t new_n, new_size_in_bytes;

    if (ns->reuse_set) {
        clear_node(ns->reuse);
        *node = ns->reuse;
        ns->reuse_set = 0;
        return 0;
    }

    if (ns->i == ns->n) {
        if (ns->n > SIZE_MAX / 2)
            mgoto(error);

        new_n = ns->n * 2;
        if (sizeof(struct nfa_node)
            && new_n > SIZE_MAX / sizeof(struct nfa_node))
            mgoto(error);

        new_size_in_bytes = new_n * sizeof(struct nfa_node);

        if ((t = realloc(ns->a, new_size_in_bytes)) == NULL)
            mgoto(error);

        ns->a = t;
        ns->n = new_n;
    }

    clear_node(ns->i);
    *node = ns->i;
    ++ns->i;

    return 0;

  error:
    return GEN_ERROR;
}

static int delete_node(struct nfa_storage *ns, size_t node)
{
    /*
     * A simple delete like this only works for nodes that are not referenced
     * by other nodes (such as start nodes).
     */
    if (ns->i && node == ns->i - 1) {
        ns->i--;
        return 0;
    }

    /* There can only be one node marked for reuse at a time */
    if (ns->reuse_set)
        d_mreturn("Node reuse already set", GEN_ERROR);

    ns->reuse = node;
    ns->reuse_set = 1;

    return 0;
}

static void fill_hole(struct nfa_storage *ns, size_t *start_node,
                      size_t *end_node)
{
    /*
     * Fills hole with the last node in the array, and patches the links.
     * Updates start_node and end_node if they were the old last node.
     */
    size_t old_node, i;

    if (!ns->i) {
        ns->reuse_set = 0;
        return;
    }

    if (!ns->reuse_set)
        return;

    old_node = ns->i - 1;
    copy_node(ns->reuse, old_node);
    --ns->i;

    /* Patch references */
    for (i = 0; i < ns->i; ++i) {
        if (lk(i).link0 == old_node)
            lk(i).link0 = ns->reuse;

        if (lk(i).link1 == old_node)
            lk(i).link1 = ns->reuse;
    }

    /* Update start and end nodes */
    if (old_node == *start_node)
        *start_node = ns->reuse;

    if (old_node == *end_node)
        *end_node = ns->reuse;

    ns->reuse_set = 0;
}

static struct operand_stack *init_operand_stack(void)
{
    struct operand_stack *t = NULL;

    if ((t = calloc(1, sizeof(struct operand_stack))) == NULL)
        mgoto(error);

    if (sizeof(struct nfa)
        && INIT_OPERAND_STACK_NUM > SIZE_MAX / sizeof(struct nfa))
        mgoto(error);

    if ((t->a =
         calloc(INIT_OPERAND_STACK_NUM, sizeof(struct nfa))) == NULL)
        mgoto(error);

    t->n = INIT_OPERAND_STACK_NUM;

    return t;

  error:
    if (t != NULL) {
        free(t->a);
        free(t);
    }
    return NULL;
}

static void free_operand_stack(struct operand_stack *z)
{
    if (z != NULL) {
        free(z->a);
        free(z);
    }
}

static int push_operand_stack(struct operand_stack *z, size_t nfa_start,
                              size_t nfa_end)
{
    struct nfa *t;
    size_t new_n, new_size_in_bytes;
    if (z->i == z->n) {
        if (z->n > SIZE_MAX / 2)
            mgoto(error);

        new_n = z->n * 2;

        if (sizeof(struct nfa) && new_n > SIZE_MAX / sizeof(struct nfa))
            mgoto(error);

        new_size_in_bytes = new_n * sizeof(struct nfa);

        if ((t = realloc(z->a, new_size_in_bytes)) == NULL)
            mgoto(error);

        z->a = t;
        z->n = new_n;
    }

    (*(z->a + z->i)).start = nfa_start;
    (*(z->a + z->i)).end = nfa_end;
    ++z->i;
    return 0;

  error:
    return GEN_ERROR;
}

static int pop_operand_stack(struct operand_stack *z, size_t *nfa_start,
                             size_t *nfa_end)
{
    if (!z->i)
        return GEN_ERROR;

    --z->i;
    *nfa_start = (*(z->a + z->i)).start;
    *nfa_end = (*(z->a + z->i)).end;
    return 0;
}

static struct regex *init_regex(void)
{
    return calloc(1, sizeof(struct regex));
}

static void free_regex(struct regex *reg)
{
    if (reg != NULL) {
        free(reg->find_esc);
        free(reg->find_eof);
        free_regex_chain(reg->ri);
        free_nfa_storage(reg->ns);
        free(reg->state);
        free(reg->state_next);
        free(reg);
    }
}


/* ********** Regex related functions ********** */

static int interpret_escaped_chars(const char *input_str, char **output,
                                   size_t *output_size)
{
    int ret = 0;
    const char *p;
    char *mem = NULL, *q, ch, c, h1, h0;
    size_t len = strlen(input_str);

    if ((mem = calloc(len + 1, 1)) == NULL)
        mgoto(error);

    p = input_str;
    q = mem;
    while ((ch = *p++) != '\0') {
        if (ch == '\\')
            switch ((c = *p++)) {
            case '\0':
                d_mgoto(syntax_error, "Incomplete escape sequence\n");
                break;
            case '0':
                *q++ = '\0';
                break;
            case 'a':
                *q++ = '\a';
                break;
            case 'b':
                *q++ = '\b';
                break;
            case 't':
                *q++ = '\t';
                break;
            case 'n':
                *q++ = '\n';
                break;
            case 'v':
                *q++ = '\v';
                break;
            case 'f':
                *q++ = '\f';
                break;
            case 'r':
                *q++ = '\r';
                break;
            case 'x':
                if (isxdigit((h1 = *p++))
                    && isxdigit((h0 = *p++)))
                    *q++ = hex(h1, h0);
                else
                    d_mgoto(syntax_error, "Invalid hex character\n");

                break;
            default:
                *q++ = '\\';
                *q++ = c;
                break;
        } else
            *q++ = ch;
    }
    *output_size = q - mem;
    *output = mem;
    return 0;

  error:
    ret = GEN_ERROR;

  syntax_error:
    if (!ret)
        ret = SYNTAX_ERROR;

    free(mem);
    return ret;
}

static int char_to_int_eof_term(const char *input, size_t input_size,
                                int **output)
{
    /* EOF terminated */
    int *imem = NULL;
    const unsigned char *p;
    size_t i;

    p = (unsigned char *) input;

    if (input_size == SIZE_MAX)
        mgoto(error);

    if ((imem = calloc(input_size + 1, sizeof(int))) == NULL)
        mgoto(error);

    for (i = 0; i < input_size; ++i)
        imem[i] = p[i];

    imem[input_size] = EOF;

    *output = imem;
    return 0;

  error:
    free(imem);
    return GEN_ERROR;
}

static void print_cs_ch(unsigned char u)
{
    if (isgraph(u) && u != '-' && u != 'e' && u != '^' && u != '$')
        putc(u, stderr);
    else
        fprintf(stderr, "\\x%02X", u);
}

static void print_char_set(const unsigned char *char_set)
{
    int i, in_range;

    if (char_set == NULL) {
        fprintf(stderr, "NULL");
        return;
    }

    in_range = 0;
    for (i = 0; i <= UCHAR_MAX; ++i)
        if (!in_range && i && char_set[i - 1] && char_set[i]
            && i != UCHAR_MAX && char_set[i + 1]) {
            in_range = 1;
            putc('-', stderr);
        } else if (in_range && !char_set[i]) {
            print_cs_ch(i - 1);
            in_range = 0;
        } else if (in_range && i == UCHAR_MAX && char_set[i]) {
            print_cs_ch(i);
            in_range = 0;
        } else if (!in_range && char_set[i]) {
            print_cs_ch(i);
        }
}

static void print_regex_chain(const struct regex_item *ri_head)
{
    while (ri_head != NULL) {
        if (ri_head->operator == NO_OPERATOR) {
            fprintf(stderr, "Char set: ");
            print_char_set(ri_head->char_set);
            putc('\n', stderr);
        } else {
            fprintf(stderr, "Operator: %s\n",
                    op_detail[ri_head->operator].str);
        }
        ri_head = ri_head->next;
    }
}

static int create_regex_chain(const int *find_eof,
                              struct regex_item **ri_head, int nl_ins)
{
    int ret = 0;
    const int *p;
    int x, y, i;
    struct regex_item *t, *ri = NULL;
    struct regex_item *head = NULL, *prev = NULL;
    int negate_set, first_ch_in_set;

    p = find_eof;
    while ((x = *p++) != EOF) {
        if ((t = init_regex_item()) == NULL)
            mgoto(error);

        if (p == find_eof + 1) {
            /* First node */
            ri = t;
            head = ri;
        } else {
            ri->next = t;
            ri = t;
        }

        if (x == '\\') {
            if (*p == EOF)
                d_mgoto(syntax_error, "Incomplete escape sequence\n");

            if ((ri->char_set = calloc(UCHAR_MAX + 1, 1)) == NULL)
                mgoto(error);

            ri->operator = NO_OPERATOR;
            ri->char_set[*p++] = 1;
        } else if (x == '[') {
            /* Character set */
            if ((ri->char_set = calloc(UCHAR_MAX + 1, 1)) == NULL)
                mgoto(error);

            ri->operator = NO_OPERATOR;

            negate_set = 0;

            if (*p == '^') {
                negate_set = 1;
                ++p;
            }

            first_ch_in_set = 1;

            while (1) {
                /* Check for range */
                if ((first_ch_in_set || (*p != ']' && *p != EOF))
                    && *(p + 1) == '-' && *(p + 2) != ']'
                    && *(p + 2) != EOF) {
                    if (*p > *(p + 2))
                        d_mgoto(syntax_error, "Descending range\n");

                    for (y = *p; y <= *(p + 2); ++y)
                        ri->char_set[y] = 1;

                    p += 2;
                } else if (*p == ']' && !first_ch_in_set) {
                    ++p;
                    break;      /* End of set */
                } else if (*p == EOF) {
                    d_mgoto(syntax_error, "Unclosed character set\n");
                } else {
                    ri->char_set[*p] = 1;
                }
                first_ch_in_set = 0;
                ++p;
            }

            if (negate_set)
                for (i = 0; i <= UCHAR_MAX; ++i)
                    ri->char_set[i] = !ri->char_set[i];
        } else {
            /* Operators and other characters */
            switch (x) {
                /* Operators */
            case '(':
                ri->operator = LEFT_PAREN;
                break;
            case ')':
                ri->operator = RIGHT_PAREN;
                break;
            case '+':
                ri->operator = ONE_OR_MORE;
                break;
            case '?':
                ri->operator = ZERO_OR_ONE;
                break;
            case '*':
                ri->operator = ZERO_OR_MORE;
                break;
            case '^':
                ri->operator = SOL_ANCHOR;
                break;
            case '$':
                ri->operator = EOL_ANCHOR;
                break;
            case '|':
                ri->operator = OR;
                break;
            default:
                /* Other characters */
                if ((ri->char_set = calloc(UCHAR_MAX + 1, 1)) == NULL)
                    mgoto(error);

                ri->operator = NO_OPERATOR;

                if (x == '.') {
                    /* Set of all chars */
                    for (i = 0; i <= UCHAR_MAX; ++i)
                        ri->char_set[i] = 1;

                    if (!nl_ins)
                        ri->char_set['\n'] = 0;
                } else {
                    ri->char_set[x] = 1;
                }

                break;
            }
        }
        if ((ri->operator == NO_OPERATOR || ri->operator == LEFT_PAREN
             || ri->operator == SOL_ANCHOR)
            && prev != NULL && (prev->operator == NO_OPERATOR
                                || prev->operator == ONE_OR_MORE
                                || prev->operator == ZERO_OR_ONE
                                || prev->operator == ZERO_OR_MORE
                                || prev->operator == RIGHT_PAREN
                                || prev->operator == EOL_ANCHOR)) {
            /* Link in concatenation */
            if ((t = init_regex_item()) == NULL)
                mgoto(error);

            t->operator = CONCAT;

            prev->next = t;
            t->next = ri;
        }

        prev = ri;
    }

    *ri_head = head;

    return 0;

  error:
    ret = GEN_ERROR;

  syntax_error:
    if (!ret)
        ret = SYNTAX_ERROR;

    free_regex_chain(head);
    return ret;
}


#define pop_operator_to_output do {             \
    if (output_tail == NULL) {                  \
        output_tail = operator_stack;           \
        postfix = output_tail;                  \
    } else {                                    \
        output_tail->next = operator_stack;     \
        output_tail = output_tail->next;        \
    }                                           \
    operator_stack = operator_stack->next;      \
} while (0)


static void shunting_yard(struct regex_item **ri_head)
{
    struct regex_item *operator_stack = NULL;
    struct regex_item *postfix = NULL;  /* Output head */
    struct regex_item *output_tail = NULL;
    struct regex_item *next = NULL;
    struct regex_item *ri, *t;

    ri = *ri_head;
    while (ri != NULL) {
        next = ri->next;        /* Backup */
        if (ri->operator == NO_OPERATOR) {
            if (output_tail == NULL) {
                output_tail = ri;
                postfix = output_tail;
            } else {
                output_tail->next = ri;
                output_tail = ri;
            }
        } else if (ri->operator == LEFT_PAREN) {
            ri->next = operator_stack;
            operator_stack = ri;
        } else if (ri->operator == RIGHT_PAREN) {
            while (operator_stack != NULL) {
                if (operator_stack->operator == LEFT_PAREN) {
                    t = operator_stack;
                    operator_stack = operator_stack->next;
                    /* Need to free left parenthesis, as removed */
                    t->next = NULL;     /* Isolate */
                    free_regex_chain(t);
                    break;
                } else {
                    pop_operator_to_output;
                }
            }
            /* Need to free right parenthesis, as removed */
            /* Isolate. next has already been backed up above. */
            ri->next = NULL;
            free_regex_chain(ri);
        } else {
            while (operator_stack != NULL) {
                if (operator_stack->operator == LEFT_PAREN
                    || (op_detail[ri->operator].associativity == 'L'
                        && op_detail[operator_stack->operator].precedence
                        < op_detail[ri->operator].precedence)
                    || (op_detail[ri->operator].associativity == 'R'
                        && op_detail[operator_stack->operator].precedence
                        <= op_detail[ri->operator].precedence)) {
                    break;
                } else {
                    pop_operator_to_output;
                }
            }
            ri->next = operator_stack;
            operator_stack = ri;
        }
        ri = next;              /* Restore from backup */
    }
    while (operator_stack != NULL)
        pop_operator_to_output;

    if (output_tail != NULL)
        output_tail->next = NULL;

    *ri_head = postfix;
}

#undef pop_operator_to_output


static int thompsons_construction(const struct regex_item *ri_head,
                                  struct nfa_storage **nfa_store,
                                  size_t *nfa_start, size_t *nfa_end)
{
    int ret = 0;
    struct nfa_storage *ns = NULL;
    struct operand_stack *z = NULL;
    const struct regex_item *ri;
    size_t start_a, end_a, start_b, end_b, start_c, end_c;

    if ((ns = init_nfa_storage()) == NULL)
        mgoto(error);

    if ((z = init_operand_stack()) == NULL)
        mgoto(error);

    ri = ri_head;
    while (ri != NULL) {
        switch (ri->operator) {
        case NO_OPERATOR:
            if (issue_node(ns, &start_c))
                mgoto(error);

            if (issue_node(ns, &end_c))
                mgoto(error);

            lk(start_c).link0 = end_c;
            lk(start_c).link_type = CHAR_SET;
            /* Responsibility to free remains with free_regex_chain */
            lk(start_c).char_set = ri->char_set;

            if (push_operand_stack(z, start_c, end_c))
                mgoto(error);

            break;
        case LEFT_PAREN:
        case RIGHT_PAREN:
            d_mgoto(syntax_error, "Parenthesis in postfix form\n");
            break;
        case ONE_OR_MORE:
            if (pop_operand_stack(z, &start_b, &end_b))
                mgoto(error);

            /* Loop-back */
            lk(end_b).link0 = start_b;

            /* New end node */
            if (issue_node(ns, &end_c))
                mgoto(error);

            lk(end_b).link1 = end_c;
            lk(end_b).link_type = BOTH_EPSILON;

            if (push_operand_stack(z, start_b, end_c))
                mgoto(error);

            break;
        case ZERO_OR_ONE:
            if (pop_operand_stack(z, &start_b, &end_b))
                mgoto(error);

            /* New start node */
            if (issue_node(ns, &start_c))
                mgoto(error);

            /* New end node */
            if (issue_node(ns, &end_c))
                mgoto(error);

            /* Link in new nodes */
            lk(start_c).link0 = start_b;
            lk(end_b).link0 = end_c;
            lk(end_b).link_type = EPSILON;

            /* Bypass */
            lk(start_c).link1 = end_c;

            lk(start_c).link_type = BOTH_EPSILON;

            if (push_operand_stack(z, start_c, end_c))
                mgoto(error);

            break;
        case ZERO_OR_MORE:
            if (pop_operand_stack(z, &start_b, &end_b))
                mgoto(error);

            /* New start node */
            if (issue_node(ns, &start_c))
                mgoto(error);

            /* New end node */
            if (issue_node(ns, &end_c))
                mgoto(error);

            /* Loop-back */
            lk(end_b).link0 = start_b;

            /* Link in new nodes */
            lk(start_c).link0 = start_b;
            lk(end_b).link0 = end_c;

            /* Bypass */
            lk(start_c).link1 = end_c;

            lk(start_c).link_type = BOTH_EPSILON;
            lk(end_b).link_type = BOTH_EPSILON;

            if (push_operand_stack(z, start_c, end_c))
                mgoto(error);

            break;
        case CONCAT:
            if (pop_operand_stack(z, &start_b, &end_b))
                mgoto(error);

            if (pop_operand_stack(z, &start_a, &end_a))
                mgoto(error);

            /* Copy contents */
            copy_node(end_a, start_b);

            /*
             * Remove unneeded node.
             * Only concatenation removes a node.
             * OK, as start nodes are not referenced by other nodes.
             */
            if (delete_node(ns, start_b))
                mgoto(error);

            if (push_operand_stack(z, start_a, end_b))
                mgoto(error);

            break;
        case SOL_ANCHOR:
            if (z->i) {
                if (pop_operand_stack(z, &start_b, &end_b))
                    mgoto(error);

                /* New start node */
                if (issue_node(ns, &start_c))
                    mgoto(error);

                /* Link in */
                lk(start_c).link0 = start_b;
                lk(start_c).link_type = SOL_READ_STATUS;

                if (push_operand_stack(z, start_c, end_b))
                    mgoto(error);
            } else {
                /* ^ is a valid regex by itself */
                if (issue_node(ns, &start_c))
                    mgoto(error);

                if (issue_node(ns, &end_c))
                    mgoto(error);

                lk(start_c).link0 = end_c;
                lk(start_c).link_type = SOL_READ_STATUS;

                if (push_operand_stack(z, start_c, end_c))
                    mgoto(error);
            }

            break;
        case EOL_ANCHOR:
            if (z->i) {
                if (pop_operand_stack(z, &start_b, &end_b))
                    mgoto(error);

                /* New end node */
                if (issue_node(ns, &end_c))
                    mgoto(error);

                lk(end_b).link0 = end_c;
                lk(end_b).link_type = EOL_READ_STATUS;

                if (push_operand_stack(z, start_b, end_c))
                    mgoto(error);
            } else {
                /* $ is a valid regex by itself */
                if (issue_node(ns, &start_c))
                    mgoto(error);

                if (issue_node(ns, &end_c))
                    mgoto(error);

                lk(start_c).link0 = end_c;
                lk(start_c).link_type = EOL_READ_STATUS;

                if (push_operand_stack(z, start_c, end_c))
                    mgoto(error);
            }

            break;
        case OR:
            if (pop_operand_stack(z, &start_b, &end_b))
                mgoto(error);

            if (pop_operand_stack(z, &start_a, &end_a))
                mgoto(error);

            /* New start node */
            if (issue_node(ns, &start_c))
                mgoto(error);

            /* New end node */
            if (issue_node(ns, &end_c))
                mgoto(error);

            /* Branch start */
            lk(start_c).link0 = start_a;
            lk(start_c).link1 = start_b;
            lk(start_c).link_type = BOTH_EPSILON;

            /* Connect end */
            lk(end_a).link0 = end_c;
            lk(end_a).link_type = EPSILON;

            lk(end_b).link0 = end_c;
            lk(end_b).link_type = EPSILON;

            if (push_operand_stack(z, start_c, end_c))
                mgoto(error);

            break;
        }

        ri = ri->next;
    }

    if (pop_operand_stack(z, &start_b, &end_b))
        d_mgoto(error, "No NFA generated\n");

    if (z->i)
        d_mgoto(syntax_error, "%lu operands left on the stack\n",
                (unsigned long) z->i);

    /* Fill the last hole (if any) */
    fill_hole(ns, &start_b, &end_b);

    *nfa_start = start_b;
    *nfa_end = end_b;
    *nfa_store = ns;
    free_operand_stack(z);

    return 0;

  error:
    ret = GEN_ERROR;

  syntax_error:
    if (!ret)
        ret = SYNTAX_ERROR;

    free_nfa_storage(ns);
    free_operand_stack(z);
    return ret;
}

static void print_nfa(struct nfa_storage *ns)
{
    size_t i;
    for (i = 0; i < ns->i; ++i) {
        if (lk(i).link_type != END_NODE) {
            fprintf(stderr, "%lu -- ", (unsigned long) i);
            switch (lk(i).link_type) {
            case BOTH_EPSILON:
            case EPSILON:
                putc('e', stderr);
                break;
            case SOL_READ_STATUS:
                putc('^', stderr);
                break;
            case EOL_READ_STATUS:
                putc('$', stderr);
                break;
            case CHAR_SET:
                print_char_set(lk(i).char_set);
                break;
            }
            fprintf(stderr, " --> %lu\n", (unsigned long) lk(i).link0);

            if (lk(i).link_type == BOTH_EPSILON)
                fprintf(stderr, "%lu -- e --> %lu\n", (unsigned long) i,
                        (unsigned long) lk(i).link1);
        }
    }
}

static int compile_regex(const char *regex_str, int nl_ins,
                         struct regex **regex_st, int verbose)
{
    int ret = GEN_ERROR;
    struct regex *reg = NULL;

    if (regex_str == NULL || *regex_str == '\0')
        d_mgoto(usage_error, "NULL or empty regex string\n");

    if ((reg = init_regex()) == NULL)
        mgoto(error);

    reg->nl_ins = nl_ins;

    if ((ret =
         interpret_escaped_chars(regex_str, &reg->find_esc,
                                 &reg->find_esc_size)))
        mgoto(error);

    if ((ret =
         char_to_int_eof_term(reg->find_esc, reg->find_esc_size,
                              &reg->find_eof)))
        mgoto(error);

    if ((ret = create_regex_chain(reg->find_eof, &reg->ri, reg->nl_ins)))
        mgoto(error);

    if (verbose) {
        fprintf(stderr, "Infix:\n");
        print_regex_chain(reg->ri);
    }

    shunting_yard(&reg->ri);

    if (verbose) {
        fprintf(stderr, "Postfix:\n");
        print_regex_chain(reg->ri);
    }

    if ((ret =
         thompsons_construction(reg->ri, &reg->ns, &reg->nfa_start,
                                &reg->nfa_end)))
        mgoto(error);

    if (verbose) {
        fprintf(stderr, "NFA:\n");
        print_nfa(reg->ns);
    }

    /* Allocate state tables */
    if ((reg->state = calloc(reg->ns->i, 1)) == NULL)
        mgoto(error);

    if ((reg->state_next = calloc(reg->ns->i, 1)) == NULL)
        mgoto(error);

    *regex_st = reg;
    return 0;

  usage_error:
    ret = USAGE_ERROR;

  error:
    free_regex(reg);
    return ret;
}


#define swap_state_tables do {              \
    t = reg->state;                         \
    reg->state = reg->state_next;           \
    reg->state_next = t;                    \
} while (0)


#define clear_state_table(tb)  memset(tb, '\0', ns->i)


#define check_for_winner do {                                               \
    if (reg->state_next[reg->nfa_end]) {                                    \
        /*                                                                  \
         * End node is in state. Record the match. Regex takes the longest  \
         * match, so OK to overwrite any previous match.                    \
         */                                                                 \
        last_match = p;                                                     \
    } else {                                                                \
        count = 0;                                                          \
        for (i = 0; i < ns->i; ++i)                                         \
            if (reg->state_next[i])                                         \
                ++count;                                                    \
                                                                            \
        if (!count) {       /* All nodes out */                             \
            goto report;                                                    \
        }                                                                   \
    }                                                                       \
} while (0)


#define print_state_tables for (i = 0; i < ns->i; ++i)          \
    fprintf(stderr, "Node %lu: %d %d\n", (unsigned long) i,     \
        reg->state[i], reg->state_next[i])


static char *run_nfa(const char *text, size_t text_size, int sol,
                     struct regex *reg, size_t *match_len, int verbose)
{
    /* Does not advance */
    struct nfa_storage *ns;
    unsigned char *t;
    const char *p, *last_match = NULL;
    unsigned char u;
    size_t s, i, count;
    int diff, eol = 0;

    ns = reg->ns;               /* Make a shortcut so that lk works */
    p = text;
    s = text_size;

    if (verbose) {
        fprintf(stderr, "=== Start of NFA run ===\n");
        fprintf(stderr, "Start node: %lu\nEnd node: %lu\n", reg->nfa_start,
                reg->nfa_end);
    }


    /* Clear state tables */
    clear_state_table(reg->state);
    clear_state_table(reg->state_next);

    /* Set start node */
    reg->state[reg->nfa_start] = 1;

    while (1) {
        /*
         * sol is initially inherited from the function call, as it is
         * internally unknown if text is at the start of the greater context
         * line. This is because this function will be called repetitively
         * by an advancing wrapper.
         */

        /*
         * Set end of line read status.
         * Note that the character \n cannot match when in newline sensitive
         * (not insensitive) mode, as the process will stop before it is read.
         */
        if (!s || (*p == '\n' && !reg->nl_ins))
            eol = 1;

        /*
         * Move without reading a character from text. States are additive.
         * Stop when no new states are being added.
         */
        while (1) {
            for (i = 0; i < ns->i; ++i) {
                if (reg->state[i]) {
                    reg->state_next[i] = 1;     /* Accumulative */
                    if (lk(i).link_type == EPSILON
                        || lk(i).link_type == BOTH_EPSILON
                        || (lk(i).link_type == SOL_READ_STATUS && sol)
                        || (lk(i).link_type == EOL_READ_STATUS && eol))
                        reg->state_next[lk(i).link0] = 1;

                    if (lk(i).link_type == BOTH_EPSILON)
                        reg->state_next[lk(i).link1] = 1;
                }
            }

            diff = 0;

            if (verbose) {
                fprintf(stderr, "No read:\n");
                print_state_tables;
            }

            /* Check if the states are the same */
            for (i = 0; i < ns->i; ++i)
                if (reg->state[i] != reg->state_next[i]) {
                    diff = 1;
                    break;
                }

            /* Stops infinite loops */
            if (!diff)
                break;

            swap_state_tables;
            clear_state_table(reg->state_next);
        }

        /* State tables are the same now */

        check_for_winner;

        if (eol)
            goto report;

        /* Read a char */
        u = *p++;
        --s;

        /* Deactivate start of line read status after first read */
        sol = 0;

        if (verbose)
            fprintf(stderr, "Read char: %c\n", u);

        clear_state_table(reg->state_next);

        /* Advance or be eliminated */
        for (i = 0; i < ns->i; ++i)
            if (reg->state[i])
                if (lk(i).link_type == CHAR_SET && lk(i).char_set[u])
                    reg->state_next[lk(i).link0] = 1;

        if (verbose)
            print_state_tables;

        check_for_winner;

        swap_state_tables;
        clear_state_table(reg->state_next);
    }

  report:



    /* End of text */
    if (last_match == NULL) {
        if (verbose)
            fprintf(stderr, " => NO MATCH\n");

        return NULL;
    }

    *match_len = last_match - text;

    if (verbose)
        fprintf(stderr, " => MATCH\n");

    return (char *) text;
}

#undef swap_state_tables
#undef clear_state_table
#undef check_for_winner
#undef print_state_tables


static char *internal_regex_search(const char *text, size_t text_size,
                                   int sol, struct regex *reg,
                                   size_t *match_len, int verbose)
{
    /* Advances */
    const char *q;
    size_t ts;
    char *match = NULL;
    size_t ml;                  /* Match length */

    q = text;
    ts = text_size;
    while ((match = run_nfa(q, ts, sol, reg, &ml, verbose)) == NULL) {
        if (!reg->nl_ins && *q == '\n')
            sol = 1;
        else
            sol = 0;

        if (!ts)
            break;

        ++q;
        --ts;
    }

    if (verbose)
        fprintf(stderr, "=== Search result ===\n");

    if (match == NULL) {
        if (verbose)
            fprintf(stderr, "No match\n");

        return NULL;
    }

    if (verbose) {
        fprintf(stderr, "match_offset: %lu\n", match - text);
        fprintf(stderr, "match_len: %lu\n", ml);
    }

    *match_len = ml;
    return match;
}

int regex_search(const char *text, size_t text_size, int sol,
                 const char *regex_str, int nl_ins, size_t *match_offset,
                 size_t *match_len, int verbose)
{
    int ret = GEN_ERROR;
    struct regex *reg = NULL;
    char *m;
    size_t ml;

    if (text == NULL) {
        ret = USAGE_ERROR;
        mgoto(clean_up);
    }

    if ((ret = compile_regex(regex_str, nl_ins, &reg, verbose)))
        mgoto(clean_up);

    m = internal_regex_search(text, text_size, sol, reg, &ml, verbose);

    if (m == NULL) {
        ret = NO_MATCH;
        mgoto(clean_up);
    }

    *match_offset = m - text;
    *match_len = ml;

    ret = 0;

  clean_up:
    free_regex(reg);

    return ret;
}

int regex_replace(const char *text, size_t text_size,
                  const char *regex_str, int nl_ins,
                  const char *replace_str, char **result,
                  size_t *result_len, int verbose)
{
    /*
     * Repeated search and replace. The result is \0 terminated and the length
     * is provide in result_len (excluding the final added \0 char).
     * However, the result might have embedded \0 chars.
     */
    int ret = GEN_ERROR;
    struct regex *reg = NULL;
    char *replace_esc = NULL;
    size_t replace_esc_size;
    struct obuf *output = NULL;

    int sol;
    const char *q;
    const char *q_stop;         /* Exclusive */
    char *m, *m_last_end;
    size_t ml;

    if (text == NULL) {
        ret = USAGE_ERROR;
        mgoto(clean_up);
    }

    if ((ret = compile_regex(regex_str, nl_ins, &reg, verbose)))
        mgoto(clean_up);

    if ((ret =
         interpret_escaped_chars(replace_str, &replace_esc,
                                 &replace_esc_size)))
        mgoto(clean_up);

    if (text_size > SIZE_MAX / 2)
        mgoto(clean_up);

    if ((output = init_obuf(text_size * 2)) == NULL)
        mgoto(clean_up);

    /* Do not run NFA if there is no input text */
    if (!text_size)
        goto finish;

    q = text;
    q_stop = q + text_size;
    sol = 1;
    m_last_end = NULL;
    while (1) {
        /* Recheck start of line read status */
        if (q != text && *(q - 1) == '\n' && !nl_ins) {
            sol = 1;
            m_last_end = NULL;
        }

        m = internal_regex_search(q, q_stop - q, sol, reg, &ml, verbose);

        if (m == NULL) {
            /* Print rest of text */
            if (put_mem(output, q, q_stop - q))
                mgoto(clean_up);

            break;
        }

        /* Print text before match */
        if (put_mem(output, q, m - q))
            mgoto(clean_up);

        /* Print replacement text */
        if (!(!ml && m == m_last_end)
            && put_mem(output, replace_esc, replace_esc_size))
            mgoto(clean_up);

        /* Stop after running on the zero length input */
        if (q == q_stop)
            break;

        /* Advance */
        if (ml) {
            q = m + ml;
        } else {
            if (m == q_stop)
                break;

            /* Jump forward a char, but pass it through */
            if (put_mem(output, m, 1))
                mgoto(clean_up);

            q = m + 1;
        }

        sol = 0;
        m_last_end = m + ml;
    }


  finish:

    /* Terminate */
    if (put_ch(output, '\0'))
        mgoto(clean_up);

    ret = 0;
    *result = output->a;
    *result_len = output->i - 1;

  clean_up:
    free_regex(reg);
    free(replace_esc);

    if (ret)
        free_obuf(output);
    else
        free_obuf_exterior(output);

    return ret;
}
