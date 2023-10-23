/*
 * Copyright (c) 2023 Logan Ryan McLintock
 *
 * Permission to use, copy, modify, and distribute this software for any
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

#include <ctype.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

#include "eval.h"
#include "debug.h"
#include "gen.h"
#include "num.h"
#include "buf.h"


#define INIT_BUF_SIZE 100

static int process_operator(struct lbuf *x, char h)
{
    if (!x->i)                  /* No operands */
        mreturn(1);

    switch (h) {
    case 'p':                  /* Unary + */
        break;
    case 'm':                  /* Unary - */
        if (lop(x->a + x->i - 1, -1, '*'))
            mreturn(1);

        break;
    default:
        if (x->i == 1)          /* Only one operand, but binary operator */
            mreturn(1);

        if (lop(x->a + x->i - 2, *(x->a + x->i - 1), h))
            mreturn(1);

        --x->i;
        break;
    }

    mreturn(0);
}

int eval(struct ibuf *input, int read_stdin, int *math_error, long *res)
{
    /* res is OK to use if return value is not ERR (EOF is OK) */
    int ret;
    int r;
    struct obuf *token = NULL;
    struct lbuf *x = NULL;      /* Operand stack */
    struct obuf *y = NULL;      /* Operator stack */
    unsigned long num;
    char t;                     /* First char of token */
    char h;                     /* Operator at head of stack */
    int u = 1;                  /* Unary + or - indicator */
    int n = 0;                  /* Last read was a number indicator */
    int first_read = 1;

    if ((token = init_obuf(INIT_BUF_SIZE)) == NULL)
        mgoto(error);

    if ((x = init_lbuf(INIT_BUF_SIZE)) == NULL)
        mgoto(error);

    if ((y = init_obuf(INIT_BUF_SIZE)) == NULL)
        mgoto(error);

    while (1) {
        r = get_word(input, token, read_stdin);
        if (r == ERR)
            mgoto(error);

        if (r == EOF && first_read)
            mgoto(end_of_input);

        first_read = 0;

        if (r == EOF || (read_stdin && *token->a == '\n')) {
            while (y->i) {
                if (*(y->a + y->i - 1) == '(')
                    mgoto(math_err);

                if (process_operator(x, *(y->a + y->i - 1)))
                    mgoto(math_err);

                --y->i;
            }
            break;
        }

        t = *token->a;

        if (isdigit(t)) {
            if (str_to_num(token->a, LONG_MAX, &num))
                mgoto(math_err);

            if (add_l(x, (long) num))
                mgoto(error);

            if (n)
                mgoto(math_err);

            u = 0;
            n = 1;
        } else if (strlen(token->a) != 1) {
            mgoto(math_err);
        } else {
            switch (t) {
            case '(':
                if (put_ch(y, t))
                    mgoto(math_err);

                u = 1;
                n = 0;
                break;
            case ')':
                while (1) {
                    if (!y->i)  /* Open bracket not found */
                        mgoto(math_err);

                    if (*(y->a + y->i - 1) == '(') {
                        --y->i;
                        break;
                    }

                    if (process_operator(x, *(y->a + y->i - 1)))
                        mgoto(math_err);

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

                    if (process_operator(x, h))
                        mgoto(math_err);

                    --y->i;
                }
                if (put_ch(y, t))
                    mgoto(error);

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

                    if (process_operator(x, h))
                        mgoto(math_err);

                    --y->i;
                }
                if (put_ch(y, t))
                    mgoto(error);

                u = 1;
                n = 0;
                break;
            case '+':
            case '-':
                if (u) {
                    /* Unary + or -, highest precedence */
                    if (put_ch(y, t == '+' ? 'p' : 'm'))
                        mgoto(error);
                } else {
                    /* Binary + or - */
                    while (y->i) {
                        h = *(y->a + y->i - 1);
                        if (h == '(')
                            break;

                        if (process_operator(x, h))
                            mgoto(math_err);

                        --y->i;
                    }
                    if (put_ch(y, t))
                        mgoto(error);
                }
                u = 1;
                n = 0;
                break;
            case ' ':          /* Eat whitespace */
            case '\n':         /* Only when !read_stdin */
                break;
            default:           /* Invalid character */
                mgoto(math_err);
            }

        }

    }

    ret = 0;

    if (!x->i)
        *res = 0;
    else if (x->i == 1)
        *res = *x->a;
    else if (x->i > 1)
        ret = ERR;

  clean_up:
    free_obuf(token);
    free_lbuf(x);
    free_obuf(y);

    mreturn(ret);

  error:
    ret = ERR;
    goto clean_up;

  math_err:
    ret = 0;
    *math_error = 1;
    /* Eat the rest of the line if not already at the end of the line */
    if (r != EOF && *token->a != '\n' && delete_to_nl(input, read_stdin))
        ret = ERR;

    goto clean_up;

  end_of_input:
    ret = EOF;
    goto clean_up;
}

int eval_str(const char *math_str, long *res)
{
    int ret = 1;
    struct ibuf *input = NULL;
    int math_error;

    if ((input = init_ibuf(INIT_BUF_SIZE)) == NULL)
        mgoto(clean_up);

    if (unget_str(input, math_str))
        mgoto(clean_up);

    if (eval(input, 0, &math_error, res))
        mgoto(clean_up);

    ret = 0;
  clean_up:
    free_ibuf(input);

    mreturn(ret || math_error);
}
