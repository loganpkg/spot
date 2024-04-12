/*
 * Copyright (c) 2024 Logan Ryan McLintock
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

#ifndef TOUCANLIB_H
#define TOUCANLIB_H


#ifdef __linux__
/* For: strdup and snprintf */
#define _XOPEN_SOURCE 500
/* For: cfmakeraw and DT_DIR and DT_UNKNOWN */
#define _DEFAULT_SOURCE
#endif

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#endif

#include <sys/types.h>
#include <sys/stat.h>

#ifdef _WIN32
/* For: _getch */
#include <conio.h>
/* For terminal functions */
#include <Windows.h>
#include <direct.h>
#include <io.h>
#include <fcntl.h>
#else
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <dirent.h>
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#endif

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdarg.h>
/* For: size_t */
#include <stddef.h>
#include <stdint.h>
/* For: FILE */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


#ifdef _WIN32
#define popen _popen
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


#ifdef _WIN32
#define DIR_SEP_STR "\\"
#else
#define DIR_SEP_STR "/"
#endif


/* Success return codes */
#define SUCCESS 0
#define MATCH SUCCESS
/* Used to not trigger the debugging */
#define NULL_OK NULL

/*
 * Error codes:
 * EOF is is negative.
 */

/* All system related errors return this: */
#define ERR 1

/* User related errors are distinguished as follows: */
#define NO_MATCH 2
#define SYNTAX_ERR 3
#define DIV_BY_ZERO_ERR 4
#define USER_OVERFLOW_ERR 5
#define USAGE_ERR 6


/* For printing a number as a string */
#define NUM_BUF_SIZE 32

#define LEFT_KEY  (UCHAR_MAX + 1)
#define RIGHT_KEY (UCHAR_MAX + 2)
#define UP_KEY    (UCHAR_MAX + 3)
#define DOWN_KEY  (UCHAR_MAX + 4)
#define DEL_KEY   (UCHAR_MAX + 5)
#define HOME_KEY  (UCHAR_MAX + 6)
#define END_KEY   (UCHAR_MAX + 7)

#define ESC 27

#define TAB_SIZE 4

/* Stringify. Converts text to a string literal. */
#define sf(text) #text
/* Expanded stringify. Stringifies the definition of macro_name. */
#define esf(macro_name) sf(macro_name)


/* attr must be an unsigned char */
#define IS_DIR(attr) ((attr) & 1)
#define IS_SLINK(attr) ((attr) & 1 << 1)
#define IS_DOTDIR(attr) ((attr) & 1 << 2)


/* Unsigned overflow tests */
/* Addition */
#define aof(a, b, max_val) ((a) > (max_val) - (b))

/* Multiplication */
#define mof(a, b, max_val) ((a) && (b) > (max_val) / (a))

#define start_of_gb(b) while (!left_ch(b))
#define end_of_gb(b) while (!right_ch(b))


#define C(l) ((l) - 'a' + 1)

#define phy_move(pos) printf("\x1B[%lu;%luH", \
    (unsigned long) ((pos) / s->w + 1),       \
    (unsigned long) ((pos) % s->w + 1))

#define phy_hl_off() printf("\x1B[m")

/* Does not toggle */
#define phy_hl_on() printf("\x1B[7m")

#define phy_clear() printf("\x1B[2J\x1B[1;1H")

#define C(l) ((l) - 'a' + 1)

#define phy_move(pos) printf("\x1B[%lu;%luH", \
    (unsigned long) ((pos) / s->w + 1),       \
    (unsigned long) ((pos) % s->w + 1))

#define phy_hl_off() printf("\x1B[m")

/* Does not toggle */
#define phy_hl_on() printf("\x1B[7m")

#define phy_clear() printf("\x1B[2J\x1B[1;1H")

typedef int (*Fptr)(void *);

/*
 * Input buffer. Characters are stored in reverse order.
 * Operated on by get and unget functions.
 */
struct ibuf {
    char *a;                    /* Memory */
    size_t i;                   /* Write index */
    size_t s;                   /* Allocated size in bytes */
};

/*
 * Ouput buffer. Characters are stored in normal order.
 * Operated on by put functions.
 */
struct obuf {
    char *a;                    /* Memory */
    size_t i;                   /* Write index */
    size_t s;                   /* Allocated size in bytes */
};

/* Buffer of long integers */
struct lbuf {
    long *a;                    /* Memory */
    size_t i;                   /* Write index */
    size_t n;                   /* Number of elements, not bytes */
};

/* Buffer of pointers */
struct pbuf {
    void **a;                   /* Array of pointers */
    size_t i;                   /* Write index */
    size_t n;                   /* Number of elements, not bytes */
};

struct gb {
    char *fn;
    unsigned char *a;
    size_t g;                   /* Gap start */
    size_t c;                   /* Cursor */
    size_t e;                   /* End of buffer */
    int m_set;                  /* Mark set */
    size_t m;                   /* Mark */
    size_t r;                   /* Row number (starts from 1) */
    size_t col;                 /* Column number (starts from 0) */
    size_t d;                   /* Draw start */
    int mod;                    /* Modified */
    struct gb *prev;
    struct gb *next;
};

