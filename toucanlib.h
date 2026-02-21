/*
 * Copyright (c) 2023-2025 Logan Ryan McLintock. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef TOUCANLIB_H
#define TOUCANLIB_H

#ifdef __linux__

/* For: strdup and snprintf */
#ifndef _XOPEN_SOURCE
#define _XOPEN_SOURCE 500
#endif

/* For: DT_DIR and DT_UNKNOWN */
#ifndef _DEFAULT_SOURCE
#define _DEFAULT_SOURCE
#endif

#endif

#ifdef _WIN32

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#ifndef _CRT_RAND_S
#define _CRT_RAND_S
#endif

#endif

#include <sys/stat.h>
#include <sys/types.h>

#ifdef _WIN32
#include <Windows.h>
#include <direct.h>
#include <fcntl.h>
#include <io.h>
#include <process.h>
#else
#include <dirent.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#endif

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <locale.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#define popen  _popen
#define pclose _pclose
#endif

#ifdef _WIN32
#define stat_f _stat64
#define stat_s __stat64
#else
#define stat_f stat
#define stat_s stat
#endif

#ifndef S_ISREG
#define S_ISREG(m) (((m) & S_IFMT) == S_IFREG)
#endif

#ifndef S_ISDIR
#define S_ISDIR(m) (((m) & S_IFMT) == S_IFDIR)
#endif

#ifndef S_ISLNK
#define S_ISLNK(m) (((m) & S_IFMT) == S_IFLNK)
#endif

#ifndef _WIN32
#define mkdir(dir) mkdir(dir, S_IRWXU)
#endif

