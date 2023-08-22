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

/*
 * An implementation of the m4 macro processor.
 *
 * Trust in the LORD with all your heart.
 *                       Proverbs 3:5 GNT
 */

/*
 * List of built-in macros generated by:
 * grep -E -o '[*]@ .+ \*' m4.c | sed -E 's_[*]@ (.+) \*_ * \1_'
 *
 * define(macro_name, macro_def)
 * undefine(`macro_name')
 * changequote(left_quote, right_quote)
 * divert or divert(div_num)
 * undivert or undivert(div_num, filename, ...)
 * writediv(div_num, filename)
 * divnum
 * include(filename)
 * dnl
 * tnl
 * ifdef(`macro_name', `when_defined', `when_undefined')
 * ifelse(A, B, `when_same', `when_different')
 * dumpdef or dumpdef(`macro_name', ...)
 * errprint(error_message)
 * incr(number)
 * sysval
 * esyscmd(shell_command)
 * m4exit(exit_value)
 * remove(filename)
 */

#ifdef _WIN32
#include <io.h>
#include <fcntl.h>
#else
#define _BSD_SOURCE
#define _DEFAULT_SOURCE
#endif

#include <ctype.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#define popen _popen
#define pclose _pclose
#endif

#define DEBUG

#define NUM_BUCKETS 1024
#define BLOCK_SIZE 512
#define NUM_BUF_SIZE 32

/* EOF cannot be 1, so OK */
#define ERR 1

/* Used to not trigger the debugging */
#define NULL_OK NULL

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


