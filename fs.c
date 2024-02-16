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

/* File system related module */

#include "toucanlib.h"


#define INIT_LS_ENTRY_NUM 512
#define INIT_BUF_SIZE 1024


#define SET_DIR(u) ((u) |= 1)
#define SET_SLINK(u) ((u) |= 1 << 1)
#define SET_DOTDIR(u) ((u) |= 1 << 2)


struct ls_info {
    struct pbuf *d;             /* To store pointers to directory names */
    struct pbuf *f;             /* To store pointers to filenames */
};


int get_path_type(const char *path, unsigned char *type)
{
    unsigned char t = '\0';
#ifdef _WIN32
    DWORD file_attr;
#else
    struct stat st;
#endif

#ifdef _WIN32
    if ((file_attr = GetFileAttributes(path)) == INVALID_FILE_ATTRIBUTES)
        mreturn(1);

    if (file_attr & FILE_ATTRIBUTE_DIRECTORY)
        SET_DIR(t);

    if (file_attr & FILE_ATTRIBUTE_REPARSE_POINT)
        SET_SLINK(t);
#else
    if (lstat(path, &st))
        mreturn(1);

    if (S_ISDIR(st.st_mode))
        SET_DIR(t);

    if (S_ISLNK(st.st_mode))
        SET_SLINK(t);
#endif

    if (!strcmp(path, ".") || !strcmp(path, ".."))
        SET_DOTDIR(t);

    *type = t;
    mreturn(0);
}


int walk_dir_inner(const char *dir, int rec, void *info,
                   int (*func_p)(const char *, unsigned char, void *info))
{
    /* Does not execute the function on dir itself */
    int ret = 1;

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
    unsigned char type;

#ifdef _WIN32
    if ((dir_wc = concat(dir, "\\*", NULL)) == NULL)
        mgoto(clean_up);

    if ((h = FindFirstFile(dir_wc, &d)) == INVALID_HANDLE_VALUE)
        mgoto(clean_up);
#else
    if ((h = opendir(dir)) == NULL)
        mgoto(clean_up);
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
        if ((path = concat(dir, DIR_SEP_STR, fn, NULL)) == NULL)
            mgoto(clean_up);

        type = 0;
#ifdef _WIN32
        if (d.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
            SET_DIR(type);

        if (d.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT)
            SET_SLINK(type);
#else
        if (d->d_type == DT_DIR)
            SET_DIR(type);

        if (d->d_type == DT_LNK)
            SET_SLINK(type);

        if (d->d_type == DT_UNKNOWN)
            if (get_path_type(path, &type))
                mgoto(clean_up);

#endif
        if (!strcmp(fn, ".") || !strcmp(fn, ".."))
            SET_DOTDIR(type);

        if (rec && IS_DIR(type) && !IS_SLINK(type) && !IS_DOTDIR(type))
            if (walk_dir_inner(path, rec, info, func_p))        /* Recurse */
                mgoto(clean_up);

        if ((*func_p) (rec ? path : fn, type, info))
            mgoto(clean_up);

        free(path);
        path = NULL;
#ifdef _WIN32
        if (!FindNextFile(h, &d))
            break;
#endif
    }

#ifdef _WIN32
    if (GetLastError() != ERROR_NO_MORE_FILES)
        mgoto(clean_up);
#else
    if (errno)
        mgoto(clean_up);
#endif

    ret = 0;

  clean_up:

#ifdef _WIN32
    if (h != INVALID_HANDLE_VALUE && !FindClose(h))
        ret = 1;

    if (dir_wc != NULL)
        free(dir_wc);
#else
    if (closedir(h))
        ret = 1;
#endif
    if (path != NULL)
        free(path);

    mreturn(ret);
}

int walk_dir(const char *dir, int rec, void *info,
             int (*func_p)(const char *, unsigned char, void *info))
{
    /* Executes the function on dir itself too */
    unsigned char type;

    if (walk_dir_inner(dir, rec, info, func_p))
        mreturn(1);

    /* Process outer directory */
    type = 0;
    SET_DIR(type);              /* Not sure if outer dir is a symbolic link */
    if (!strcmp(dir, ".") || !strcmp(dir, ".."))
        SET_DOTDIR(type);

    if ((*func_p) (dir, type, info))
        mreturn(1);

    mreturn(0);
}

static int rm_path(const char *path, unsigned char type, void *info)
{
    if (info != NULL)           /* Not used in this function */
        mreturn(1);

    if (!IS_DOTDIR(type)) {
        if (IS_DIR(type)) {
            if (rmdir(path))
                mreturn(1);
        } else {
            if (unlink(path))
                mreturn(1);
        }
    }
    mreturn(0);
}

int rec_rm(const char *path)
{
    errno = 0;
    if (unlink(path) == 0)      /* Success */
        mreturn(0);

    if (errno == ENOENT)
        mreturn(0);

    mreturn(walk_dir(path, 1, NULL, &rm_path));
}

static int add_fn(const char *fn, unsigned char type, void *info)
{
    struct ls_info *lsi;
    char *fn_copy;
    lsi = (struct ls_info *) info;

    if ((fn_copy = strdup(fn)) == NULL)
        mreturn(1);

    if (IS_DIR(type)) {
        if (add_p(lsi->d, fn_copy)) {
            free(fn_copy);
            mreturn(1);
        }
    } else {
        if (add_p(lsi->f, fn_copy)) {
            free(fn_copy);
            mreturn(1);
        }
    }

    mreturn(0);
}

static int order_func(const void *a, const void *b)
{
    return strcmp(*(const char **) a, *(const char **) b);
}

char *ls_dir(const char *dir)
{
    int err = 1;
    struct ls_info *lsi = NULL;
    struct obuf *b = NULL;
    char *t = NULL;
    size_t j;

    if ((lsi = calloc(1, sizeof(struct ls_info))) == NULL)
        mgoto(clean_up);

    if ((lsi->d = init_pbuf(INIT_LS_ENTRY_NUM)) == NULL)
        mgoto(clean_up);

    if ((lsi->f = init_pbuf(INIT_LS_ENTRY_NUM)) == NULL)
        mgoto(clean_up);

    if ((b = init_obuf(INIT_BUF_SIZE)) == NULL)
        mgoto(clean_up);

    if (walk_dir_inner(dir, 0, lsi, &add_fn))
        mgoto(clean_up);

    /* Sort the results */
    qsort((char *) lsi->d->a, lsi->d->i, sizeof(const char *), order_func);

    qsort((char *) lsi->f->a, lsi->f->i, sizeof(const char *), order_func);

    for (j = 0; j < lsi->d->i; ++j) {
        if (put_str(b, *(lsi->d->a + j)))
            mgoto(clean_up);

        if (put_ch(b, '\n'))
            mgoto(clean_up);
    }

    if (put_str(b, "----------\n"))
        mgoto(clean_up);

    for (j = 0; j < lsi->f->i; ++j) {
        if (put_str(b, *(lsi->f->a + j)))
            mgoto(clean_up);

        if (put_ch(b, '\n'))
            mgoto(clean_up);
    }

    if ((t = obuf_to_str(&b)) == NULL)
        mgoto(clean_up);

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
        mreturn(t);
    }
}