/* Hash table entry */
struct entry {
    char *name;                 /* Macro name */
    char *def;                  /* User-defined macro definition */
    Fptr func_p;                /* Function Pointer */
    struct entry *next;         /* To chain collisions */
};

/* Hash table */
struct ht {
    struct entry **b;           /* Buckets */
    size_t n;                   /* Number of buckets */
};

struct screen {
#ifdef _WIN32
    HANDLE term_handle;
    DWORD term_orig;
    DWORD term_new;
#else
    struct termios term_orig;
    struct termios term_new;
#endif
    size_t h;                   /* Physical screen heigth */
    size_t w;                   /* Physical screen width */
    unsigned char *vs_c;        /* Current virtual screen */
    unsigned char *vs_n;        /* Next virtual screen */
    size_t vs_s;                /* Size of each virtual screen */
    size_t v_i;                 /* Virtual cursor index (print location) */
    int v_hl;                   /* Virtual highlight mode indicator */
    int clear;                  /* Clear physical screen */
    int centre;                 /* Draw cursor on the centre row */
};


/* Function declarations */
int sane_io(void);
char *concat(const char *str, ...);
void *quick_search(const void *mem, size_t mem_len, const void *find,
                   size_t find_len);
FILE *fopen_w(const char *fn);
int str_to_num(const char *str, unsigned long max_val, unsigned long *res);
int str_to_size_t(const char *str, size_t *res);
int hex_to_val(unsigned char h[2], unsigned char *res);
int lop(long *a, long b, char op);
int lpow(long *a, long b);
struct ibuf *init_ibuf(size_t s);
struct obuf *init_obuf(size_t s);
void free_ibuf(struct ibuf *b);
void free_obuf(struct obuf *b);
int unget_ch(struct ibuf *b, char ch);
int put_ch(struct obuf *b, char ch);
int unget_str(struct ibuf *b, const char *str);
int unget_file(struct ibuf *b, const char *fn);
int get_ch(struct ibuf *input, char *ch, int read_stdin);
int eat_str_if_match(struct ibuf *input, const char *str, int read_stdin);
int get_word(struct ibuf *input, struct obuf *token, int read_stdin);
int eat_whitespace(struct ibuf *input, int read_stdin);
int delete_to_nl(struct ibuf *input, int read_stdin);
int put_str(struct obuf *b, const char *str);
int put_mem(struct obuf *b, const char *mem, size_t mem_len);
int put_obuf(struct obuf *b, struct obuf *t);
int put_file(struct obuf *b, const char *fn);
int write_obuf(struct obuf *b, const char *fn);
int flush_obuf(struct obuf *b);
char *obuf_to_str(struct obuf **b);
struct lbuf *init_lbuf(size_t n);
void free_lbuf(struct lbuf *b);
int add_l(struct lbuf *b, long x);
struct pbuf *init_pbuf(size_t n);
void free_pbuf(struct pbuf *b);
int add_p(struct pbuf *b, void *ptr);
struct gb *init_gb(size_t s);
void free_gb(struct gb *b);
void free_gb_list(struct gb *b);
void delete_gb(struct gb *b);
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
void right_word(struct gb *b, char transform);
int goto_row(struct gb *b, struct gb *cl);
int insert_hex(struct gb *b, struct gb *cl);
int swap_cursor_and_mark(struct gb *b);
int exact_forward_search(struct gb *b, struct gb *cl);
int regex_forward_search(struct gb *b, struct gb *cl);
int regex_replace_region(struct gb *b, struct gb *cl);
int match_bracket(struct gb *b);
void trim_clean(struct gb *b);
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
int eval(struct ibuf *input, int read_stdin, long *res, int verbose);
int eval_str(const char *math_str, long *res, int verbose);
struct ht *init_ht(size_t num_buckets);
void free_ht(struct ht *ht);
struct entry *lookup(struct ht *ht, const char *name);
int delete_entry(struct ht *ht, const char *name);
int upsert(struct ht *ht, const char *name, const char *def, Fptr func_p);
int regex_search(const char *mem, size_t mem_len,
                 const char *regex_find_str, int sol,
                 int nl_sen, size_t *match_offset, size_t *match_len);
int regex_replace(const char *mem, size_t mem_len,
                  const char *regex_find_str, const char *replace,
                  size_t replace_len, int nl_sen, char **res,
                  size_t *res_len, int verbose);
struct screen *init_screen(void);
int free_screen(struct screen *s);
int get_key(void);
int erase_screen(struct screen *s);
void refresh_screen(struct screen *s);
int print_ch(struct screen *s, unsigned char ch);
int print_object(struct screen *s, size_t y, size_t x, const char *object);
int get_file_size(const char *fn, size_t *fs);
int get_path_attr(const char *path, unsigned char *attr);
int rec_rm(const char *path);
char *ls_dir(const char *dir);
int mmap_file_ro(const char *fn, void **mem, size_t *fs);
int un_mmap(void *p, size_t s);

#endif