#define mreturn(rv)                                                           \
    do {                                                                      \
        fprintf(stderr, "[%s:%d]: Error: " #rv "\n", __FILE__, __LINE__);     \
        return (rv);                                                          \
    } while (0)

#define d_mreturn(msg, rv)                                                    \
    do {                                                                      \
        fprintf(stderr, "[%s:%d]: " msg " error\n", __FILE__, __LINE__);      \
        return (rv);                                                          \
    } while (0)

#define mgoto(lab)                                                            \
    do {                                                                      \
        fprintf(stderr, "[%s:%d]: Error: " #lab "\n", __FILE__, __LINE__);    \
        goto lab;                                                             \
    } while (0)

#define d_mgoto(lab, ...)                                                     \
    do {                                                                      \
        fprintf(stderr, "[%s: %d]: " #lab ": ", __FILE__, __LINE__);          \
        fprintf(stderr, __VA_ARGS__);                                         \
        goto lab;                                                             \
    } while (0)

/* Error codes */

/*
 * 0 = Success.
 * 1 = Error.
 */

/* Success return codes */
#define MATCH         2
#define PARTIAL_MATCH 3

/* Error codes */
#define ERROR_BUT_CONTIN    4
#define NO_MATCH            5
#define SYNTAX_ERROR        6
#define DIV_BY_ZERO_ERROR   7
#define USER_OVERFLOW_ERROR 8
#define USAGE_ERROR         9
#define NO_HISTORY          10

#ifdef _WIN32
#define DIR_SEP_STR "\\"
#else
#define DIR_SEP_STR "/"
#endif

#define TAB_SIZE 8

/* For printing a number as a string */
#define NUM_BUF_SIZE 32

#define NUM_OPERATORS 25

/* Parentheses */
#define LEFT_PARENTHESIS  0
#define RIGHT_PARENTHESIS 1

/* Unary operators */
#define POSITIVE           2
#define NEGATIVE           3
#define BITWISE_COMPLEMENT 4
#define LOGICAL_NEGATION   5

/* Binary operators */
#define EXPONENTIATION        6
#define MULTIPLICATION        7
#define DIVISION              8
#define MODULO                9
#define ADDITION              10
#define SUBTRACTION           11
#define BITWISE_LEFT_SHIFT    12
#define BITWISE_RIGHT_SHIFT   13
#define LESS_THAN             14
#define LESS_THAN_OR_EQUAL    15
#define GREATER_THAN          16
#define GREATER_THAN_OR_EQUAL 17
#define EQUAL                 18
#define NOT_EQUAL             19
#define BITWISE_AND           20
#define BITWISE_XOR           21
#define BITWISE_OR            22
#define LOGICAL_AND           23
#define LOGICAL_OR            24

/* Stringify. Converts text to a string literal. */
#define sf(text) #text
/* Expanded stringify. Stringifies the definition of macro_name. */
#define esf(macro_name) sf(macro_name)

/* Concatenation */
#define conc(a, b) a##b
/* Epanded concatenation */
#define econc(a, b) conc(a, b)

/* attr must be an unsigned char */
#define IS_DIR(attr)    ((attr) & 1)
#define IS_SLINK(attr)  ((attr) & 1 << 1)
#define IS_DOTDIR(attr) ((attr) & 1 << 2)

/* Unsigned addition overflow test */
#define aof(a, b, max_val) ((a) > (max_val) - (b))

/* Unsigned multiplication overflow test */
#define mof(a, b, max_val) ((a) && (b) > (max_val) / (a))

/* Inputs need to be isxdigit */
#define hex_nibble(h) (((h) & 0x0F) + ((h) & 0x40 ? 9 : 0))
#define hex(h1, h0)   (hex_nibble(h1) << 4 | hex_nibble(h0))

#define start_of_gb(b) while (!left_ch(b))
#define end_of_gb(b)   while (!right_ch(b))

#define C(l) ((l) - 'a' + 1)

typedef int (*Fptr)(void *);

/*
 * Input buffer: Characters are stored in reverse order.
 * Unget file links in a new struct at the head.
 * Operated on by get and unget functions.
 */
struct ibuf {
    char *nm;    /* Associated filename or name of stream */
    FILE *fp;    /* File pointer */
    int incr_rn; /* Increment row number next character */
    /*
     * Row number of character read from file, starting from 1.
     * \n is treated as at the end of the line, so the row number will not
     * increment until the character after is read.
     */
    size_t rn;
    char *a;           /* Memory */
    size_t i;          /* Write index */
    size_t n;          /* Allocated number of elements */
    struct ibuf *next; /* Link to next struct */
};

/*
 * Ouput buffer: Characters are stored in normal order.
 * Operated on by put functions.
 */
struct obuf {
    char *a;  /* Memory */
    size_t i; /* Write index */
    size_t n; /* Allocated number of elements */
};

struct lbuf {
    long *a;  /* Memory */
    size_t i; /* Write index */
    size_t n; /* Allocated number of elements */
};

struct sbuf {
    size_t *a; /* Memory */
    size_t i;  /* Write index */
    size_t n;  /* Allocated number of elements */
};

struct pbuf {
    void **a; /* Memory */
    size_t i; /* Write index */
    size_t n; /* Allocated number of elements */
};

/* Gap buffer atomic operation */
struct atomic_op {
    /*
     * A "group" is a sequence of atomic operations that are undone together.
     * 'S' = Start group.
     * 'E' = End group.
     * 'I' = Insert ch.
     * 'D' = Delete ch.
     */
    unsigned char id;
    /*
     * Start of gap location.
     * The gap will change in size, but g remains comparable.
     */
    size_t g_loc;
    char ch;
};

struct op_buf {
    struct atomic_op *a; /* Memory */
    size_t i;            /* Index of next free element */
    size_t n;            /* Number of allocated elements */
};

struct gb {
    char *fn;
    unsigned char *a;
    size_t g;   /* Gap start */
    size_t c;   /* Cursor */
    size_t e;   /* End of buffer */
    int m_set;  /* Mark set */
    size_t m;   /* Mark */
    size_t r;   /* Row number (starts from 1) */
    size_t col; /* Column number (starts from 1) */
    int sc_set; /* Sticky column set */
    size_t sc;  /* Sticky column for repeated up and down */
    size_t d;   /* Draw start */
    int mod;    /* Modified */
    /* 0 = Redo or normal operation, 1 = Undo in progress */
    unsigned char mode;  /* 'N' = Normal, 'U' = Undo, 'R' = redo */
    struct op_buf *undo; /* Undo buffer */
    struct op_buf *redo; /* Redo buffer */
    struct gb *prev;
    struct gb *next;
};

/* Hash table entry */
struct entry {
    char *name;         /* Macro name */
    char *def;          /* User-defined macro definition */
    Fptr func_p;        /* Function Pointer */
    struct entry *hist; /* For entry history */
    struct entry *prev; /* Previous entry in collision chain */
    struct entry *next; /* Next entry in collision chain */
};

/* Hash table */
struct ht {
    struct entry **b; /* Buckets */
    size_t n;         /* Number of buckets */
};

/* Function declarations */
int binary_io(void);
char *concat(const char *str, ...);
void *quick_search(
    const void *mem, size_t mem_len, const void *find, size_t find_len);
FILE *fopen_w(const char *fn, int append);
int tty_check(FILE *stream, int *is_tty);
int milli_sleep(long milliseconds);
int random_uint(unsigned int *x);
int random_num(unsigned int max_inclusive, unsigned int *x);
int str_to_num(const char *str, unsigned long max_val, unsigned long *res);
int str_to_size_t(const char *str, size_t *res);
int str_to_uint(const char *str, unsigned int *res);
int hex_to_val(unsigned char h1, unsigned char h0, unsigned char *res);
int lop(long *a, long b, unsigned char op);
int lpow(long *a, long b);
char *ltostr(long a, unsigned int base, unsigned int pad);
struct ibuf *init_ibuf(size_t n);
int free_ibuf(struct ibuf *b);
int unget_ch(struct ibuf *b, char ch);
int unget_str(struct ibuf *b, const char *str);
int unget_stream(struct ibuf **b, FILE *fp, const char *nm);
int unget_file(struct ibuf **b, const char *fn);
int append_stream(struct ibuf **b, FILE *fp, const char *nm);
int append_file(struct ibuf **b, const char *fn);
int get_ch(struct ibuf **input, char *ch);
int eat_whitespace(struct ibuf **input);
int delete_to_nl(struct ibuf **input);
int eat_str_if_match(struct ibuf **input, const char *str);
int get_word(struct ibuf **input, struct obuf *token, int interpret_hex);
struct obuf *init_obuf(size_t n);
void free_obuf(struct obuf *b);
void free_obuf_exterior(struct obuf *b);
int put_ch(struct obuf *b, char ch);
int put_str(struct obuf *b, const char *str);
int put_mem(struct obuf *b, const char *mem, size_t mem_len);
int put_obuf(struct obuf *b, struct obuf *t);
int put_file(struct obuf *b, const char *fn);
int put_stream(struct obuf *b, FILE *fp);
int write_obuf(struct obuf *b, const char *fn, int append);
int flush_obuf(struct obuf *b, int tty_output);
char *obuf_to_str(struct obuf **b);
struct lbuf *init_lbuf(size_t n);
void free_lbuf(struct lbuf *b);
int add_l(struct lbuf *b, long x);
struct sbuf *init_sbuf(size_t n);
void free_sbuf(struct sbuf *b);
int add_s(struct sbuf *b, size_t x);
struct pbuf *init_pbuf(size_t n);
void free_pbuf(struct pbuf *b);
int add_p(struct pbuf *b, void *x);
int reverse(struct gb *b, unsigned char mode);
void free_gb(struct gb *b);
struct gb *init_gb(size_t s);
void free_gb_list(struct gb *b);
void reset_gb(struct gb *b);
int insert_ch(struct gb *b, char ch);
int insert_str(struct gb *b, const char *str);
int insert_mem(struct gb *b, const char *mem, size_t mem_len);
int insert_file(struct gb *b, const char *fn);
int delete_ch(struct gb *b);
int left_ch(struct gb *b);
int right_ch(struct gb *b);
int backspace_ch(struct gb *b);
void start_of_line(struct gb *b);
void end_of_line(struct gb *b);
int up_line(struct gb *b);
int down_line(struct gb *b);
void left_word(struct gb *b);
int right_word(struct gb *b, char transform);
int goto_row(struct gb *b, struct gb *cl);
int insert_hex(struct gb *b, struct gb *cl);
void set_mark(struct gb *b);
int swap_cursor_and_mark(struct gb *b);
int exact_forward_search(struct gb *b, struct gb *cl);
int regex_forward_search(struct gb *b, struct gb *cl, int case_ins);
int regex_replace_region(struct gb *b, struct gb *cl, int case_ins);
int match_bracket(struct gb *b);
int trim_clean(struct gb *b);
int copy_region(struct gb *b, struct gb *p, int cut);
int cut_to_eol(struct gb *b, struct gb *p);
int cut_to_sol(struct gb *b, struct gb *p);
int word_under_cursor(struct gb *b, struct gb *tmp);
int copy_logical_line(struct gb *b, struct gb *tmp);
int insert_shell_cmd(struct gb *b, const char *cmd, int *es);
int shell_line(struct gb *b, struct gb *tmp, int *es);
int paste(struct gb *b, struct gb *p);
int save(struct gb *b);
int rename_gb(struct gb *b, const char *fn);
int new_gb(struct gb **b, const char *fn, size_t s);
void remove_gb(struct gb **b);
int eval_ibuf(struct ibuf **input, long *res, int verbose);
int eval_str(const char *math_str, long *res, int verbose);
struct ht *init_ht(size_t num_buckets);
void free_ht(struct ht *ht);
struct entry *lookup(struct ht *ht, const char *name);
int delete_entry(struct ht *ht, const char *name, int pop_hist);
int upsert(struct ht *ht, const char *name, const char *def, Fptr func_p,
    int push_hist);
int regex_search(const char *text, size_t text_size, int sol,
    const char *regex_str, int nl_ins, int case_ins, size_t *match_offset,
    size_t *match_len, int verbose);
int regex_replace(const char *text, size_t text_size, const char *regex_str,
    int nl_ins, int case_ins, const char *replace_str, char **result,
    size_t *result_len, int verbose);
int get_file_size(const char *fn, size_t *fs);
int get_path_attr(const char *path, unsigned char *attr);
int rec_rm(const char *path);
char *ls_dir(const char *dir);
int mmap_file_ro(const char *fn, void **mem, size_t *fs);
int un_mmap(void *p, size_t s);
int make_temp(const char *template, char **temp_fn);
int make_stemp(const char *template, char **temp_fn);

#endif
