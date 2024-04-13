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

/* Evaluate arithmetic expression module */

#include "toucanlib.h"


#define INIT_BUF_SIZE 100

static int process_operator(struct lbuf *x, char h, int verbose)
{
    int ret = ERR;

    if (!x->i) {
        fprintf(stderr, "%s:%d: Syntax Error: No operands\n", __FILE__,
                __LINE__);
        return SYNTAX_ERR;
    }

    if (verbose)
        printf("%c ", h);

    switch (h) {
    case 'p':                  /* Unary + */
        break;
    case 'm':                  /* Unary - */
        if ((ret = lop(x->a + x->i - 1, -1, '*'))) {
            fprintf(stderr, "%s:%d: Operation error\n", __FILE__,
                    __LINE__);
            return ret;
        }

        break;
    default:
        if (x->i == 1) {
            fprintf(stderr, "%s:%d: Syntax error: "
                    "Only one operand but binary operator\n", __FILE__,
                    __LINE__);
            return SYNTAX_ERR;
        }

        if ((ret = lop(x->a + x->i - 2, *(x->a + x->i - 1), h))) {
            fprintf(stderr, "%s:%d: Operation error\n", __FILE__,
                    __LINE__);
            return ret;
        }

        --x->i;
        break;
    }

    return 0;
}

