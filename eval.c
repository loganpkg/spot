/*
 * Copyright (c) 2023 Logan Ryan McLintock
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

/* Evaluate arithmetic expression module */

#include "toucanlib.h"


#define INIT_BUF_SIZE 100


struct math_operator {
    unsigned char prec;
    unsigned char assoc;
    unsigned char num_operands;
    char *symbol_str;
};

struct math_operator
 oper[NUM_OPERATORS]
    = {
    { 12, '_', 0, "(" },        /* LEFT_PARENTHESIS */
    { 12, '_', 0, ")" },        /* RIGHT_PARENTHESIS */
    { 11, 'R', 1, "+" },        /* POSITIVE */
    { 11, 'R', 1, "-" },        /* NEGATIVE */
    { 11, 'R', 1, "~" },        /* BITWISE_COMPLEMENT */
    { 11, 'R', 1, "!" },        /* LOGICAL_NEGATION */
    { 10, 'R', 2, "**" },       /* EXPONENTIATION */
    { 9, 'L', 2, "*" },         /* MULTIPLICATION */
    { 9, 'L', 2, "/" },         /* DIVISION */
    { 9, 'L', 2, "%" },         /* MODULO */
    { 8, 'L', 2, "+" },         /* ADDITION */
    { 8, 'L', 2, "-" },         /* SUBTRACTION */
    { 7, 'L', 2, "<<" },        /* BITWISE_LEFT_SHIFT */
    { 7, 'L', 2, ">>" },        /* BITWISE_RIGHT_SHIFT */
    { 6, 'L', 2, "<" },         /* LESS_THAN */
    { 6, 'L', 2, "<=" },        /* LESS_THAN_OR_EQUAL */
    { 6, 'L', 2, ">" },         /* GREATER_THAN */
    { 6, 'L', 2, ">=" },        /* GREATER_THAN_OR_EQUAL */
    { 5, 'L', 2, "==" },        /* EQUAL */
    { 5, 'L', 2, "!=" },        /* NOT_EQUAL */
    { 4, 'L', 2, "&" },         /* BITWISE_AND */
    { 3, 'L', 2, "^" },         /* BITWISE_XOR */
    { 2, 'L', 2, "|" },         /* BITWISE_OR */
    { 1, 'L', 2, "&&" },        /* LOGICAL_AND */
    { 0, 'L', 2, "||" }         /* LOGICAL_OR */
};


static int process_operator(struct lbuf *x, unsigned char h, int verbose)
{
    int ret = ERROR;

    if (x->i < oper[h].num_operands) {
        fprintf(stderr, "%s:%d: Syntax Error: Insufficient operands\n",
                __FILE__, __LINE__);
        return SYNTAX_ERROR;
    }

    if (verbose) {
        printf("%s", oper[h].symbol_str);
        if (h == POSITIVE || h == NEGATIVE)
            printf("ve ");
        else
            putchar(' ');
    }

    if (oper[h].num_operands == 1) {
        /* Unary */
        if ((ret = lop(x->a + x->i - 1, 0, h)))
            d_mreturn("Operation", ret);
    } else {
        /* Binary */
        if ((ret = lop(x->a + x->i - 2, *(x->a + x->i - 1), h)))
            d_mreturn("Operation", ret);
        --x->i;
    }

    return 0;
}

