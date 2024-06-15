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

#include <ctype.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


#define INIT_NUM_NODES 100
#define INIT_OPERAND_STACK_SIZE 100


#define ERR 1
#define SYNTAX_ERR 3


#define mgoto(lab) do {                                             \
    fprintf(stderr, "[%s: %d]: " #lab "\n", __FILE__, __LINE__);    \
    goto lab;                                                       \
} while (0)

#define d_mgoto(lab, ...) do {                                      \
    fprintf(stderr, "[%s: %d]: " #lab ": ", __FILE__, __LINE__);    \
    fprintf(stderr, __VA_ARGS__);                                   \
    goto lab;                                                       \
} while (0)


/* Inputs need to be isxdigit */
#define hex_nibble(h) (((h) & 0x0F) + ((h) & 0x40 ? 9 : 0))
#define hex(h1, h0) (hex_nibble(h1) << 4 | hex_nibble(h0))


/* Lookup in storage */
#define lk(n) (*(ns->a + (n)))


#define clear_node(n) memset(ns->a + (n), '\0', sizeof(struct nfa_node))


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
    char *char_set;
    unsigned char operator;
    struct regex_item *next;
};

struct nfa_node {
    char *char_set;
    char link_type;
    size_t link0;
    size_t link1;
    unsigned char in_state;
    unsigned char in_state_next;
};

struct nfa_storage {
    struct nfa_node *a;
    size_t i;
    size_t s;
    size_t reuse;
    int reuse_set;
};

/* Each NFA fragment is itself a valid NFA */
struct nfa {
    size_t start;
    size_t end;
};

struct operand_stack {
    struct nfa *a;
    size_t i;
    size_t s;
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

    if ((t->a = calloc(INIT_NUM_NODES, sizeof(struct nfa_node))) == NULL)
        mgoto(error);

    t->s = INIT_NUM_NODES;

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
    size_t new_s;
    if (ns->reuse_set) {
        clear_node(ns->reuse);
        *node = ns->reuse;
        ns->reuse_set = 0;
        ns->reuse = 0;
        return 0;
    }

    if (ns->i == ns->s) {
        if (ns->s > SIZE_MAX / 2)
            mgoto(error);

        new_s = ns->s * 2;
        if ((t = realloc(ns->a, new_s)) == NULL)
            mgoto(error);

        ns->a = t;
        ns->s = new_s;
    }

    clear_node(ns->i);
    *node = ns->i;
    ++ns->i;

    return 0;

  error:
    return ERR;
}

static int delete_node(struct nfa_storage *ns, size_t node)
{
    if (ns->i && node == ns->i - 1) {
        --ns->i;
        return 0;
    }
    if (ns->reuse_set)
        mgoto(error);

    ns->reuse = node;
    ns->reuse_set = 1;

    return 0;

  error:
    return ERR;
}

static void block_up_nodes(struct nfa_storage *ns)
{
    if (!ns->reuse_set)
        return;

    if (ns->reuse >= ns->i) {
        ns->reuse_set = 0;
        ns->reuse = 0;
        return;
    }

    /* Move down memory to fill hole */
    memmove(ns->a + ns->reuse, ns->a + ns->reuse + 1,
            (ns->i - (ns->reuse + 1)) * sizeof(struct nfa_node));

    --ns->i;

    ns->reuse_set = 0;
    ns->reuse = 0;
}

static struct operand_stack *init_operand_stack(void)
{
    struct operand_stack *t = NULL;

    if ((t = calloc(1, sizeof(struct operand_stack))) == NULL)
        return NULL;

    if ((t->a =
         calloc(INIT_OPERAND_STACK_SIZE, sizeof(struct nfa))) == NULL)
        mgoto(error);

    t->s = INIT_OPERAND_STACK_SIZE;

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
    size_t new_s;
    if (z->i == z->s) {
        if (z->s > SIZE_MAX / 2)
            mgoto(error);

        new_s = z->s * 2;
        if ((t = realloc(z->a, new_s)) == NULL)
            mgoto(error);

        z->a = t;
        z->s = new_s;
    }

    (*(z->a + z->i)).start = nfa_start;
    (*(z->a + z->i)).start = nfa_end;
    ++z->i;
    return 0;

  error:
    return ERR;
}

static int pop_operand_stack(struct operand_stack *z, size_t *nfa_start,
                             size_t *nfa_end)
{
    if (!z->i)
        return ERR;

    --z->i;
    *nfa_start = (*(z->a + z->i)).start;
    *nfa_end = (*(z->a + z->i)).end;
    return 0;
}

static int interpret_escaped_chars(const char *regex, int **escaped_regex)
{
    int ret = 0;
    const char *p;
    int *esc_reg = NULL, *q, ch, c, h1, h0;
    size_t len = strlen(regex);

    if ((esc_reg = calloc(len + 1, sizeof(int))) == NULL)
        mgoto(error);

    p = regex;
    q = esc_reg;
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
    *q = EOF;
    *escaped_regex = esc_reg;
    return 0;

  error:
    ret = ERR;

  syntax_error:
    if (!ret)
        ret = SYNTAX_ERR;

    free(esc_reg);
    return ret;
}

