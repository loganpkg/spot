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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

struct operator_detail {
    unsigned char precedence;
    char associativity;
    char *str;
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

struct regex_item {
    char *char_set;
    unsigned char operator;
    struct regex_item *next;
};

/*
 * link1 is only used when both links are epsilon.
 * link_type:
 */
#define END_NODE 0
#define BOTH_EPSILON 1
#define EPSILON 2
#define SOL 3
#define EOL 4
#define CHAR_SET 5

struct nfa_node {
    char *char_set;
    char link_type;
    struct nfa_node *link0;
    struct nfa_node *link1;
};

struct nfa {
    struct nfa_node *start;
    struct nfa_node *end;
};


static int interpret_escaped_chars(const char *regex, int **escaped_regex)
{
    int ret = 0;
    const char *p;
    int *esc_reg = NULL, *q, ch, c, h1, h0;
    size_t len = strlen(regex);

    if ((esc_reg = calloc(len + 1, sizeof(int))) == NULL)
        return ERR;

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

  syntax_error:
    ret = SYNTAX_ERR;

    free(esc_reg);
    return ret;
}


static struct regex_item *init_regex_item(void)
{
    struct regex_item *ri;

    if ((ri = calloc(1, sizeof(struct regex_item))) == NULL)
        return NULL;

    return ri;
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

static void print_regex_chain(struct regex_item *ri_head)
{
    int i;
    while (ri_head != NULL) {
        if (ri_head->operator == NO_OPERATOR) {
            printf("Char set: ");
            for (i = 0; i <= UCHAR_MAX; ++i)
                if (ri_head->char_set[i]) {
                    if (isgraph(i))
                        putchar(i);
                    else
                        printf("%02X", i);
                }

            putchar('\n');
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
    struct regex_item *ri;

    ri = *ri_head;
    while (ri != NULL) {
        next = ri->next;        /* Backup */
        if (ri->operator == NO_OPERATOR) {
            if (postfix == NULL) {
                postfix = ri;
                output_tail = ri;
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
                    operator_stack = operator_stack->next;
                    break;
                } else {
                    pop_operator_to_output;
                }
            }
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

        ri = next;
    }
    while (operator_stack != NULL)
        pop_operator_to_output;

    if (output_tail != NULL)
        output_tail->next = NULL;

    *ri_head = postfix;
}


int main(void)
{
    int r;
/*    char *reg = "\\thello\\nworld \\x43\\x4F\\x4F\\x4C \\xEB \\e"; */
    char *reg = "^^(ab)(az)|c";
    int *res = NULL;
    struct regex_item *head;

    if ((r = interpret_escaped_chars(reg, &res)))
        return r;

    if ((r = create_regex_chain(res, &head)))
        return r;

    print_regex_chain(head);


    shunting_yard(&head);

    printf("Postfix:\n");
    print_regex_chain(head);

    return 0;
}