int eval_ibuf(struct ibuf **input, long *res, int verbose)
{
    /* res is OK to use if return value is not ERROR (EOF is OK) */
    int ret = ERROR;
    int r = 0;
    struct obuf *token = NULL;
    struct obuf *next_token = NULL;
    struct lbuf *x = NULL;      /* Operand stack */
    struct obuf *y = NULL;      /* Operator stack */
    unsigned long num;
    char t;                     /* First char of token */
    char nt;                    /* First char of next_token */
    int match;
    unsigned char op = '_';     /* Read operator as a code */
    unsigned char h;            /* Operator at head of stack */
    int u = 1;                  /* Unary + or - indicator */
    int n = 0;                  /* Last read was a number indicator */
    int first_read = 1;
    size_t i;
    char ch;

    if ((token = init_obuf(INIT_BUF_SIZE)) == NULL) {
        ret = ERROR;
        mgoto(clean_up);
    }

    if ((next_token = init_obuf(INIT_BUF_SIZE)) == NULL) {
        ret = ERROR;
        mgoto(clean_up);
    }

    if ((x = init_lbuf(INIT_BUF_SIZE)) == NULL) {
        ret = ERROR;
        mgoto(clean_up);
    }

    if ((y = init_obuf(INIT_BUF_SIZE)) == NULL) {
        ret = ERROR;
        mgoto(clean_up);
    }

    while (1) {
        r = get_word(input, token, 1);
        if (r == ERROR) {
            ret = ERROR;
            mgoto(clean_up);
        }
        if (r == EOF && first_read) {
            /* Do not print an error message here */
            ret = EOF;
            goto clean_up;
        }

        first_read = 0;

        if (r == EOF || ((*input)->fp == stdin && *token->a == '\n')) {
            while (y->i) {
                h = *(y->a + y->i - 1);
                if (h == LEFT_PARENTHESIS)
                    mgoto(syntax_error);

                if ((ret = process_operator(x, h, verbose)))
                    mgoto(clean_up);

                --y->i;
            }
            break;
        }

        t = *token->a;

        if (isdigit(t)) {
            if ((ret = str_to_num(token->a, LONG_MAX, &num)))
                mgoto(clean_up);

            if (verbose)
                printf("%lu ", num);

            if ((ret = add_l(x, (long) num)))
                mgoto(clean_up);

            if (n) {
                ret = SYNTAX_ERROR;
                fprintf(stderr,
                        "%s:%d: Syntax error: Two consecutive numbers\n",
                        __FILE__, __LINE__);
                goto clean_up;
            }

            u = 0;
            n = 1;
        } else if (isgraph(t)) {
            /*
             * Operator:
             * Some operators are two characters long.
             */
            r = get_word(input, next_token, 1);
            if (r == ERROR) {
                ret = ERROR;
                mgoto(clean_up);
            } else if (r == EOF) {
                ret = SYNTAX_ERROR;
                fprintf(stderr,
                        "%s:%d: Syntax error: Operator at end of expression\n",
                        __FILE__, __LINE__);
                goto clean_up;
            }

            nt = *next_token->a;

            match = 0;
            for (i = 0; i < NUM_OPERATORS; ++i)
                if (*oper[i].symbol_str == t)
                    if ((ch = *(oper[i].symbol_str + 1)) == '\0'
                        || ch == nt) {
                        match = 1;
                        op = i;
                        break;
                    }

            if (!match) {
                ret = SYNTAX_ERROR;
                fprintf(stderr,
                        "%s:%d: Syntax error: Invalid operator\n",
                        __FILE__, __LINE__);
                goto clean_up;
            }

            /* Return second char if only one char operator */
            if (ch == '\0' && unget_str(*input, next_token->a)) {
                ret = ERROR;
                mgoto(clean_up);
            }

            if (!u) {
                /* Distinguish between unary and binary + and - */
                if (op == POSITIVE)
                    op = ADDITION;
                else if (op == NEGATIVE)
                    op = SUBTRACTION;
            }

            switch (op) {
            case LEFT_PARENTHESIS:
                if (put_ch(y, op)) {
                    ret = ERROR;
                    mgoto(clean_up);
                }
                u = 1;
                n = 0;
                break;
            case RIGHT_PARENTHESIS:
                while (1) {
                    if (!y->i) {
                        ret = SYNTAX_ERROR;
                        fprintf(stderr,
                                "%s:%d: Syntax error: "
                                "Open bracket not found\n",
                                __FILE__, __LINE__);
                        goto clean_up;
                    }

                    h = *(y->a + y->i - 1);
                    if (h == LEFT_PARENTHESIS) {
                        --y->i; /* Eat */
                        break;
                    }

                    if ((ret = process_operator(x, h, verbose)))
                        mgoto(clean_up);

                    --y->i;
                }
                u = 0;
                n = 0;
                break;
            default:
                while (y->i) {
                    h = *(y->a + y->i - 1);
                    if (h == LEFT_PARENTHESIS
                        || (oper[op].assoc == 'L'
                            && oper[h].prec < oper[op].prec)
                        || (oper[op].assoc == 'R'
                            && oper[h].prec <= oper[op].prec))
                        break;

                    if ((ret = process_operator(x, h, verbose)))
                        mgoto(clean_up);

                    --y->i;
                }
                if (put_ch(y, op)) {
                    ret = ERROR;
                    mgoto(clean_up);
                }
                u = 1;
                n = 0;
                break;
            }
        }
        /* Non-graph characters will be eaten */
    }

    ret = 0;

    if (!x->i) {
        *res = 0;
    } else if (x->i == 1) {
        *res = *x->a;
    } else if (x->i > 1) {
        ret = SYNTAX_ERROR;
        fprintf(stderr,
                "%s:%d: Syntax error: Multiple numbers left on the stack\n",
                __FILE__, __LINE__);
        goto clean_up;
    }

  clean_up:
    if (verbose)
        putchar('\n');

    if (ret) {
        /* Eat the rest of the line if not already at the end of the line */
        if (r != EOF && *token->a != '\n' && delete_to_nl(input))
            ret = ERROR;
    }

    free_obuf(token);
    free_obuf(next_token);
    free_lbuf(x);
    free_obuf(y);

    return ret;

  syntax_error:
    ret = SYNTAX_ERROR;
    goto clean_up;
}

int eval_str(const char *math_str, long *res, int verbose)
{
    int ret = ERROR;
    struct ibuf *input = NULL;

    if ((input = init_ibuf(INIT_BUF_SIZE)) == NULL)
        mgoto(clean_up);

    if (unget_str(input, math_str))
        mgoto(clean_up);

    if ((ret = eval_ibuf(&input, res, verbose)))
        mgoto(clean_up);

    ret = 0;

  clean_up:
    free_ibuf(input);

    return ret;
}