static void print_regex_chain(struct regex_item *ri_head)
{
    int i;
    while (ri_head != NULL) {
        if (ri_head->operator == NO_OPERATOR) {
            printf("Char set: ");
            if (ri_head->char_set != NULL) {
                for (i = 0; i <= UCHAR_MAX; ++i)
                    if (ri_head->char_set[i]) {
                        if (isgraph(i))
                            putchar(i);
                        else
                            printf("%02X", i);
                    }

                putchar('\n');
            } else {
                printf("NULL\n");
            }
        } else {
            printf("Operator: %s\n", op_detail[ri_head->operator].str);
        }

        ri_head = ri_head->next;
    }
}

static int create_regex_chain(int *escaped_regex,
                              struct regex_item **ri_head)
{
    int ret = 0;
    int *p, x, y, i;
    struct regex_item *t, *ri;
    struct regex_item *head = NULL, *prev = NULL;
    int negate_set, first_ch_in_set;

    p = escaped_regex;
    while ((x = *p++) != EOF) {
        if ((t = init_regex_item()) == NULL)
            mgoto(error);

        if (p == escaped_regex + 1) {
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

                if (x == '.')   /* Set of all chars */
                    for (i = 0; i <= UCHAR_MAX; ++i)
                        ri->char_set[i] = 1;
                else
                    ri->char_set[x] = 1;

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
    ret = ERR;

  syntax_error:
    if (!ret)
        ret = SYNTAX_ERR;

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
                        && op_detail[operator_stack->operator].precedence <
                        op_detail[ri->operator].precedence)
                    || (op_detail[ri->operator].associativity == 'R'
                        && op_detail[operator_stack->
                                     operator].precedence <=
                        op_detail[ri->operator].precedence)) {
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


int thompsons_construction(struct regex_item *ri_head,
                           struct nfa_storage **nfa_store,
                           size_t *nfa_start)
{
    int ret = 0;
    struct nfa_storage *ns = NULL;
    struct operand_stack *z = NULL;
    struct regex_item *ri;
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

            /* Copy links */
            lk(end_a).link0 = lk(start_b).link0;
            lk(end_a).link1 = lk(start_b).link1;
            lk(end_a).link_type = lk(start_b).link_type;

            /* Remove unneeded node */
            if (delete_node(ns, start_b))
                mgoto(error);

            if (push_operand_stack(z, start_a, end_b))
                mgoto(error);

            break;
        case SOL_ANCHOR:
            if (pop_operand_stack(z, &start_b, &end_b))
                mgoto(error);

            /* New start node */
            if (issue_node(ns, &start_c))
                mgoto(error);

            /* Link in */
            lk(end_c).link0 = start_b;
            lk(end_c).link_type = SOL_READ_STATUS;

            if (push_operand_stack(z, start_c, end_b))
                mgoto(error);

            break;
        case EOL_ANCHOR:
            if (pop_operand_stack(z, &start_b, &end_b))
                mgoto(error);

            /* New end node */
            if (issue_node(ns, &end_c))
                mgoto(error);

            lk(end_b).link0 = end_c;
            lk(end_b).link_type = EOL_READ_STATUS;

            if (push_operand_stack(z, start_b, end_c))
                mgoto(error);

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
        mgoto(error);

    if (z->i)
        d_mgoto(syntax_error, "%lu operands left on the stack\n",
                (unsigned long) z->i);

    *nfa_start = start_b;

    block_up_nodes(ns);

    *nfa_store = ns;

    free_operand_stack(z);

    return 0;

  error:
    ret = ERR;

  syntax_error:
    if (!ret)
        ret = SYNTAX_ERR;

    free_nfa_storage(ns);
    free_operand_stack(z);
    return ret;
}

int main(void)
{
    int r;
/*    char *reg = "\\thello\\nworld \\x43\\x4F\\x4F\\x4C \\xEB \\e"; */
    char *reg = "^^(ab)(az)|c";
    int *res = NULL;
    struct regex_item *head = NULL;
    struct nfa_storage *nfa_store = NULL;
    size_t nfa_start;

    if ((r = interpret_escaped_chars(reg, &res)))
        mgoto(clean_up);

    if ((r = create_regex_chain(res, &head)))
        mgoto(clean_up);

    print_regex_chain(head);

    shunting_yard(&head);

    printf("Postfix:\n");
    print_regex_chain(head);

    if ((r = thompsons_construction(head, &nfa_store, &nfa_start)))
        mgoto(clean_up);

    printf("After TC:\n");
    print_regex_chain(head);

  clean_up:
    free(res);
    free_regex_chain(head);
    free_nfa_storage(nfa_store);

    return 0;
}
