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

/* File system related module */

#include "toucanlib.h"


#define INIT_LS_ENTRY_NUM 512
#define INIT_BUF_SIZE 1024

/* attr must be an unsigned char */
#define SET_DIR(attr) ((attr) |= 1)
#define SET_SLINK(attr) ((attr) |= 1 << 1)
#define SET_DOTDIR(attr) ((attr) |= 1 << 2)


struct ls_info {
    struct pbuf *d;             /* To store pointers to directory names */
    struct pbuf *f;             /* To store pointers to filenames */
};

int get_file_size(const char *fn, size_t *fs)
{
    struct stat_s st;
    if (stat_f(fn, &st) == -1)
        mreturn(ERROR);

    if (!S_ISREG(st.st_mode))
        mreturn(ERROR);

    if (st.st_size < 0)
        mreturn(ERROR);

    *fs = (size_t) st.st_size;

    return 0;
}

int get_path_attr(const char *path, unsigned char *attr)
{
    unsigned char t = '\0';
#ifdef _WIN32
    DWORD file_attr;
#else
    struct stat_s st;
#endif

#ifdef _WIN32
    if ((file_attr = GetFileAttributes(path)) == INVALID_FILE_ATTRIBUTES)
        mreturn(ERROR);

    if (file_attr & FILE_ATTRIBUTE_DIRECTORY)
        SET_DIR(t);

    if (file_attr & FILE_ATTRIBUTE_REPARSE_POINT)
        SET_SLINK(t);
#else
    if (lstat(path, &st))
        mreturn(ERROR);

    if (S_ISDIR(st.st_mode))
        SET_DIR(t);

    if (S_ISLNK(st.st_mode))
        SET_SLINK(t);
#endif

    if (!strcmp(path, ".") || !strcmp(path, ".."))
        SET_DOTDIR(t);

    *attr = t;
    return 0;
}