#ifdef DEBUG
#define mreturn(ret) do {                                                    \
    if (!strcmp(#ret, "1") || !strcmp(#ret, "NULL") || !strcmp(#ret, "ERR")) \
        fprintf(stderr, "%s:%d: mreturn: %s\n", __FILE__, __LINE__, #ret);   \
    return ret;                                                              \
} while (0)
#else
#define mreturn(ret) return ret
#endif

#ifdef DEBUG
#define mgoto(lab) do {                                                  \
    if (!strcmp(#lab, "clean_up") || !strcmp(#lab, "error"))             \
        fprintf(stderr, "%s:%d: mgoto: %s\n", __FILE__, __LINE__, #lab); \
    goto lab;                                                            \
} while (0)
#else
#define mgoto(lab) goto lab
#endif


/* Overflow tests for size_t */
/* Addition */
#define aof(a, b) ((a) > SIZE_MAX - (b))

/* Multiplication */
#define mof(a, b) ((a) && (b) > SIZE_MAX / (a))

/*
 * unget is used when the characters in the buffer are stored in reverse order.
 * put is used when the characters are stored in normal order.
 */
#define unget_ch put_ch

/*
 * When there is no stack, the output will be the active diversion.
 * Otherwise, during argument collection, the output will be the store buffer.
 * (Definitions and argument strings in the store are referenced by the stack).
 */
#define output (m4->stack == NULL ? m4->div[m4->active_div] : m4->store)

/* Arguments that have not been collected reference the empty string */
#define arg(n) (m4->store->a + m4->mc.arg_i[n])


typedef struct m4_info *M4ptr;
typedef int (*Fptr)(M4ptr);


/* Hash table entry */
struct entry {
    char *name;                 /* Macro name */
    char *def;                  /* User-defined macro definition */
    Fptr mfp;                   /* Built-in Macro Function Pointer */
    struct entry *next;         /* To chain collisions */
};


/* All unget commands reverse the order of the characters */

struct buf {
    char *a;                    /* Memory */
    size_t i;                   /* Write index */
    size_t s;                   /* Allocated size in bytes */
};

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

struct m4_info {
    int req_exit_val;           /* User requested exit value */
    struct entry **ht;
    int read_stdin;
    /* There is only one input. Characters are stored in reverse order. */
    struct buf *input;
    struct buf *token;
    struct buf *store;          /* Stores strings referenced by the stack */
    struct macro_call *stack;
    /*
     * Copy of stack head node made before popping and processing, so that
     * output is redirected correctly.
     */
    struct macro_call mc;
    struct buf *tmp;            /* Used for substituting arguments */
    struct buf *div[NUM_DIVS];
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

struct entry *init_entry(void)
{
    struct entry *e;

    if ((e = malloc(sizeof(struct entry))) == NULL)
        mreturn(NULL);

    e->name = NULL;
    e->def = NULL;
    e->mfp = NULL;
    mreturn(e);
}

void free_entry(struct entry *e)
{
    if (e != NULL) {
        free(e->name);
        free(e->def);
        free(e);
    }
}

struct entry **init_ht(void)
{
    struct entry **ht;
    size_t i;

    if (mof(NUM_BUCKETS, sizeof(struct entry *)))
        mreturn(NULL);

    if ((ht = malloc(NUM_BUCKETS * sizeof(struct entry *))) == NULL)
        mreturn(NULL);

    for (i = 0; i < NUM_BUCKETS; ++i)
        ht[i] = NULL;

    mreturn(ht);
}

void free_ht(struct entry **ht)
{
    size_t i;
    struct entry *e, *e_next;

    if (ht != NULL) {
        for (i = 0; i < NUM_BUCKETS; ++i) {
            e = ht[i];
            while (e != NULL) {
                e_next = e->next;
                free_entry(e);
                e = e_next;
            }
        }
        free(ht);
    }
}

size_t hash_func(const char *str)
{
    /* djb2 */
    unsigned char ch;
    size_t h = 5381;

    while ((ch = *str) != '\0') {
        h = h * 33 ^ ch;
        ++str;
    }
    mreturn(h % NUM_BUCKETS);   /* Bucket index */
}

struct entry *lookup(struct entry **ht, const char *name)
{
    size_t bucket;
    struct entry *e;

    bucket = hash_func(name);
    e = ht[bucket];

    while (e != NULL) {
        if (!strcmp(name, e->name))
            mreturn(e);         /* Match */

        e = e->next;
    }
    mreturn(NULL_OK);           /* Not found */
}

int delete_entry(struct entry **ht, const char *name)
{
    size_t bucket;
    struct entry *e, *e_prev;

    bucket = hash_func(name);
    e = ht[bucket];
    e_prev = NULL;
    while (e != NULL) {
        if (!strcmp(name, e->name)) {
            /* Link around */
            if (e_prev != NULL)
                e_prev->next = e->next;
            else
                ht[bucket] = e->next;   /* At head of list */

            free_entry(e);
            mreturn(0);
        }
        e_prev = e;
        e = e->next;
    }

    mreturn(1);                 /* Not found */
}

int upsert(struct entry **ht, const char *name, const char *def, Fptr mfp)
{
    struct entry *e;
    size_t bucket;
    char *name_copy, *def_copy = NULL;

    e = lookup(ht, name);

    if (e == NULL) {
        /* Make a new entry */
        if ((e = init_entry()) == NULL)
            mreturn(1);

        if ((e->name = strdup(name)) == NULL) {
            free_entry(e);
            mreturn(1);
        }

        if (def != NULL && (e->def = strdup(def)) == NULL) {
            free_entry(e);
            mreturn(1);
        }

        e->mfp = mfp;

        /* Link in at the head of the bucket collision list */
        bucket = hash_func(name);
        e->next = ht[bucket];
        ht[bucket] = e;

    } else {
        /* Update the existing entry */
        if ((name_copy = strdup(name)) == NULL)
            mreturn(1);

        if (def != NULL && (def_copy = strdup(def)) == NULL) {
            free(name_copy);
            mreturn(1);
        }

        free(e->name);
        free(e->def);
        e->name = name_copy;
        e->def = def_copy;
    }
    mreturn(0);
}

struct buf *init_buf(void)
{
    struct buf *b;

    if ((b = malloc(sizeof(struct buf))) == NULL)
        mreturn(NULL);

    if ((b->a = malloc(BLOCK_SIZE)) == NULL)
        mreturn(NULL);

    b->i = 0;
    b->s = BLOCK_SIZE;
    mreturn(b);
}

void free_buf(struct buf *b)
{
    if (b != NULL) {
        free(b->a);
        free(b);
    }
}

int grow_buf(struct buf *b, size_t will_use)
{
    char *t;
    size_t new_s, num;

    if (aof(b->s, will_use))
        mreturn(1);

    new_s = b->s + will_use;

    if (mof(new_s, 2))
        mreturn(1);

    new_s *= 2;

    num = new_s / BLOCK_SIZE;

    if (aof(num, 1))
        mreturn(1);

    ++num;

    if (mof(num, BLOCK_SIZE))
        mreturn(1);

    new_s = num * BLOCK_SIZE;

    if ((t = realloc(b->a, new_s)) == NULL)
        mreturn(1);

    b->a = t;
    b->s = new_s;
    mreturn(0);
}

int put_ch(struct buf *b, char ch)
{
    if (b->i == b->s && grow_buf(b, 1))
        mreturn(1);

    *(b->a + b->i) = ch;
    ++b->i;
    mreturn(0);
}

int put_str(struct buf *b, const char *str)
{
    size_t len;

    len = strlen(str);
    if (!len)
        mreturn(0);

    if (len > b->s - b->i && grow_buf(b, len))
        mreturn(1);

    memcpy(b->a + b->i, str, len);
    b->i += len;
    mreturn(0);
}

int unget_str(struct buf *b, const char *str)
{
    size_t len, j;
    char *p;

    len = strlen(str);

    if (!len)
        mreturn(0);

    if (len > b->s - b->i && grow_buf(b, len))
        mreturn(1);

    p = b->a + b->i + len - 1;
    j = len;
    while (j) {
        *p = *str;
        --p;
        ++str;
        --j;
    }
    b->i += len;
    mreturn(0);
}

int put_buf(struct buf *b, struct buf *t)
{
    /* Empties t onto the end of b */
    if (t->i > b->s - b->i && grow_buf(b, t->i))
        mreturn(1);

    memcpy(b->a + b->i, t->a, t->i);
    b->i += t->i;
    t->i = 0;
    mreturn(0);
}

int put_file(struct buf *b, const char *fn)
{
    int ret = 1;
    FILE *fp = NULL;
    long fs_l;
    size_t fs;

    if (fn == NULL || *fn == '\0')
        mreturn(1);

    if ((fp = fopen(fn, "rb")) == NULL)
        mgoto(clean_up);

    if (fseek(fp, 0L, SEEK_END))
        mgoto(clean_up);

    if ((fs_l = ftell(fp)) == -1 || fs_l < 0)
        mgoto(clean_up);

    if (fseek(fp, 0L, SEEK_SET))
        mgoto(clean_up);

    if (!fs_l)
        mgoto(done);

    fs = (size_t) fs_l;

    if (fs > b->s - b->i && grow_buf(b, fs))
        mgoto(clean_up);

    if (fread(b->a + b->i, 1, fs, fp) != fs)
        mgoto(clean_up);

    b->i += fs;

  done:
    ret = 0;
  clean_up:
    if (fp != NULL)
        if (fclose(fp))
            ret = 1;

    mreturn(ret);
}

int unget_file(struct buf *b, const char *fn)
{
    int ret = 1;
    FILE *fp = NULL;
    long fs_l;
    size_t fs, j;
    char *p;
    int x;

    if (fn == NULL || *fn == '\0')
        mreturn(1);

    if ((fp = fopen(fn, "rb")) == NULL)
        mgoto(clean_up);

    if (fseek(fp, 0L, SEEK_END))
        mgoto(clean_up);

    if ((fs_l = ftell(fp)) == -1 || fs_l < 0)
        mgoto(clean_up);

    if (fseek(fp, 0L, SEEK_SET))
        mgoto(clean_up);

    if (!fs_l)
        mgoto(done);

    fs = (size_t) fs_l;

    if (fs > b->s - b->i && grow_buf(b, fs))
        mgoto(clean_up);

    p = b->a + b->i + fs - 1;
    j = fs;
    while (j) {
        if ((x = getc(fp)) == EOF)
            mgoto(clean_up);

        *p = x;
        --p;
        --j;
    }
    if (ferror(fp))
        mgoto(clean_up);

    b->i += fs;

  done:
    ret = 0;
  clean_up:
    if (fp != NULL && fclose(fp))
        ret = 1;

    mreturn(ret);
}

int write_buf(struct buf *b, const char *fn)
{
    /* Empties b to file fn */
    FILE *fp;

    if (fn == NULL || *fn == '\0')
        mreturn(1);

    if ((fp = fopen(fn, "wb")) == NULL)
        mreturn(1);

    if (fwrite(b->a, 1, b->i, fp) != b->i) {
        fclose(fp);
        mreturn(1);
    }
    if (fclose(fp))
        mreturn(1);

    b->i = 0;
    mreturn(0);
}

int flush_buf(struct buf *b)
{
    if (!b->i)
        mreturn(0);

    if (fwrite(b->a, 1, b->i, stdout) != b->i)
        mreturn(1);

    if (fflush(stdout))
        mreturn(1);

    b->i = 0;
    mreturn(0);
}

int get_ch(struct buf *input, char *ch, int read_stdin)
{
    int x;

    if (input->i) {
        --input->i;
        *ch = *(input->a + input->i);
        mreturn(0);
    }
    if (!read_stdin)
        mreturn(EOF);

    if ((x = getchar()) == EOF) {
        if (feof(stdin) && !ferror(stdin))
            mreturn(EOF);
        else
            mreturn(ERR);
    }
    *ch = x;
    mreturn(0);
}

int get_word(struct buf *input, struct buf *token, int read_stdin)
{
    int r;
    char ch, type;

    do {
        if ((r = get_ch(input, &ch, read_stdin)) != 0)
            mreturn(r);
    } while (ch == '\0' || ch == '\r'); /* Discard these chars */

    token->i = 0;

    if (put_ch(token, ch))
        mreturn(ERR);

    if (isdigit(ch))
        type = 'd';             /* Decimal number */
    else if (isalpha(ch) || ch == '_')  /* First char cannot be a digit */
        type = 'w';             /* Word */
    else
        mgoto(end);             /* Send a single char */

    while (1) {
        do {
            r = get_ch(input, &ch, read_stdin);
            if (r == ERR)
                mreturn(ERR);
            else if (r == EOF)  /* Ignore, as not the first char */
                mgoto(end);
        } while (ch == '\0' || ch == '\r');

        if ((type == 'd' && isdigit(ch))
            || (type == 'w' && (isalnum(ch) || ch == '_'))) {
            /* More of the same type. Words can include digits here. */
            if (put_ch(token, ch))
                mreturn(ERR);
        } else {
            if (unget_ch(input, ch))
                mreturn(ERR);

            mgoto(end);
        }
    }

  end:
    if (put_ch(token, '\0'))    /* Terminate string */
        mreturn(ERR);

    mreturn(0);
}

int eat_whitespace(struct buf *input, int read_stdin)
{
    int r;
    char ch;

    while (1) {
        r = get_ch(input, &ch, read_stdin);
        if (r == ERR)
            mreturn(1);
        else if (r == EOF)
            break;

        if (!(isspace(ch) || ch == '\0')) {
            if (unget_ch(input, ch))
                mreturn(1);

            break;
        }
    }
    mreturn(0);
}

int sub_args(M4ptr m4)
{
    const char *p;
    char ch, next_ch;
    size_t x;

    m4->tmp->i = 0;
    p = m4->store->a + m4->mc.def_i;

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
        free_buf(m4->input);
        free_buf(m4->token);
        free_buf(m4->store);
        free_mc_stack(&m4->stack);
        free_buf(m4->tmp);
        for (i = 0; i < NUM_DIVS; ++i)
            free_buf(m4->div[i]);

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
    m4->store = NULL;
    m4->stack = NULL;
    /* Used for substituting arguments */
    m4->tmp = NULL;
    for (i = 0; i < NUM_DIVS; ++i)
        m4->div[i] = NULL;

    if ((m4->ht = init_ht()) == NULL)
        mgoto(error);

    m4->read_stdin = 0;

    if ((m4->input = init_buf()) == NULL)
        mgoto(error);

    if ((m4->token = init_buf()) == NULL)
        mgoto(error);

    if ((m4->store = init_buf()) == NULL)
        mgoto(error);

    /* Setup empty string for uncollected args to reference at index 0 */
    if (put_ch(m4->store, '\0'))
        mgoto(error);

    if ((m4->tmp = init_buf()) == NULL)
        mgoto(error);

    for (i = 0; i < NUM_DIVS; ++i)
        if ((m4->div[i] = init_buf()) == NULL)
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

int str_to_size_t(const char *str, size_t *res)
{
    unsigned char ch;
    size_t x = 0;

    if (str == NULL || *str == '\0')
        mreturn(1);

    while ((ch = *str) != '\0') {
        if (isdigit(ch)) {
            if (mof(x, 10))
                return 1;

            x *= 10;
            if (aof(x, ch - '0'))
                return 1;

            x += ch - '0';
        } else {
            return 1;
        }

        ++str;
    }
    *res = x;
    return 0;
}

int define(M4ptr m4)
{
    /*@ define(macro_name, macro_def) */
    if (m4->mc.active_arg == 0) {
        /* Called without arguments, so pass through */
        if (put_str(output, m4->token->a))
            mreturn(1);

        mreturn(0);
    }

    if (m4->mc.active_arg != 2) /* Invalid number of arguments */
        mreturn(1);

    if (upsert(m4->ht, arg(1), arg(2), NULL))
        mreturn(1);

    mreturn(0);
}

int undefine(M4ptr m4)
{
    /*@ undefine(`macro_name') */
    if (m4->mc.active_arg == 0) {
        /* Called without arguments, so pass through */
        if (put_str(output, m4->token->a))
            mreturn(1);

        mreturn(0);
    }

    if (m4->mc.active_arg != 1) /* Invalid number of arguments */
        mreturn(1);

    if (delete_entry(m4->ht, arg(1)))
        mreturn(1);

    mreturn(0);
}

int changequote(M4ptr m4)
{
    /*@ changequote(left_quote, right_quote) */
    char l_ch, r_ch;

    if (m4->mc.active_arg == 0) {
        /* Called without arguments, so restore the defaults */
        m4->left_quote[0] = '`';
        m4->right_quote[0] = '\'';
        mreturn(0);
    }
    if (m4->mc.active_arg != 2)
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

int divert(M4ptr m4)
{
    /*@ divert or divert(div_num) */
    if (m4->mc.active_arg == 0) {
        m4->active_div = 0;
        mreturn(0);
    }
    if (m4->mc.active_arg != 1)
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

int undivert(M4ptr m4)
{
    /*@ undivert or undivert(div_num, filename, ...) */
    char ch;
    size_t i, x;

    if (m4->mc.active_arg == 0) {
        if (m4->active_div != 0)
            mreturn(1);

        for (i = 1; i < NUM_DIVS - 1; ++i)
            if (put_buf(m4->div[m4->active_div], m4->div[i]))
                mreturn(1);

        mreturn(0);
    }
    for (i = 1; i <= m4->mc.active_arg; ++i) {
        ch = *arg(i);
        if (ch == '\0') {
            mreturn(1);
        } else if (isdigit(ch) && strlen(arg(i)) == 1
                   && (x = ch - '0') != m4->active_div) {
            if (put_buf(m4->div[m4->active_div], m4->div[x]))
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

int writediv(M4ptr m4)
{
    /*@ writediv(div_num, filename) */
    char ch;

    if (m4->mc.active_arg == 0) {
        /* Called without arguments, so pass through */
        if (put_str(output, m4->token->a))
            mreturn(1);

        mreturn(0);
    }

    if (m4->mc.active_arg != 2)
        mreturn(1);

    ch = *arg(1);
    /* Cannot write diversions 0 and -1 */
    if (strlen(arg(1)) == 1 && isdigit(ch) && ch != '0') {
        if (write_buf(m4->div[ch - '0'], arg(2)))
            mreturn(1);
    } else {
        mreturn(1);
    }

    mreturn(0);
}

int divnum(M4ptr m4)
{
    /*@ divnum */
    char ch;

    if (m4->mc.active_arg != 0)
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

int include(M4ptr m4)
{
    /*@ include(filename) */
    if (m4->mc.active_arg == 0) {
        /* Called without arguments, so pass through */
        if (put_str(output, m4->token->a))
            mreturn(1);

        mreturn(0);
    }
    if (m4->mc.active_arg != 1)
        mreturn(1);

    if (unget_file(m4->input, arg(1)))
        mreturn(1);

    mreturn(0);
}

int dnl(M4ptr m4)
{
    /*@ dnl */
    /* Delete to NewLine (inclusive) */
    int r;
    char ch;

    if (m4->mc.active_arg != 0)
        mreturn(1);

    while (1) {
        r = get_ch(m4->input, &ch, m4->read_stdin);
        if (r == ERR)
            mreturn(1);
        else if (r == EOF)
            break;

        if (ch == '\n')
            break;
    }
    mreturn(0);
}

int tnl(M4ptr m4)
{
    /*@ tnl */
    /* Trim NewLine chars at the end of the first argument */
    char *p, *q, ch;

    if (m4->mc.active_arg == 0) {
        /* Called without arguments, so pass through */
        if (put_str(output, m4->token->a))
            mreturn(1);

        mreturn(0);
    }
    if (m4->mc.active_arg != 1)
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

int ifdef(M4ptr m4)
{
    /*@ ifdef(`macro_name', `when_defined', `when_undefined') */
    struct entry *e;

    if (m4->mc.active_arg == 0) {
        /* Called without arguments, so pass through */
        if (put_str(output, m4->token->a))
            mreturn(1);

        mreturn(0);
    }
    if (m4->mc.active_arg != 2 && m4->mc.active_arg != 3)
        mreturn(1);

    e = lookup(m4->ht, arg(1));
    if (e != NULL) {
        if (unget_str(m4->input, arg(2)))
            mreturn(1);
    } else {
        if (m4->mc.active_arg == 3 && unget_str(m4->input, arg(3)))
            mreturn(1);
    }
    mreturn(0);
}

int ifelse(M4ptr m4)
{
    /*@ ifelse(A, B, `when_same', `when_different') */
    if (m4->mc.active_arg == 0) {
        /* Called without arguments, so pass through */
        if (put_str(output, m4->token->a))
            mreturn(1);

        mreturn(0);
    }
    if (m4->mc.active_arg != 3 && m4->mc.active_arg != 4)
        mreturn(1);

    if (!strcmp(arg(1), arg(2))) {
        if (unget_str(m4->input, arg(3)))
            mreturn(1);
    } else {
        if (m4->mc.active_arg == 4 && unget_str(m4->input, arg(4)))
            mreturn(1);
    }
    mreturn(0);
}

int dumpdef(M4ptr m4)
{
    /*@ dumpdef or dumpdef(`macro_name', ...) */
    size_t i;
    struct entry *e;

    if (m4->mc.active_arg == 0) {
        /* Dump all macro definitions */
        for (i = 0; i < NUM_BUCKETS; ++i) {
            e = m4->ht[i];
            while (e != NULL) {
                fprintf(stderr, "%s: %s\n", e->name,
                        e->mfp == NULL ? e->def : "built-in");
                e = e->next;
            }
        }
        mreturn(0);
    }
    for (i = 1; i <= m4->mc.active_arg; ++i) {
        if (*arg(i) == '\0')
            mreturn(1);

        e = lookup(m4->ht, arg(i));
        if (e == NULL)
            fprintf(stderr, "%s: undefined\n", arg(i));
        else
            fprintf(stderr, "%s: %s\n", e->name,
                    e->mfp == NULL ? e->def : "built-in");
    }
    mreturn(0);
}

int errprint(M4ptr m4)
{
    /*@ errprint(error_message) */
    if (m4->mc.active_arg == 0) {
        /* Called without arguments, so pass through */
        if (put_str(output, m4->token->a))
            mreturn(1);

        mreturn(0);
    }
    if (m4->mc.active_arg != 1)
        mreturn(1);

    fprintf(stderr, "%s\n", arg(1));
    mreturn(0);
}

int incr(M4ptr m4)
{
    /*@ incr(number) */
    size_t x;
    char num[NUM_BUF_SIZE];
    int r;

    if (m4->mc.active_arg == 0) {
        /* Called without arguments, so pass through */
        if (put_str(output, m4->token->a))
            mreturn(1);

        mreturn(0);
    }
    if (m4->mc.active_arg != 1)
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

int sysval(M4ptr m4)
{
    /*@ sysval */
    if (m4->mc.active_arg != 0)
        mreturn(1);

    if (unget_str(m4->input, m4->store->a + m4->mc.def_i))
        mreturn(1);

    mreturn(0);
}

int esyscmd(M4ptr m4)
{
    /*@ esyscmd(shell_command) */
    struct entry *e;
    FILE *fp;
    int x, st, r;
    char num[NUM_BUF_SIZE];

    if (m4->mc.active_arg == 0) {
        /* Called without arguments, so pass through */
        if (put_str(output, m4->token->a))
            mreturn(1);

        mreturn(0);
    }
    if (m4->mc.active_arg != 1)
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

    if (e != NULL && e->mfp != NULL) {
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

int m4exit(M4ptr m4)
{
    /*@ m4exit(exit_value) */
    size_t x;

    if (m4->mc.active_arg == 0) {
        m4->req_exit_val = 0;
        mreturn(0);
    }
    if (m4->mc.active_arg != 1)
        mreturn(1);

    if (str_to_size_t(arg(1), &x))
        mreturn(1);

    if (x > UCHAR_MAX)
        mreturn(1);

    m4->req_exit_val = x;

    mreturn(0);
}

int remove_file(M4ptr m4)
{
    /*@ remove(filename) */
    /* Removes empty directories on some systems */
    if (m4->mc.active_arg == 0) {
        /* Called without arguments, so pass through */
        if (put_str(output, m4->token->a))
            mreturn(1);

        mreturn(0);
    }
    if (m4->mc.active_arg != 1)
        mreturn(1);

    if (*arg(1) == '\0')
        mreturn(1);

    if (remove(arg(1)))
        mreturn(1);

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

    /* Used to read the next token to see if it is an open bracket */
    struct buf *next_token = NULL;
    int i, r;

#ifdef _WIN32
    if (_setmode(_fileno(stdin), _O_BINARY) == -1)
        mreturn(1);

    if (_setmode(_fileno(stdout), _O_BINARY) == -1)
        mreturn(1);

    if (_setmode(_fileno(stderr), _O_BINARY) == -1)
        mreturn(1);
#endif

    if ((m4 = init_m4()) == NULL)
        mgoto(clean_up);

    if ((next_token = init_buf()) == NULL)
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

    if (upsert(m4->ht, "esyscmd", NULL, &esyscmd))
        mgoto(clean_up);

    if (upsert(m4->ht, "sysval", NULL, &sysval))
        mgoto(clean_up);

    if (upsert(m4->ht, "m4exit", NULL, &m4exit))
        mgoto(clean_up);

    if (upsert(m4->ht, "remove", NULL, &remove_file))
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

        if (flush_buf(m4->div[0]))
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

          macro_end:
            /* Copy stack head and pop first, so that output is correct */
            memcpy(&m4->mc, m4->stack, sizeof(struct macro_call));
            m4->mc.next = NULL; /* Should not be used */

            /* Truncate store */
            m4->store->i = m4->stack->def_i;

            pop_mc(&m4->stack);

            if (m4->mc.mfp != NULL) {
                if ((*m4->mc.mfp) (m4)) {
                    fprintf(stderr, "m4: %s: Failed\n",
                            m4->store->a + m4->mc.arg_i[0]);
                    goto clean_up;
                }
            } else {
                if (sub_args(m4))
                    mgoto(clean_up);
            }
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
                /* See if called with or with brackts */
                r = get_word(m4->input, next_token, m4->read_stdin);
                if (r == ERR)
                    mgoto(clean_up);

                if (stack_mc(&m4->stack))
                    mgoto(clean_up);

                m4->stack->bracket_depth = 1;
                m4->stack->mfp = e->mfp;
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

                if (r == EOF || strcmp(next_token->a, "(")) {
                    /* Called without arguments */
                    if (r != EOF && unget_str(m4->input, next_token->a))
                        mgoto(clean_up);

                    goto macro_end;
                }

                ++m4->stack->active_arg;
                m4->stack->arg_i[m4->stack->active_arg] = m4->store->i;

                /* Ready to collect arg 1 */
                if (eat_whitespace(m4->input, m4->read_stdin))
                    mgoto(clean_up);
            }
        }
    }

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
        flush_buf(m4->div[i]);

    ret = 0;
  clean_up:
    free_m4(m4);
    free_buf(next_token);

    if (req_exit_val != -1 && !ret)
        mreturn(req_exit_val);

    mreturn(ret);
}
