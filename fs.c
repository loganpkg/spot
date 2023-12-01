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

#ifdef __linux__
/* For: strdup */
#define _XOPEN_SOURCE 500
/* For: DT_DIR and DT_UNKNOWN */
#define _DEFAULT_SOURCE
#endif


#ifdef _WIN32
#include <Windows.h>
#include <direct.h>
#else
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#endif

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "fs.h"
#include "debug.h"
#include "gen.h"
#include "num.h"


#define SET_DIR(u) ((u) |= 1)
#define SET_SLINK(u) ((u) |= 1 << 1)
#define SET_DOTDIR(u) ((u) |= 1 << 2)


FILE *fopen_w(const char *fn)
{
    /* Creates missing directories and opens a file for writing */
    FILE *fp;
    char *p, *q, ch;

    errno = 0;
    fp = fopen(fn, "wb");
    if (fp != NULL)
        mreturn(fp);

    if (fp == NULL && errno != ENOENT)
        mreturn(NULL);

    /* Try to make missing directories */
    if ((p = strdup(fn)) == NULL)
        mreturn(NULL);

    q = p;

    while ((ch = *q) != '\0') {
        if (ch == '/' || ch == '\\') {
            *q = '\0';
            errno = 0;
            if (mkdir(p) && errno != EEXIST) {
                free(p);
                mreturn(NULL);
            }
            *q = ch;
        }
        ++q;
    }

    free(p);

    mreturn(fopen(fn, "wb"));
}


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


int walk_dir_inner(const char *dir, int (*func_p) (const char *,
                                                   unsigned char))
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


        if (IS_DIR(type) && !IS_SLINK(type) && !IS_DOTDIR(type))
            if (walk_dir_inner(path, func_p))   /* Recurse */
                mgoto(clean_up);

        if ((*func_p) (path, type))
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

int walk_dir(const char *dir, int (*func_p) (const char *, unsigned char))
{
    /* Executes the function on dir itself too */
    unsigned char type;

    if (walk_dir_inner(dir, func_p))
        mreturn(1);

    /* Process outer directory */
    type = 0;
    SET_DIR(type);              /* Not sure if outer dir is a symbolic link */
    if (!strcmp(dir, ".") || !strcmp(dir, ".."))
        SET_DOTDIR(type);

    if ((*func_p) (dir, type))
        mreturn(1);

    mreturn(0);
}

static int rm_path(const char *path, unsigned char type)
{
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

    mreturn(walk_dir(path, &rm_path));
}