int walk_dir_inner(const char *dir, int rec, void *info,
                   int (*func_p)(const char *, unsigned char, void *info))
{
    /* Does not execute the function on dir itself */
    int ret = ERROR;

#ifdef _WIN32
    HANDLE h = INVALID_HANDLE_VALUE;
    WIN32_FIND_DATA d;
    char *dir_wc = NULL;
#else
    DIR *h = NULL;
    struct dirent *d;
#endif
    const char *fn;
    char *path = NULL;
    unsigned char attr;

#ifdef _WIN32
    if ((dir_wc = concat(dir, "\\*", NULL)) == NULL) {
        ret = ERROR;
        mgoto(clean_up);
    }

    if ((h = FindFirstFile(dir_wc, &d)) == INVALID_HANDLE_VALUE) {
        ret = ERROR;
        mgoto(clean_up);
    }
#else
    if ((h = opendir(dir)) == NULL) {
        ret = ERROR;
        mgoto(clean_up);
    }
#endif

    while (1) {
#ifndef _WIN32
        errno = 0;
        if ((d = readdir(h)) == NULL)
            break;
#endif

#ifdef _WIN32
        fn = d.cFileName;
#else
        fn = d->d_name;
#endif
        if ((path = concat(dir, DIR_SEP_STR, fn, NULL)) == NULL) {
            ret = ERROR;
            mgoto(clean_up);
        }

        attr = 0;
#ifdef _WIN32
        if (d.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
            SET_DIR(attr);

        if (d.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT)
            SET_SLINK(attr);
#else
        if (d->d_type == DT_DIR)
            SET_DIR(attr);

        if (d->d_type == DT_LNK)
            SET_SLINK(attr);

        if (d->d_type == DT_UNKNOWN)
            if (get_path_attr(path, &attr)) {
                ret = ERROR;
                mgoto(clean_up);
            }
#endif
        if (!strcmp(fn, ".") || !strcmp(fn, ".."))
            SET_DOTDIR(attr);

        if (rec && IS_DIR(attr) && !IS_SLINK(attr) && !IS_DOTDIR(attr))
            if (walk_dir_inner(path, rec, info, func_p)) {      /* Recurse */
                ret = ERROR;
                mgoto(clean_up);
            }

        if ((*func_p) (rec ? path : fn, attr, info)) {
            ret = ERROR;
            mgoto(clean_up);
        }

        free(path);
        path = NULL;
#ifdef _WIN32
        if (!FindNextFile(h, &d))
            break;
#endif
    }

#ifdef _WIN32
    if (GetLastError() != ERROR_NO_MORE_FILES) {
        ret = ERROR;
        mgoto(clean_up);
    }
#else
    if (errno) {
        ret = ERROR;
        mgoto(clean_up);
    }
#endif

    ret = 0;

  clean_up:

#ifdef _WIN32
    if (h != INVALID_HANDLE_VALUE && !FindClose(h))
        ret = ERROR;

    if (dir_wc != NULL)
        free(dir_wc);
#else
    if (closedir(h))
        ret = ERROR;
#endif
    if (path != NULL)
        free(path);

    return ret;
}

int walk_dir(const char *dir, int rec, void *info,
             int (*func_p)(const char *, unsigned char, void *info))
{
    /* Executes the function on dir itself too */
    unsigned char attr;

    if (walk_dir_inner(dir, rec, info, func_p))
        mreturn(ERROR);

    /* Process outer directory */
    attr = 0;
    SET_DIR(attr);              /* Not sure if outer dir is a symbolic link */
    if (!strcmp(dir, ".") || !strcmp(dir, ".."))
        SET_DOTDIR(attr);

    if ((*func_p) (dir, attr, info))
        mreturn(ERROR);

    return 0;
}

static int rm_path(const char *path, unsigned char attr, void *info)
{
    /* Not used in this function */
    if (info != NULL)
        mreturn(ERROR);

    if (!IS_DOTDIR(attr)) {
        if (IS_DIR(attr)) {
            if (rmdir(path))
                mreturn(ERROR);
        } else {
            if (unlink(path))
                mreturn(ERROR);
        }
    }
    return 0;
}

int rec_rm(const char *path)
{
    errno = 0;
    if (unlink(path) == 0)      /* Success */
        return 0;

    if (errno == ENOENT)
        return 0;

    return walk_dir(path, 1, NULL, &rm_path);
}

static int add_fn(const char *fn, unsigned char attr, void *info)
{
    struct ls_info *lsi;
    char *fn_copy;
    lsi = (struct ls_info *) info;

    if ((fn_copy = strdup(fn)) == NULL)
        mreturn(ERROR);

    if (IS_DIR(attr)) {
        if (add_p(lsi->d, fn_copy)) {
            free(fn_copy);
            mreturn(ERROR);
        }
    } else {
        if (add_p(lsi->f, fn_copy)) {
            free(fn_copy);
            mreturn(ERROR);
        }
    }

    return 0;
}

static int order_func(const void *a, const void *b)
{
    return strcmp(*(const char **) a, *(const char **) b);
}

char *ls_dir(const char *dir)
{
    int err = ERROR;
    struct ls_info *lsi = NULL;
    struct obuf *b = NULL;
    char *t = NULL;
    size_t j;

    if ((lsi = calloc(1, sizeof(struct ls_info))) == NULL) {
        err = ERROR;
        mgoto(clean_up);
    }

    if ((lsi->d = init_pbuf(INIT_LS_ENTRY_NUM)) == NULL) {
        err = ERROR;
        mgoto(clean_up);
    }

    if ((lsi->f = init_pbuf(INIT_LS_ENTRY_NUM)) == NULL) {
        err = ERROR;
        mgoto(clean_up);
    }

    if ((b = init_obuf(INIT_BUF_SIZE)) == NULL) {
        err = ERROR;
        mgoto(clean_up);
    }

    if (walk_dir_inner(dir, 0, lsi, &add_fn)) {
        err = ERROR;
        mgoto(clean_up);
    }

    /* Sort the results */
    qsort((char *) lsi->d->a, lsi->d->i, sizeof(const char *), order_func);

    qsort((char *) lsi->f->a, lsi->f->i, sizeof(const char *), order_func);

    for (j = 0; j < lsi->d->i; ++j) {
        if (put_str(b, *(lsi->d->a + j))) {
            err = ERROR;
            mgoto(clean_up);
        }

        if (put_ch(b, '\n')) {
            err = ERROR;
            mgoto(clean_up);
        }
    }

    if (put_str(b, "----------\n")) {
        err = ERROR;
        mgoto(clean_up);
    }

    for (j = 0; j < lsi->f->i; ++j) {
        if (put_str(b, *(lsi->f->a + j))) {
            err = ERROR;
            mgoto(clean_up);
        }

        if (put_ch(b, '\n')) {
            err = ERROR;
            mgoto(clean_up);
        }
    }

    if ((t = obuf_to_str(&b)) == NULL) {
        err = ERROR;
        mgoto(clean_up);
    }

    err = 0;
  clean_up:
    if (lsi != NULL) {
        if (lsi->d != NULL) {
            for (j = 0; j < lsi->d->i; ++j)
                free(*(lsi->d->a + j));

            free_pbuf(lsi->d);
        }
        if (lsi->f != NULL) {
            for (j = 0; j < lsi->f->i; ++j)
                free(*(lsi->f->a + j));

            free_pbuf(lsi->f);
        }
        free(lsi);
    }

    free_obuf(b);

    if (err) {
        free(t);
        mreturn(NULL);
    } else {
        return t;
    }
}

int mmap_file_ro(const char *fn, void **mem, size_t *fs)
{
    size_t s;
#ifdef _WIN32
    int ret = ERROR;
    HANDLE file_h = INVALID_HANDLE_VALUE;
    HANDLE map_h = NULL;
    LPVOID p = NULL;
#else
    int fd;
    void *p;
#endif

    if (get_file_size(fn, &s))
        mreturn(ERROR);

    /* Empty file */
    if (!s) {
        *mem = NULL;
        *fs = 0;
        return 0;
    }

#ifdef _WIN32

    /* Open existing file read only */
    if ((file_h =
         CreateFile(fn, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING,
                    FILE_ATTRIBUTE_NORMAL,
                    NULL)) == INVALID_HANDLE_VALUE) {
        ret = ERROR;
        mgoto(clean_up);
    }

    if ((map_h =
         CreateFileMapping(file_h, NULL, PAGE_READONLY, 0, 0,
                           NULL)) == NULL) {
        ret = ERROR;
        mgoto(clean_up);
    }

    if ((p = MapViewOfFile(map_h, FILE_MAP_READ, 0, 0, 0)) == NULL) {
        ret = ERROR;
        mgoto(clean_up);
    }

    ret = 0;
  clean_up:
    if (file_h != INVALID_HANDLE_VALUE && !CloseHandle(file_h))
        ret = ERROR;

    if (map_h != NULL && !CloseHandle(map_h))
        ret = ERROR;

    if (ret) {
        if (p != NULL)
            UnmapViewOfFile(p);
        mreturn(ERROR);
    }

#else

    if ((fd = open(fn, O_RDONLY)) == -1)
        mreturn(ERROR);

    if ((p = mmap(NULL, s, PROT_READ, MAP_PRIVATE, fd, 0)) == MAP_FAILED) {
        close(fd);
        mreturn(ERROR);
    }

    if (close(fd)) {
        munmap(p, s);
        mreturn(ERROR);
    }

#endif

    *mem = p;
    *fs = s;
    return 0;
}

int un_mmap(void *p, size_t s)
{
    if (p == NULL || !s)
        return 0;

#ifdef _WIN32
    if (!UnmapViewOfFile(p))
        mreturn(ERROR);
#else
    if (munmap(p, s))
        mreturn(ERROR);
#endif
    return 0;
}

int make_temp(const char *template, char **temp_fn)
{
#ifdef _WIN32
    int pid;
#else
    pid_t pid;
#endif
    const char *p, *suffix_start = NULL;
    char ch, *fn = NULL;
    char num[NUM_BUF_SIZE];
    size_t prefix_len, pid_len, s;
    int r;

    p = template;

    while ((ch = *p)) {
        if (ch == 'X') {
            if (suffix_start == NULL)
                suffix_start = p;
        } else {
            suffix_start = NULL;
        }
        ++p;
    }

    if (suffix_start == NULL) {
        fprintf(stderr, "%s:%d: Syntax error: "
                "make_temp: Invalid template, no X suffix\n", __FILE__,
                __LINE__);
        return SYNTAX_ERROR;
    }

    prefix_len = suffix_start - template;

    pid = getpid();

    r = snprintf(num, NUM_BUF_SIZE, "%d", (int) pid);
    if (r < 0 || r >= NUM_BUF_SIZE)
        mreturn(ERROR);

    pid_len = strlen(num);

    if (aof(prefix_len, pid_len, SIZE_MAX))
        mreturn(ERROR);

    s = prefix_len + pid_len;

    if (aof(s, 1, SIZE_MAX))
        mreturn(ERROR);
    ++s;

    if ((fn = calloc(s, 1)) == NULL)
        mreturn(ERROR);

    memcpy(fn, template, prefix_len);
    memcpy(fn + prefix_len, num, pid_len);
    *(fn + prefix_len + pid_len) = '\0';

    *temp_fn = fn;
    return 0;
}

#ifdef _WIN32
static int rand_alnum(char *ch)
{
    unsigned int x;
    char *y, c;
    size_t i;
    int try = 100;

    while (1) {
        if (!try) {
            fprintf(stderr, "%s:%d: Error\n", __FILE__, __LINE__);
            break;
        }

        if (rand_s(&x))
            return ERROR;

        y = (char *) &x;

        for (i = 0; i < sizeof(unsigned int); ++i) {
            c = y[i] & 0x7F;    /* Clear upper bit, as not used in ASCII */
            if (isalnum(c)) {
                *ch = c;
                return 0;
            }
        }
        --try;
    }

    return ERROR;
}

#endif

int make_stemp(const char *template, char **temp_fn)
{
    char *template_copy = NULL;
#ifdef _WIN32
    char *p, *suffix_start = NULL, ch, *q;
    HANDLE h;
    size_t try = 10;
#else
    int fd;
#endif

    if ((template_copy = strdup(template)) == NULL)
        mreturn(ERROR);

#ifdef _WIN32
    p = template_copy;

    while ((ch = *p)) {
        if (ch == 'X') {
            if (suffix_start == NULL)
                suffix_start = p;
        } else {
            suffix_start = NULL;
        }
        ++p;
    }

    if (suffix_start == NULL) {
        fprintf(stderr, "%s:%d: Syntax error: "
                "make_temp: Invalid template, no X suffix\n", __FILE__,
                __LINE__);
        free(template_copy);
        return SYNTAX_ERROR;
    }

    while (1) {
        if (!try) {
            free(template_copy);
            mreturn(ERROR);
        }

        /* Overwrite suffix X chars with random chars */
        q = suffix_start;
        while (*q) {
            if (rand_alnum(&ch)) {
                free(template_copy);
                mreturn(ERROR);
            }
            *q = ch;
            ++q;
        }

        h = CreateFile(template_copy, GENERIC_WRITE, 0, NULL, CREATE_NEW,
                       FILE_ATTRIBUTE_NORMAL, NULL);

        if (h == INVALID_HANDLE_VALUE) {
            if (GetLastError() != ERROR_FILE_EXISTS) {
                free(template_copy);
                mreturn(ERROR);
            }
        } else {
            if (!CloseHandle(h)) {
                free(template_copy);
                mreturn(ERROR);
            }
            break;
        }

        --try;
    }

#else
    if ((fd = mkstemp(template_copy)) == -1) {
        free(template_copy);
        mreturn(ERROR);
    }

    if (close(fd)) {
        free(template_copy);
        mreturn(ERROR);
    }
#endif

    *temp_fn = template_copy;
    return 0;
}