int eval(struct ibuf *input, int read_stdin, long *res, int verbose)
{
    /* res is OK to use if return value is not ERR (EOF is OK) */
    int ret = ERR;
    int r = 0;
    struct obuf *token = NULL;
    struct lbuf *x = NULL;      /* Operand stack */
    struct obuf *y = NULL;      /* Operator stack */
    unsigned long num;
    char t;                     /* First char of token */
    char h;                     /* Operator at head of stack */
    int u = 1;                  /* Unary + or - indicator */
    int n = 0;                  /* Last read was a number indicator */
    int first_read = 1;

    if ((token = init_obuf(INIT_BUF_SIZE)) == NULL) {
        ret = ERR;
        fprintf(stderr, "%s:%d: Error\n", __FILE__, __LINE__);
        goto clean_up;
    }

    if ((x = init_lbuf(INIT_BUF_SIZE)) == NULL) {
        ret = ERR;
        fprintf(stderr, "%s:%d: Error\n", __FILE__, __LINE__);
        goto clean_up;
    }

    if ((y = init_obuf(INIT_BUF_SIZE)) == NULL) {
        ret = ERR;
        fprintf(stderr, "%s:%d: Error\n", __FILE__, __LINE__);
        goto clean_up;
    }

    while (1) {
        r = get_word(input, token, read_stdin);
        if (r == ERR) {
            ret = ERR;
            fprintf(stderr, "%s:%d: Error\n", __FILE__, __LINE__);
            goto clean_up;
        }

        if (r == EOF && first_read) {
            /* Do not print an error message here */
            ret = EOF;
            goto clean_up;
        }

        first_read = 0;

        if (r == EOF || (read_stdin && *token->a == '\n')) {
            while (y->i) {
                h = *(y->a + y->i - 1);
                if (h == '(') {
                    ret = SYNTAX_ERR;
                    fprintf(stderr, "%s:%d: Syntax error\n", __FILE__,
                            __LINE__);
                    goto clean_up;
                }

                if ((ret = process_operator(x, h, verbose))) {
                    fprintf(stderr, "%s:%d: Error\n", __FILE__, __LINE__);
                    goto clean_up;
                }

                --y->i;
            }
            break;
        }

        t = *token->a;

        if (isdigit(t)) {
            if ((ret = str_to_num(token->a, LONG_MAX, &num))) {
                fprintf(stderr, "%s:%d: Error\n", __FILE__, __LINE__);
                goto clean_up;
            }

            if (verbose)
                printf("%lu ", num);

            if ((ret = add_l(x, (long) num))) {
                fprintf(stderr, "%s:%d: Error\n", __FILE__, __LINE__);
                goto clean_up;
            }

            if (n) {
                ret = SYNTAX_ERR;
                fprintf(stderr,
                        "%s:%d: Syntax error: Two consecutive numbers\n",
                        __FILE__, __LINE__);
                goto clean_up;
            }

            u = 0;
            n = 1;
        } else if (strlen(token->a) != 1) {
            ret = SYNTAX_ERR;
            fprintf(stderr,
                    "%s:%d: Syntax error: Unknown multi-character token\n",
                    __FILE__, __LINE__);
            goto clean_up;
        } else {
            switch (t) {
            case '(':
                if (put_ch(y, t)) {
                    ret = ERR;
                    fprintf(stderr, "%s:%d: Error\n", __FILE__, __LINE__);
                    goto clean_up;
                }

                u = 1;
                n = 0;
                break;
            case ')':
                while (1) {
                    if (!y->i) {
                        ret = SYNTAX_ERR;
                        fprintf(stderr,
                                "%s:%d: Syntax error: "
                                "Open bracket not found\n",
                                __FILE__, __LINE__);
                        goto clean_up;
                    }

                    h = *(y->a + y->i - 1);
                    if (h == '(') {
                        --y->i; /* Eat */
                        break;
                    }

                    if ((ret = process_operator(x, h, verbose))) {
                        fprintf(stderr, "%s:%d: Error\n", __FILE__,
                                __LINE__);
                        goto clean_up;
                    }

                    --y->i;
                }
                u = 0;
                n = 0;
                break;
            case '^':
                while (y->i) {
                    h = *(y->a + y->i - 1);
                    if (h == '(' || (h != 'p' && h != 'm'))
                        break;

                    if ((ret = process_operator(x, h, verbose))) {
                        fprintf(stderr, "%s:%d: Error\n", __FILE__,
                                __LINE__);
                        goto clean_up;
                    }

                    --y->i;
                }
                if (put_ch(y, t)) {
                    ret = ERR;
                    fprintf(stderr, "%s:%d: Error\n", __FILE__, __LINE__);
                    goto clean_up;
                }

                u = 1;
                n = 0;
                break;
            case '*':
            case '/':
            case '%':
                while (y->i) {
                    h = *(y->a + y->i - 1);
                    if (h == '(' || h == '+' || h == '-')
                        break;

                    if ((ret = process_operator(x, h, verbose))) {
                        fprintf(stderr, "%s:%d: Error\n", __FILE__,
                                __LINE__);
                        goto clean_up;
                    }

                    --y->i;
                }
                if (put_ch(y, t)) {
                    ret = ERR;
                    fprintf(stderr, "%s:%d: Error\n", __FILE__, __LINE__);
                    goto clean_up;
                }

                u = 1;
                n = 0;
                break;
            case '+':
            case '-':
                if (u) {
                    /* Unary + or -, highest precedence */
                    if (put_ch(y, t == '+' ? 'p' : 'm')) {
                        ret = ERR;
                        fprintf(stderr, "%s:%d: Error\n", __FILE__,
                                __LINE__);
                        goto clean_up;
                    }
                } else {
                    /* Binary + or - */
                    while (y->i) {
                        h = *(y->a + y->i - 1);
                        if (h == '(')
                            break;

                        if ((ret = process_operator(x, h, verbose))) {
                            fprintf(stderr, "%s:%d: Error\n", __FILE__,
                                    __LINE__);
                            goto clean_up;
                        }

                        --y->i;
                    }
                    if (put_ch(y, t)) {
                        ret = ERR;
                        fprintf(stderr, "%s:%d: Error\n", __FILE__,
                                __LINE__);
                        goto clean_up;
                    }
                }
                u = 1;
                n = 0;
                break;
            case ' ':          /* Eat whitespace */
            case '\n':         /* Only when !read_stdin */
                break;
            default:
                ret = SYNTAX_ERR;
                fprintf(stderr, "%s:%d: Syntax error: Invalid character\n",
                        __FILE__, __LINE__);
                goto clean_up;
            }

        }

    }
    ret = 0;

    if (!x->i) {
        *res = 0;
    } else if (x->i == 1) {
        *res = *x->a;
    } else if (x->i > 1) {
        ret = SYNTAX_ERR;
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
        if (r != EOF && *token->a != '\n'
            && delete_to_nl(input, read_stdin))
            ret = ERR;
    }

    free_obuf(token);
    free_lbuf(x);
    free_obuf(y);

    return ret;
}

int eval_str(const char *math_str, long *res, int verbose)
{
    int ret = ERR;
    struct ibuf *input = NULL;

    if ((input = init_ibuf(INIT_BUF_SIZE)) == NULL) {
        ret = ERR;
        fprintf(stderr, "%s:%d: Error\n", __FILE__, __LINE__);
        goto clean_up;
    }

    if (unget_str(input, math_str)) {
        ret = ERR;
        fprintf(stderr, "%s:%d: Error\n", __FILE__, __LINE__);
        goto clean_up;
    }

    if ((ret = eval(input, 0, res, verbose))) {
        fprintf(stderr, "%s:%d: Error\n", __FILE__, __LINE__);
        goto clean_up;
    }

    ret = 0;
  clean_up:
    free_ibuf(input);

    return ret;
}
